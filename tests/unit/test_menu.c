/**
 * @file test_menu.c
 * @brief Unity tests for syn_menu.
 */

#include "mocks/mock_port.h"
#include "syntropic/syntropic.h"
#include "syntropic/ui/syn_menu.h"
#include "unity/unity.h"

static int mnu_render_n = 0;
static void mnu_render(const SYN_Menu *m, void *c)
{
    (void)m;
    (void)c;
    mnu_render_n++;
}
static int mnu_cb_n = 0;
static void mnu_cb(void *c)
{
    (void)c;
    mnu_cb_n++;
}

static void test_menu(void)
{
    static bool mnu_led = false;
    static int32_t mnu_bright = 50;
    static const SYN_MenuItem s_items[] = {
        SYN_MENU_TOGGLE("LED", &mnu_led),
        SYN_MENU_VALUE("Bright", &mnu_bright, 0, 100, 10),
        SYN_MENU_CALLBACK("Save", mnu_cb, NULL),
    };
    static const SYN_MenuItem r_items[] = {
        SYN_MENU_SUBMENU("Settings", s_items),
        SYN_MENU_CALLBACK("Reboot", mnu_cb, NULL),
    };
    SYN_MENU_ROOT(root, r_items);
    SYN_Menu menu;
    mnu_render_n = 0;
    mnu_cb_n = 0;
    mnu_led = false;
    mnu_bright = 50;
    syn_menu_init(&menu, &root, mnu_render, NULL);
    TEST_ASSERT_TRUE(menu.selected == 0);
    syn_menu_down(&menu);
    TEST_ASSERT_TRUE(menu.selected == 1);
    syn_menu_down(&menu);
    TEST_ASSERT_TRUE(menu.selected == 0);
    syn_menu_enter(&menu);
    TEST_ASSERT_TRUE(menu.depth == 1);
    TEST_ASSERT_TRUE(mnu_led == false);
    syn_menu_enter(&menu);
    TEST_ASSERT_TRUE(mnu_led == true);
    syn_menu_enter(&menu);
    TEST_ASSERT_TRUE(mnu_led == false);
    syn_menu_down(&menu);
    syn_menu_enter(&menu);
    TEST_ASSERT_TRUE(menu.editing);
    syn_menu_up(&menu);
    TEST_ASSERT_EQUAL_INT(60, mnu_bright);
    for (int i = 0; i < 5; i++)
        syn_menu_up(&menu);
    TEST_ASSERT_EQUAL_INT(100, mnu_bright);
    syn_menu_down(&menu);
    TEST_ASSERT_EQUAL_INT(90, mnu_bright);
    syn_menu_enter(&menu);
    TEST_ASSERT_TRUE(!menu.editing);
    syn_menu_down(&menu);
    mnu_cb_n = 0;
    syn_menu_enter(&menu);
    TEST_ASSERT_EQUAL_INT(1, mnu_cb_n);
    syn_menu_back(&menu);
    TEST_ASSERT_TRUE(menu.depth == 0);
    TEST_ASSERT_TRUE(mnu_render_n > 5);
}

/** Navigate up from position 0 — exercises wrap-around (lines 47-48) */
static void test_menu_up_wrap(void)
{
    static bool mnu_led2 = false;
    static int32_t mnu_bright2 = 50;
    static const SYN_MenuItem s2_items[] = {
        SYN_MENU_TOGGLE("LED", &mnu_led2),
        SYN_MENU_VALUE("Bright", &mnu_bright2, 0, 100, 10),
        SYN_MENU_CALLBACK("Save", mnu_cb, NULL),
    };
    static const SYN_MenuItem r2_items[] = {
        SYN_MENU_SUBMENU("Settings", s2_items),
        SYN_MENU_CALLBACK("Reboot", mnu_cb, NULL),
    };
    SYN_MENU_ROOT(root2, r2_items);
    SYN_Menu menu2;
    mnu_render_n = 0;
    syn_menu_init(&menu2, &root2, mnu_render, NULL);

    /* At position 0 — up should wrap to last item */
    TEST_ASSERT_EQUAL_INT(0, menu2.selected);
    syn_menu_up(&menu2);
    /* Should wrap to last item (index 1) */
    TEST_ASSERT_EQUAL_INT(1, menu2.selected);

    /* Go up again (from 1 → 0) — exercises line 50 (decrement) */
    syn_menu_up(&menu2);
    TEST_ASSERT_EQUAL_INT(0, menu2.selected);
}

/** syn_menu_back while editing — exercises lines 130-132 (cancel edit) */
static void test_menu_back_while_editing(void)
{
    static bool mnu_led3 = false;
    static int32_t mnu_bright3 = 50;
    static const SYN_MenuItem s3_items[] = {
        SYN_MENU_TOGGLE("LED", &mnu_led3),
        SYN_MENU_VALUE("Bright", &mnu_bright3, 0, 100, 10),
    };
    static const SYN_MenuItem r3_items[] = {
        SYN_MENU_SUBMENU("Settings", s3_items),
    };
    SYN_MENU_ROOT(root3, r3_items);
    SYN_Menu menu3;
    mnu_render_n = 0;
    syn_menu_init(&menu3, &root3, mnu_render, NULL);

    /* Enter submenu, move to Value item, enter edit mode */
    syn_menu_enter(&menu3); /* into Settings submenu */
    syn_menu_down(&menu3);  /* select Bright (index 1) */
    syn_menu_enter(&menu3); /* start editing */
    TEST_ASSERT_TRUE(menu3.editing);

    /* Back while editing — should cancel edit but stay in submenu */
    syn_menu_back(&menu3);
    TEST_ASSERT_FALSE(menu3.editing);
    TEST_ASSERT_EQUAL_INT(1, menu3.depth); /* still in submenu */

    /* Decrement edit mode to min bound clamping */
    syn_menu_enter(&menu3); /* start editing */
    for (int i = 0; i < 10; i++)
        syn_menu_down(&menu3);
    TEST_ASSERT_EQUAL_INT(0, mnu_bright3);
}

