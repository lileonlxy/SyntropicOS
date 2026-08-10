/**
 * @file test_wasm.c
 * @brief Unit tests for 32-bit WebAssembly (Wasm MVP) cooperative interpreter.
 */

#include "syntropic/vm/syn_wasm.h"
#include "unity/unity.h"

#include <string.h>

static uint8_t g_linear_mem[4096];
static SYN_WASM_Module g_wasm_mod;
static SYN_WASM_Context g_wasm_ctx;

/* Sample valid minimal Wasm module with an export 'add' (i32.add) */
/* Binary generated from WebAssembly text:
 * (module
 *   (func (export "add") (param i32 i32) (result i32)
 *     local.get 0
 *     local.get 1
 *     i32.add)
 * )
 */
static const uint8_t g_wasm_add_binary[] = {
    0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00,       /* magic & version */
    0x01, 0x07, 0x01, 0x60, 0x02, 0x7f, 0x7f, 0x01, 0x7f, /* Type: (i32, i32) -> i32 */
    0x03, 0x02, 0x01, 0x00,                               /* Func 0: type 0 */
    0x07, 0x07, 0x01, 0x03, 0x61, 0x64, 0x64, 0x00, 0x00, /* Export "add" -> func 0 */
    0x0a, 0x09, 0x01, 0x07, 0x00, 0x20, 0x00, 0x20, 0x01,
    0x6a, 0x0b /* Code: local.get 0, local.get 1, i32.add, end */
};

/* Sample Wasm module with loop counting down from N */
/*
 * (module
 *   (func (export "loop_count") (param i32) (result i32)
 *     (local i32)
 *     local.get 0
 *     local.set 1
 *     block
 *       loop
 *         local.get 1
 *         i32.eqz
 *         br_if 1
 *         local.get 1
 *         i32.const 1
 *         i32.sub
 *         local.set 1
 *         br 0
 *       end
 *     end
 *     local.get 1)
 * )
 */
static const uint8_t g_wasm_loop_binary[] = {
    0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x06, 0x01, 0x60, 0x01, 0x7f,
    0x01, 0x7f, 0x03, 0x02, 0x01, 0x00, 0x07, 0x0e, 0x01, 0x0a, 0x6c, 0x6f, 0x6f, 0x70,
    0x5f, 0x63, 0x6f, 0x75, 0x6e, 0x74, 0x00, 0x00, 0x0a, 0x20, 0x01, 0x1e, 0x01, 0x01,
    0x7f, 0x20, 0x00, 0x21, 0x01, 0x02, 0x40, 0x03, 0x40, 0x20, 0x01, 0x45, 0x0d, 0x01,
    0x20, 0x01, 0x41, 0x01, 0x6b, 0x21, 0x01, 0x0c, 0x00, 0x0b, 0x0b, 0x20, 0x01, 0x0b};

/* Host function callback counter */
static uint32_t g_host_call_count = 0;
static uint64_t mock_host_func(SYN_WASM_Context *ctx, const uint64_t *args, uint8_t argc)
{
    (void)ctx;
    (void)args;
    (void)argc;
    g_host_call_count++;
    return 42;
}

static void test_wasm_load_null_and_invalid(void)
{
    TEST_ASSERT_FALSE(syn_wasm_module_load(NULL, g_wasm_add_binary, sizeof(g_wasm_add_binary)));
    TEST_ASSERT_FALSE(syn_wasm_module_load(&g_wasm_mod, NULL, sizeof(g_wasm_add_binary)));
    TEST_ASSERT_FALSE(syn_wasm_module_load(&g_wasm_mod, g_wasm_add_binary, 4));

    uint8_t bad_hdr[8] = {0x00, 0x61, 0x73, 0x6d, 0x02, 0x00, 0x00, 0x00};
    TEST_ASSERT_FALSE(syn_wasm_module_load(&g_wasm_mod, bad_hdr, sizeof(bad_hdr)));
}

