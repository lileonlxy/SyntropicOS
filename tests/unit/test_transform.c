/**
 * @file test_transform.c
 * @brief Unit tests for coordinate transformations.
 */

#include "syntropic/util/syn_transform.h"
#include "unity/unity.h"

#define Q16_TOL 600

static void ASSERT_Q16_NEAR(q16_t expected, q16_t actual, q16_t tol)
{
    q16_t diff = q16_abs(expected - actual);
    if (diff > tol) {
        TEST_FAIL_MESSAGE("Q16 values not equal within tolerance");
    }
}

void test_cart2pol_and_pol2cart(void)
{
    q16_t x_in = Q16_FROM_INT(3);
    q16_t y_in = Q16_FROM_INT(4);
    q16_t r, theta;

    syn_cart2pol(x_in, y_in, &r, &theta);
    ASSERT_Q16_NEAR(Q16_FROM_INT(5), r, Q16_TOL);

    q16_t x_out, y_out;
    syn_pol2cart(r, theta, &x_out, &y_out);
    ASSERT_Q16_NEAR(x_in, x_out, Q16_TOL);
    ASSERT_Q16_NEAR(y_in, y_out, Q16_TOL);
}

void test_cart2sph_and_sph2cart(void)
{
    q16_t x_in = Q16_FROM_INT(1);
    q16_t y_in = Q16_FROM_INT(2);
    q16_t z_in = Q16_FROM_INT(2);
    q16_t r, theta, phi;

    syn_cart2sph(x_in, y_in, z_in, &r, &theta, &phi);
    ASSERT_Q16_NEAR(Q16_FROM_INT(3), r, Q16_TOL);

    q16_t x_out, y_out, z_out;
    syn_sph2cart(r, theta, phi, &x_out, &y_out, &z_out);
    ASSERT_Q16_NEAR(x_in, x_out, Q16_TOL);
    ASSERT_Q16_NEAR(y_in, y_out, Q16_TOL);
    ASSERT_Q16_NEAR(z_in, z_out, Q16_TOL);
}

void test_transform_quadrants_and_negative(void)
{
    /* Test Quadrant 2 (-x, +y) */
    q16_t r, theta, x, y;
    syn_cart2pol(-Q16_FROM_INT(3), Q16_FROM_INT(4), &r, &theta);
    syn_pol2cart(r, theta, &x, &y);
    ASSERT_Q16_NEAR(-Q16_FROM_INT(3), x, Q16_TOL);
    ASSERT_Q16_NEAR(Q16_FROM_INT(4), y, Q16_TOL);

    /* Test Quadrant 3 (-x, -y) */
    syn_cart2pol(-Q16_FROM_INT(3), -Q16_FROM_INT(4), &r, &theta);
    syn_pol2cart(r, theta, &x, &y);
    ASSERT_Q16_NEAR(-Q16_FROM_INT(3), x, Q16_TOL);
    ASSERT_Q16_NEAR(-Q16_FROM_INT(4), y, Q16_TOL);

    /* Test Quadrant 4 (+x, -y) */
    syn_cart2pol(Q16_FROM_INT(3), -Q16_FROM_INT(4), &r, &theta);
    syn_pol2cart(r, theta, &x, &y);
    ASSERT_Q16_NEAR(Q16_FROM_INT(3), x, Q16_TOL);
    ASSERT_Q16_NEAR(-Q16_FROM_INT(4), y, Q16_TOL);

    /* Test 3D Spherical Negative Z */
    q16_t phi, z;
    syn_cart2sph(Q16_FROM_INT(1), Q16_FROM_INT(2), -Q16_FROM_INT(2), &r, &theta, &phi);
    syn_sph2cart(r, theta, phi, &x, &y, &z);
    ASSERT_Q16_NEAR(Q16_FROM_INT(1), x, Q16_TOL);
    ASSERT_Q16_NEAR(Q16_FROM_INT(2), y, Q16_TOL);
    ASSERT_Q16_NEAR(-Q16_FROM_INT(2), z, Q16_TOL);
}

void run_transform_tests(void)
{
    RUN_TEST(test_cart2pol_and_pol2cart);
    RUN_TEST(test_cart2sph_and_sph2cart);
    RUN_TEST(test_transform_quadrants_and_negative);
}