static void test_menu_max_depth(void)
{
    static const SYN_MenuItem d8_items[] = {SYN_MENU_CALLBACK("Leaf", mnu_cb, NULL)};
    static const SYN_MenuItem d7_items[] = {SYN_MENU_SUBMENU("L8", d8_items)};
    static const SYN_MenuItem d6_items[] = {SYN_MENU_SUBMENU("L7", d7_items)};
    static const SYN_MenuItem d5_items[] = {SYN_MENU_SUBMENU("L6", d6_items)};
    static const SYN_MenuItem d4_items[] = {SYN_MENU_SUBMENU("L5", d5_items)};
    static const SYN_MenuItem d3_items[] = {SYN_MENU_SUBMENU("L4", d4_items)};
    static const SYN_MenuItem d2_items[] = {SYN_MENU_SUBMENU("L3", d3_items)};
    static const SYN_MenuItem d1_items[] = {SYN_MENU_SUBMENU("L2", d2_items)};
    SYN_MENU_ROOT(root_nest, d1_items);

    SYN_Menu menu;
    syn_menu_init(&menu, &root_nest, mnu_render, NULL);

    /* Push through 8 levels of submenus until capped at MAX_DEPTH - 1 (7) */
    for (int i = 0; i < 10; i++) {
        syn_menu_enter(&menu);
    }
    TEST_ASSERT_EQUAL_INT(SYN_MENU_MAX_DEPTH - 1, menu.depth);
}

static void test_menu_null_item_pointers(void)
{
    static const SYN_MenuItem null_items[] = {
        SYN_MENU_TOGGLE("NullToggle", NULL),
        SYN_MENU_CALLBACK("NullCb", NULL, NULL),
        SYN_MENU_VALUE("NullValue", NULL, 0, 100, 1),
    };
    SYN_MENU_ROOT(root_null, null_items);

    SYN_Menu menu;
    syn_menu_init(&menu, &root_null, NULL, NULL); /* NULL render fn */

    /* Toggle with NULL bool pointer */
    syn_menu_enter(&menu);

    /* Callback with NULL func pointer */
    syn_menu_down(&menu);
    syn_menu_enter(&menu);

    /* Value with NULL int32_t pointer: edit mode enter/up/down */
    syn_menu_down(&menu);
    syn_menu_enter(&menu);
    TEST_ASSERT_TRUE(menu.editing);
    syn_menu_up(&menu);
    syn_menu_down(&menu);
    syn_menu_back(&menu);
    TEST_ASSERT_FALSE(menu.editing);

    /* Empty submenu handling */
    static const SYN_MenuItem empty_items[1] = {SYN_MENU_TOGGLE("Dummy", NULL)};
    SYN_MenuItem empty_root = {
        .label = "Empty",
        .action = SYN_MENU_ACTION_SUBMENU,
        .u.submenu = {.children = empty_items, .count = 0},
    };
    SYN_Menu empty_menu;
    syn_menu_init(&empty_menu, &empty_root, NULL, NULL);
    syn_menu_up(&empty_menu);
    syn_menu_down(&empty_menu);
    syn_menu_enter(&empty_menu);
    TEST_ASSERT_NULL(syn_menu_selected_item(&empty_menu));
}

static void test_menu_selected_item_and_render(void)
{
    TEST_ASSERT_NULL(syn_menu_selected_item(NULL));

    SYN_Menu menu;
    memset(&menu, 0, sizeof(menu));
    TEST_ASSERT_NULL(syn_menu_selected_item(&menu));

    static bool toggle_val = false;
    static const SYN_MenuItem items[] = {
        SYN_MENU_TOGGLE("Test1", &toggle_val),
        SYN_MENU_TOGGLE("Test2", &toggle_val),
    };
    SYN_MENU_ROOT(root_sel, items);

    mnu_render_n = 0;
    syn_menu_init(&menu, &root_sel, mnu_render, NULL);
    TEST_ASSERT_EQUAL_UINT8(2, syn_menu_item_count(&menu));

    const SYN_MenuItem *sel = syn_menu_selected_item(&menu);
    TEST_ASSERT_NOT_NULL(sel);
    TEST_ASSERT_EQUAL_STRING("Test1", sel->label);

    /* Test selection index out of bounds */
    menu.selected = 10;
    TEST_ASSERT_NULL(syn_menu_selected_item(&menu));
    menu.selected = 0;

    /* Force manual render */
    int prev_render = mnu_render_n;
    syn_menu_render(&menu);
    TEST_ASSERT_EQUAL_INT(prev_render + 1, mnu_render_n);

    /* Render with NULL callback */
    menu.render = NULL;
    syn_menu_render(&menu);
}

void run_menu_tests(void)
{
    RUN_TEST(test_menu);
    RUN_TEST(test_menu_up_wrap);
    RUN_TEST(test_menu_back_while_editing);
    RUN_TEST(test_menu_max_depth);
    RUN_TEST(test_menu_null_item_pointers);
    RUN_TEST(test_menu_selected_item_and_render);
}