static void test_wasm_load_and_export_lookup(void)
{
    TEST_ASSERT_TRUE(
        syn_wasm_module_load(&g_wasm_mod, g_wasm_add_binary, sizeof(g_wasm_add_binary)));
    TEST_ASSERT_EQUAL_UINT16(1, g_wasm_mod.export_count);

    int32_t add_idx = syn_wasm_find_export(&g_wasm_mod, "add");
    TEST_ASSERT_EQUAL_INT32(0, add_idx);

    int32_t missing_idx = syn_wasm_find_export(&g_wasm_mod, "nonexistent");
    TEST_ASSERT_EQUAL_INT32(-1, missing_idx);
}

static void test_wasm_init_and_execution(void)
{
    TEST_ASSERT_TRUE(
        syn_wasm_module_load(&g_wasm_mod, g_wasm_add_binary, sizeof(g_wasm_add_binary)));
    TEST_ASSERT_TRUE(syn_wasm_init(&g_wasm_ctx, &g_wasm_mod, g_linear_mem, sizeof(g_linear_mem)));

    TEST_ASSERT_TRUE(syn_wasm_call(&g_wasm_ctx, 0));

    /* Populate arguments into local variables 0 and 1 */
    g_wasm_ctx.locals[0] = 15;
    g_wasm_ctx.locals[1] = 27;

    SYN_WASM_Status st = syn_wasm_step(&g_wasm_ctx, 100);
    TEST_ASSERT_EQUAL(SYN_WASM_HALTED, st);
    TEST_ASSERT_EQUAL_UINT32(42, syn_wasm_result(&g_wasm_ctx));
}

static void test_wasm_loop_and_yielding(void)
{
    TEST_ASSERT_TRUE(
        syn_wasm_module_load(&g_wasm_mod, g_wasm_loop_binary, sizeof(g_wasm_loop_binary)));
    TEST_ASSERT_TRUE(syn_wasm_init(&g_wasm_ctx, &g_wasm_mod, g_linear_mem, sizeof(g_linear_mem)));

    TEST_ASSERT_TRUE(syn_wasm_call(&g_wasm_ctx, 0));
    g_wasm_ctx.locals[0] = 10; /* Loop 10 iterations */

    /* Step with small instruction budget (5 opcodes) -> Expect YIELDED */
    SYN_WASM_Status st = syn_wasm_step(&g_wasm_ctx, 5);
    TEST_ASSERT_EQUAL(SYN_WASM_YIELDED, st);

    /* Finish remaining execution */
    st = syn_wasm_step(&g_wasm_ctx, 1000);
    TEST_ASSERT_EQUAL(SYN_WASM_HALTED, st);
    TEST_ASSERT_EQUAL_UINT32(0, syn_wasm_result(&g_wasm_ctx));
}

static void test_wasm_host_function_registration(void)
{
    g_host_call_count = 0;
    TEST_ASSERT_TRUE(syn_wasm_init(&g_wasm_ctx, &g_wasm_mod, g_linear_mem, sizeof(g_linear_mem)));
    TEST_ASSERT_TRUE(syn_wasm_register_host(&g_wasm_ctx, 0, mock_host_func));

    /* Manually invoke host func registration slot 0 */
    uint32_t ret = g_wasm_ctx.host_funcs[0](&g_wasm_ctx, NULL, 0);
    TEST_ASSERT_EQUAL_UINT32(42, ret);
    TEST_ASSERT_EQUAL_UINT32(1, g_host_call_count);
}

