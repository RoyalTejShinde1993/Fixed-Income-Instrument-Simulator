#include "fixed_income/yield_curve.hpp"

#include <cmath>
#include <limits>
#include <stdexcept>
#include <gtest/gtest.h>

using namespace fixed_income;

TEST(YieldCurve, LinearInterpolationReturnsIntermediateRate) {
    YieldCurve curve({{0.5, 0.01}, {1.0, 0.02}}, Interpolation::Linear);
    EXPECT_NEAR(curve.zeroRate(0.75), 0.015, 1e-12);
}

TEST(YieldCurve, CubicSplineInterpolationIsSmoothAtKnots) {
    YieldCurve curve({{0.5, 0.01}, {1.0, 0.02}, {2.0, 0.025}}, Interpolation::CubicSpline);
    EXPECT_NEAR(curve.zeroRate(0.5), 0.01, 1e-12);
    EXPECT_NEAR(curve.zeroRate(1.0), 0.02, 1e-12);
    EXPECT_GT(curve.zeroRate(1.5), 0.02);
}

TEST(YieldCurve, DiscountFactorIsExpOfZeroRate) {
    YieldCurve curve({{1.0, 0.05}, {2.0, 0.06}}, Interpolation::Linear);
    EXPECT_NEAR(curve.discountFactor(1.0), std::exp(-0.05), 1e-12);
}

TEST(YieldCurve, ForwardRateMatchesDiscountFactorRatio) {
    YieldCurve curve({{1.0, 0.02}, {2.0, 0.025}}, Interpolation::Linear);
    const double fwd = curve.forwardRate(1.0, 2.0);
    const double df1 = curve.discountFactor(1.0);
    const double df2 = curve.discountFactor(2.0);
    EXPECT_NEAR(fwd, std::log(df1 / df2), 1e-12);
}

TEST(YieldCurveBootstrapper, BootstrapsFromDepositAndSwapRates) {
    std::vector<double> deposits{0.005, 0.007};
    std::vector<double> depositMaturities{0.25, 0.5};
    std::vector<double> swaps{0.01, 0.012};
    std::vector<double> swapMaturities{1.0, 2.0};
    const YieldCurve curve = YieldCurveBootstrapper::bootstrapFromDepositAndSwapRates(
        deposits, depositMaturities, swaps, swapMaturities, Interpolation::Linear);

    EXPECT_EQ(curve.points().size(), 4u);
    EXPECT_NEAR(curve.zeroRate(0.25), 0.005, 1e-12);
    EXPECT_NEAR(curve.zeroRate(0.5), 0.007, 1e-12);
}

TEST(YieldCurve, RejectsDuplicateMaturitiesForLinearInterpolation) {
    EXPECT_THROW((YieldCurve({{0.5, 0.01}, {0.5, 0.02}}, Interpolation::Linear)), std::invalid_argument);
}

TEST(YieldCurve, RejectsNonFiniteCurveValues) {
    EXPECT_THROW((YieldCurve({{0.5, 0.01}, {1.0, std::numeric_limits<double>::infinity()}}, Interpolation::Linear)), std::invalid_argument);
}