static void test_wasm_float_and_bulk_memory(void)
{
    TEST_ASSERT_TRUE(syn_wasm_init(&g_wasm_ctx, &g_wasm_mod, g_linear_mem, sizeof(g_linear_mem)));
    memset(g_linear_mem, 0, sizeof(g_linear_mem));
    memcpy(&g_linear_mem[100], "WORLD", 5);

    /* Test 0xFC subop 10 memory.copy in linear_mem */
    g_wasm_ctx.sp = 0;
    g_wasm_ctx.stack[g_wasm_ctx.sp++] = 0;   /* dst = 0 */
    g_wasm_ctx.stack[g_wasm_ctx.sp++] = 100; /* src = 100 */
    g_wasm_ctx.stack[g_wasm_ctx.sp++] = 5;   /* len = 5 */

    static const uint8_t code_copy[] = {0xFC, 0x0A, 0x00, 0x00, 0x0B};
    SYN_WASM_Module mod_copy;
    memset(&mod_copy, 0, sizeof(mod_copy));
    mod_copy.bytes = code_copy;
    mod_copy.size = sizeof(code_copy);
    mod_copy.func_count = 1;
    mod_copy.funcs[0].code_offset = 0;
    mod_copy.funcs[0].code_size = sizeof(code_copy);

    g_wasm_ctx.module = &mod_copy;
    g_wasm_ctx.call_depth = 1;
    g_wasm_ctx.call_stack[0].func_idx = 0;
    g_wasm_ctx.call_stack[0].return_pc = 0;
    g_wasm_ctx.pc = 0;
    g_wasm_ctx.status = SYN_WASM_OK;

    TEST_ASSERT_EQUAL(SYN_WASM_HALTED, syn_wasm_step(&g_wasm_ctx, 100));
    TEST_ASSERT_EQUAL_MEMORY("WORLD", g_linear_mem, 5);
}

static void test_wasm_fixed_point_float_ops(void)
{
    TEST_ASSERT_TRUE(syn_wasm_init(&g_wasm_ctx, &g_wasm_mod, g_linear_mem, sizeof(g_linear_mem)));

    /* Test F32 opcodes in bytecode: f32.const 3.0f, f32.const 4.0f, f32.add */
    static const uint8_t code_f32_add[] = {0x43, 0x00, 0x00, 0x40, 0x40, 0x43,
                                           0x00, 0x00, 0x80, 0x40, 0x92, 0x0B};
    SYN_WASM_Module mod_add;
    memset(&mod_add, 0, sizeof(mod_add));
    mod_add.bytes = code_f32_add;
    mod_add.size = sizeof(code_f32_add);
    mod_add.func_count = 1;
    mod_add.funcs[0].code_offset = 0;
    mod_add.funcs[0].code_size = sizeof(code_f32_add);

    g_wasm_ctx.module = &mod_add;
    g_wasm_ctx.call_depth = 1;
    g_wasm_ctx.call_stack[0].func_idx = 0;
    g_wasm_ctx.call_stack[0].return_pc = 0;
    g_wasm_ctx.pc = 0;
    g_wasm_ctx.sp = 0;
    g_wasm_ctx.status = SYN_WASM_OK;

    TEST_ASSERT_EQUAL(SYN_WASM_HALTED, syn_wasm_step(&g_wasm_ctx, 100));
#if defined(SYN_WASM_USE_FIXED) && SYN_WASM_USE_FIXED
    /* 3.0 + 4.0 = 7.0 in Q16.16 (7 * 65536 = 458752) */
    TEST_ASSERT_EQUAL_UINT32(458752, syn_wasm_result(&g_wasm_ctx));
#else
    /* 3.0f + 4.0f = 7.0f in IEEE 754 (0x40e00000) */
    TEST_ASSERT_EQUAL_HEX32(0x40e00000, (uint32_t)syn_wasm_result(&g_wasm_ctx));
#endif
}

#include "wasm/test_arithmetic.wasm.h"
#include "wasm/test_control_flow.wasm.h"
#include "wasm/test_conversions.wasm.h"
#include "wasm/test_embedded.wasm.h"
#include "wasm/test_functions.wasm.h"
#include "wasm/test_host_api.wasm.h"
#include "wasm/test_memory.wasm.h"
#include "wasm/test_recursion.wasm.h"
#include "wasm/test_structs.wasm.h"
#include "wasm/test_traps.wasm.h"

static uint32_t g_host_log_called = 0;
static uint32_t g_host_log_code = 0;
static uint32_t g_host_log_val = 0;

static uint64_t mock_host_log(SYN_WASM_Context *ctx, const uint64_t *args, uint8_t argc)
{
    (void)ctx;
    (void)argc;
    g_host_log_called++;
    g_host_log_code = (uint32_t)args[0];
    g_host_log_val = (uint32_t)args[1];
    return g_host_log_code + g_host_log_val;
}

static uint32_t run_wasm_fixture(const uint8_t *bytes, uint32_t size)
{
    TEST_ASSERT_TRUE(syn_wasm_module_load(&g_wasm_mod, bytes, size));
    TEST_ASSERT_TRUE(syn_wasm_init(&g_wasm_ctx, &g_wasm_mod, g_linear_mem, sizeof(g_linear_mem)));

    int32_t fn = syn_wasm_find_export(&g_wasm_mod, "run_tests");
    TEST_ASSERT_TRUE(fn >= 0);

    TEST_ASSERT_TRUE(syn_wasm_call(&g_wasm_ctx, (uint16_t)fn));
    SYN_WASM_Status st = syn_wasm_step(&g_wasm_ctx, 10000);
    TEST_ASSERT_EQUAL(SYN_WASM_HALTED, st);
    return (uint32_t)syn_wasm_result(&g_wasm_ctx);
}

static void test_wasm_fixture_arithmetic(void)
{
    uint32_t res = run_wasm_fixture(test_arithmetic_wasm, test_arithmetic_wasm_len);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0, res, "WASM test_arithmetic failed at case #");
}

static void test_wasm_fixture_memory(void)
{
    uint32_t res = run_wasm_fixture(test_memory_wasm, test_memory_wasm_len);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0, res, "WASM test_memory failed at case #");
}

static void test_wasm_fixture_functions(void)
{
    uint32_t res = run_wasm_fixture(test_functions_wasm, test_functions_wasm_len);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0, res, "WASM test_functions failed at case #");
}

static void test_wasm_fixture_structs(void)
{
    uint32_t res = run_wasm_fixture(test_structs_wasm, test_structs_wasm_len);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0, res, "WASM test_structs failed at case #");
}

static void test_wasm_fixture_control_flow(void)
{
    uint32_t res = run_wasm_fixture(test_control_flow_wasm, test_control_flow_wasm_len);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0, res, "WASM test_control_flow failed at case #");
}

static void test_wasm_fixture_conversions(void)
{
    uint32_t res = run_wasm_fixture(test_conversions_wasm, test_conversions_wasm_len);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0, res, "WASM test_conversions failed at case #");
}

static void test_wasm_fixture_recursion(void)
{
    uint32_t res = run_wasm_fixture(test_recursion_wasm, test_recursion_wasm_len);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0, res, "WASM test_recursion failed at case #");
}

static void test_wasm_fixture_embedded(void)
{
    uint32_t res = run_wasm_fixture(test_embedded_wasm, test_embedded_wasm_len);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0, res, "WASM test_embedded failed at case #");
}

static void test_wasm_fixture_host_api(void)
{
    g_host_log_called = 0;
    TEST_ASSERT_TRUE(syn_wasm_module_load(&g_wasm_mod, test_host_api_wasm, test_host_api_wasm_len));
    TEST_ASSERT_TRUE(syn_wasm_init(&g_wasm_ctx, &g_wasm_mod, g_linear_mem, sizeof(g_linear_mem)));
    TEST_ASSERT_TRUE(syn_wasm_register_host(&g_wasm_ctx, 0, mock_host_log));

    int32_t fn = syn_wasm_find_export(&g_wasm_mod, "run_tests");
    TEST_ASSERT_TRUE(fn >= 0);

    TEST_ASSERT_TRUE(syn_wasm_call(&g_wasm_ctx, (uint16_t)fn));
    SYN_WASM_Status st = syn_wasm_step(&g_wasm_ctx, 10000);
    TEST_ASSERT_EQUAL(SYN_WASM_HALTED, st);
    TEST_ASSERT_EQUAL_UINT32(0, syn_wasm_result(&g_wasm_ctx));
    TEST_ASSERT_EQUAL_UINT32(1, g_host_log_called);
    TEST_ASSERT_EQUAL_UINT32(42, g_host_log_code);
    TEST_ASSERT_EQUAL_UINT32(150, g_host_log_val);
}

static void test_wasm_fixture_traps(void)
{
    TEST_ASSERT_TRUE(syn_wasm_module_load(&g_wasm_mod, test_traps_wasm, test_traps_wasm_len));
    TEST_ASSERT_TRUE(syn_wasm_init(&g_wasm_ctx, &g_wasm_mod, g_linear_mem, sizeof(g_linear_mem)));

    /* 1. Divide by zero trap */
    int32_t fn_div = syn_wasm_find_export(&g_wasm_mod, "trigger_div_zero");
    TEST_ASSERT_TRUE(fn_div >= 0);
    TEST_ASSERT_TRUE(syn_wasm_call(&g_wasm_ctx, (uint16_t)fn_div));
    TEST_ASSERT_EQUAL(SYN_WASM_TRAP_DIV_ZERO, syn_wasm_step(&g_wasm_ctx, 1000));

    /* 2. Out of bounds memory access trap */
    TEST_ASSERT_TRUE(syn_wasm_init(&g_wasm_ctx, &g_wasm_mod, g_linear_mem, sizeof(g_linear_mem)));
    int32_t fn_oob = syn_wasm_find_export(&g_wasm_mod, "trigger_oob_read");
    TEST_ASSERT_TRUE(fn_oob >= 0);
    TEST_ASSERT_TRUE(syn_wasm_call(&g_wasm_ctx, (uint16_t)fn_oob));
    TEST_ASSERT_EQUAL(SYN_WASM_TRAP_OUT_OF_BOUNDS, syn_wasm_step(&g_wasm_ctx, 1000));

    /* 3. Direct i32.div_s signed overflow trap (INT32_MIN / -1) */
    static const uint8_t code_div_overflow[] = {
        0x41, 0x80, 0x80, 0x80, 0x80, 0x78, /* i32.const -2147483648 (INT32_MIN) */
        0x41, 0x7F,                         /* i32.const -1 */
        0x6D,                               /* i32.div_s */
        0x0B                                /* end */
    };
    SYN_WASM_Module mod_div_ovf;
    memset(&mod_div_ovf, 0, sizeof(mod_div_ovf));
    mod_div_ovf.bytes = code_div_overflow;
    mod_div_ovf.size = sizeof(code_div_overflow);
    mod_div_ovf.func_count = 1;
    mod_div_ovf.funcs[0].code_offset = 0;
    mod_div_ovf.funcs[0].code_size = sizeof(code_div_overflow);

    TEST_ASSERT_TRUE(syn_wasm_init(&g_wasm_ctx, &mod_div_ovf, g_linear_mem, sizeof(g_linear_mem)));
    g_wasm_ctx.call_depth = 1;
    g_wasm_ctx.call_stack[0].func_idx = 0;
    g_wasm_ctx.call_stack[0].return_pc = 0;
    g_wasm_ctx.pc = 0;
    g_wasm_ctx.sp = 0;
    g_wasm_ctx.status = SYN_WASM_OK;
    TEST_ASSERT_EQUAL(SYN_WASM_TRAP_DIV_ZERO, syn_wasm_step(&g_wasm_ctx, 100));
}

static void test_wasm_fixture_yield_resume(void)
{
    TEST_ASSERT_TRUE(
        syn_wasm_module_load(&g_wasm_mod, test_control_flow_wasm, test_control_flow_wasm_len));
    TEST_ASSERT_TRUE(syn_wasm_init(&g_wasm_ctx, &g_wasm_mod, g_linear_mem, sizeof(g_linear_mem)));

    int32_t fn = syn_wasm_find_export(&g_wasm_mod, "run_tests");
    TEST_ASSERT_TRUE(fn >= 0);

    TEST_ASSERT_TRUE(syn_wasm_call(&g_wasm_ctx, (uint16_t)fn));

    /* Step 3 instructions -> Expect YIELDED */
    SYN_WASM_Status st = syn_wasm_step(&g_wasm_ctx, 3);
    TEST_ASSERT_EQUAL(SYN_WASM_YIELDED, st);

    /* Resume execution -> Expect HALTED */
    st = syn_wasm_step(&g_wasm_ctx, 10000);
    TEST_ASSERT_EQUAL(SYN_WASM_HALTED, st);
    TEST_ASSERT_EQUAL_UINT32(0, syn_wasm_result(&g_wasm_ctx));
}

static void test_wasm_extended_traps_and_nulls(void)
{
    SYN_WASM_Module mod;
    SYN_WASM_Context ctx;
    uint8_t mem[1024];

    /* Null & boundary API calls */
    TEST_ASSERT_FALSE(syn_wasm_module_load(NULL, g_wasm_add_binary, sizeof(g_wasm_add_binary)));
    TEST_ASSERT_FALSE(syn_wasm_module_load(&mod, NULL, sizeof(g_wasm_add_binary)));
    TEST_ASSERT_FALSE(syn_wasm_module_load(&mod, g_wasm_add_binary, 0));
    TEST_ASSERT_FALSE(syn_wasm_init(NULL, &g_wasm_mod, mem, sizeof(mem)));
    TEST_ASSERT_FALSE(syn_wasm_init(&ctx, NULL, mem, sizeof(mem)));
    TEST_ASSERT_TRUE(syn_wasm_init(&ctx, &g_wasm_mod, NULL, 0));
    TEST_ASSERT_FALSE(syn_wasm_register_host(NULL, 0, mock_host_log));
    TEST_ASSERT_FALSE(syn_wasm_register_host(&ctx, 999, mock_host_log));
    TEST_ASSERT_EQUAL(-1, syn_wasm_find_export(NULL, "add"));
    TEST_ASSERT_EQUAL(-1, syn_wasm_find_export(&g_wasm_mod, NULL));
    TEST_ASSERT_EQUAL(-1, syn_wasm_find_export(&g_wasm_mod, "non_existent"));
    TEST_ASSERT_FALSE(syn_wasm_call(NULL, 0));
    TEST_ASSERT_FALSE(syn_wasm_call(&ctx, 9999));
    TEST_ASSERT_EQUAL(SYN_WASM_TRAP_INVALID_MODULE, syn_wasm_step(NULL, 10));
    TEST_ASSERT_EQUAL_UINT64(0, syn_wasm_result(NULL));

    /* Invalid Magic Header check */
    uint8_t bad_magic[8] = {0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00};
    TEST_ASSERT_FALSE(syn_wasm_module_load(&mod, bad_magic, sizeof(bad_magic)));

    /* Load valid minimal module and test step traps */
    SYN_WASM_Module test_mod;
    TEST_ASSERT_TRUE(syn_wasm_module_load(&test_mod, g_wasm_add_binary, sizeof(g_wasm_add_binary)));
    TEST_ASSERT_TRUE(syn_wasm_init(&ctx, &test_mod, mem, sizeof(mem)));
    TEST_ASSERT_TRUE(syn_wasm_call(&ctx, 0));
    syn_wasm_step(&ctx, 10);

    /* Test step on non-OK status returns current status immediately */
    ctx.status = SYN_WASM_HALTED;
    TEST_ASSERT_EQUAL(SYN_WASM_HALTED, syn_wasm_step(&ctx, 10));

    /* Test register_host at max host func index boundary */
    TEST_ASSERT_TRUE(syn_wasm_register_host(&ctx, SYN_WASM_MAX_HOST_FUNCS - 1, mock_host_func));
}

static void test_wasm_native_f64_ops(void)
{
    TEST_ASSERT_TRUE(syn_wasm_init(&g_wasm_ctx, &g_wasm_mod, g_linear_mem, sizeof(g_linear_mem)));

    /* Bytecode: i64.const 42, f64.const 2.0, f64.abs, end */
    double v1 = -2.0;
    uint8_t code_f64[30];
    code_f64[0] = 0x42; /* OP_I64_CONST */
    code_f64[1] = 42;   /* LEB128 42 */
    code_f64[2] = 0x44; /* OP_F64_CONST */
    memcpy(&code_f64[3], &v1, 8);
    code_f64[11] = 0x99; /* OP_F64_ABS */
    code_f64[12] = 0x0B; /* OP_END */

    SYN_WASM_Module mod_f64;
    memset(&mod_f64, 0, sizeof(mod_f64));
    mod_f64.bytes = code_f64;
    mod_f64.size = 13;
    mod_f64.func_count = 1;
    mod_f64.funcs[0].code_offset = 0;
    mod_f64.funcs[0].code_size = 13;

    g_wasm_ctx.module = &mod_f64;
    g_wasm_ctx.call_depth = 1;
    g_wasm_ctx.call_stack[0].func_idx = 0;
    g_wasm_ctx.call_stack[0].return_pc = 0;
    g_wasm_ctx.pc = 0;
    g_wasm_ctx.sp = 0;
    g_wasm_ctx.status = SYN_WASM_OK;

    TEST_ASSERT_EQUAL(SYN_WASM_HALTED, syn_wasm_step(&g_wasm_ctx, 100));
}

static void test_wasm_opcodes_branch_coverage(void)
{
    TEST_ASSERT_TRUE(syn_wasm_init(&g_wasm_ctx, &g_wasm_mod, g_linear_mem, sizeof(g_linear_mem)));

    /* Bytecode: i32.const 10, i32.const 20, i32.const 0, select, end (selects 2nd operand 20 when
     * cond=0) */
    static const uint8_t code_select_false[] = {0x41, 0x0A, 0x41, 0x14, 0x41, 0x00, 0x1B, 0x0B};
    SYN_WASM_Module mod_select;
    memset(&mod_select, 0, sizeof(mod_select));
    mod_select.bytes = code_select_false;
    mod_select.size = sizeof(code_select_false);
    mod_select.func_count = 1;
    mod_select.funcs[0].code_offset = 0;
    mod_select.funcs[0].code_size = sizeof(code_select_false);

    g_wasm_ctx.module = &mod_select;
    g_wasm_ctx.call_depth = 1;
    g_wasm_ctx.call_stack[0].func_idx = 0;
    g_wasm_ctx.call_stack[0].return_pc = 0;
    g_wasm_ctx.pc = 0;
    g_wasm_ctx.sp = 0;
    g_wasm_ctx.status = SYN_WASM_OK;

    TEST_ASSERT_EQUAL(SYN_WASM_HALTED, syn_wasm_step(&g_wasm_ctx, 100));
    TEST_ASSERT_EQUAL_UINT32(20, syn_wasm_result(&g_wasm_ctx));

    /* Test local.tee and global.set/get */
    static const uint8_t code_globals[] = {0x41, 0x2A, 0x24, 0x00, 0x23, 0x00, 0x22, 0x01, 0x0B};
    SYN_WASM_Module mod_glob;
    memset(&mod_glob, 0, sizeof(mod_glob));
    mod_glob.bytes = code_globals;
    mod_glob.size = sizeof(code_globals);
    mod_glob.func_count = 1;
    mod_glob.funcs[0].code_offset = 0;
    mod_glob.funcs[0].code_size = sizeof(code_globals);

    g_wasm_ctx.module = &mod_glob;
    g_wasm_ctx.call_depth = 1;
    g_wasm_ctx.call_stack[0].func_idx = 0;
    g_wasm_ctx.call_stack[0].return_pc = 0;
    g_wasm_ctx.pc = 0;
    g_wasm_ctx.sp = 0;
    g_wasm_ctx.status = SYN_WASM_OK;

    TEST_ASSERT_EQUAL(SYN_WASM_HALTED, syn_wasm_step(&g_wasm_ctx, 100));
    TEST_ASSERT_EQUAL_UINT32(42, g_wasm_ctx.globals[0]);
    TEST_ASSERT_EQUAL_UINT32(42, g_wasm_ctx.locals[1]);

    /* Test Table (kind 1), Memory (kind 2), Global (kind 3) imports and Element section (sec 9) */
    static const uint8_t wasm_extra_sections[] = {
        0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, /* magic & version */
        0x01, 0x04, 0x01, 0x60, 0x00, 0x00,             /* Type: () -> () */
        0x02, 0x26, 0x03, /* Import Section (3 entries, 38 bytes payload) */
        0x03, 'e',  'n',  'v',  0x05, 't',  'a',  'b',  'l',  'e',
        0x01, 0x70, 0x00, /* Table import: funcref, min 0 */
        0x03, 'e',  'n',  'v',  0x03, 'm',  'e',  'm',  0x02, 0x01,
        0x01, 0x02, /* Memory import: flags=1, min 1, max 2 */
        0x03, 'e',  'n',  'v',  0x04, 'g',  'l',  'o',  'b',  0x03,
        0x7f, 0x00,                                           /* Global import: i32, const */
        0x09, 0x07, 0x01, 0x00, 0x41, 0x00, 0x0b, 0x01, 0x00, /* Element section: table 0, offset
                                                                 i32.const 0, func 0 */
    };
    SYN_WASM_Module extra_mod;
    TEST_ASSERT_TRUE(
        syn_wasm_module_load(&extra_mod, wasm_extra_sections, sizeof(wasm_extra_sections)));
    TEST_ASSERT_EQUAL_UINT16(1, extra_mod.table_element_count);
}

void run_wasm_tests(void)
{
    RUN_TEST(test_wasm_load_null_and_invalid);
    RUN_TEST(test_wasm_load_and_export_lookup);
    RUN_TEST(test_wasm_init_and_execution);
    RUN_TEST(test_wasm_loop_and_yielding);
    RUN_TEST(test_wasm_host_function_registration);
    RUN_TEST(test_wasm_float_and_bulk_memory);
    RUN_TEST(test_wasm_fixed_point_float_ops);
    RUN_TEST(test_wasm_native_f64_ops);
    RUN_TEST(test_wasm_fixture_arithmetic);
    RUN_TEST(test_wasm_fixture_memory);
    RUN_TEST(test_wasm_fixture_functions);
    RUN_TEST(test_wasm_fixture_structs);
    RUN_TEST(test_wasm_fixture_control_flow);
    RUN_TEST(test_wasm_fixture_conversions);
    RUN_TEST(test_wasm_fixture_recursion);
    RUN_TEST(test_wasm_fixture_embedded);
    RUN_TEST(test_wasm_fixture_host_api);
    RUN_TEST(test_wasm_fixture_traps);
    RUN_TEST(test_wasm_fixture_yield_resume);
    RUN_TEST(test_wasm_extended_traps_and_nulls);
    RUN_TEST(test_wasm_opcodes_branch_coverage);
}
/* touch test_wasm.c */
