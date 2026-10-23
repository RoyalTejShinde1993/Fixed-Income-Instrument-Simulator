#include "fixed_income/bond.hpp"

#include <gtest/gtest.h>
#include <limits>

using namespace fixed_income;

static sys_days make_date(int year, unsigned month, unsigned day) {
    return sys_days(std::chrono::year{year} / std::chrono::month{month} / std::chrono::day{day});
}

TEST(BondAnalytics, CashFlowScheduleProducesExpectedPayments) {
    Bond bond(1000.0, 0.05, make_date(2021, 1, 1), make_date(2026, 1, 1), Frequency::SemiAnnual, make_date(2021, 1, 1));
    const auto schedule = bond.cashFlowSchedule();

    EXPECT_EQ(schedule.payments.size(), 10u);
    EXPECT_DOUBLE_EQ(schedule.payments.front().amount, 25.0);
    EXPECT_DOUBLE_EQ(schedule.payments.back().amount, 1025.0);
    EXPECT_GT(schedule.totalAmount(), 1000.0);
}

TEST(BondAnalytics, PriceEqualsParForParBond) {
    Bond bond(1000.0, 0.05, make_date(2021, 1, 1), make_date(2026, 1, 1), Frequency::SemiAnnual, make_date(2021, 1, 1));
    const double price = bond.price(0.05);

    EXPECT_NEAR(price, 1000.0, 1e-8);
}

TEST(BondAnalytics, YieldToMaturitySolvesPrice) {
    Bond bond(1000.0, 0.05, make_date(2021, 1, 1), make_date(2026, 1, 1), Frequency::SemiAnnual, make_date(2021, 1, 1));
    const double ytm = bond.yieldToMaturity(980.0);
    const double price = bond.price(ytm);

    EXPECT_NEAR(price, 980.0, 1e-6);
    EXPECT_GT(ytm, 0.05);
}

TEST(BondAnalytics, DurationAndConvexityArePositive) {
    Bond bond(1000.0, 0.05, make_date(2021, 1, 1), make_date(2026, 1, 1), Frequency::SemiAnnual, make_date(2021, 1, 1));
    const double ytm = 0.05;
    const double macaulay = bond.macaulayDuration(ytm);
    const double modified = bond.modifiedDuration(ytm);
    const double convexity = bond.convexity(ytm);
    const double dv01 = bond.dv01(ytm);

    EXPECT_GT(macaulay, modified);
    EXPECT_GT(modified, 0.0);
    EXPECT_GT(convexity, 0.0);
    EXPECT_GT(dv01, 0.0);
    EXPECT_NEAR(bond.pv01(ytm), dv01 / 1000.0, 1e-12);
}

TEST(BondAnalytics, CorporateBondDiscountsCreditSpread) {
    TreasuryBond treasury(1000.0, 0.05, make_date(2021, 1, 1), make_date(2026, 1, 1), Frequency::SemiAnnual, make_date(2021, 1, 1));
    CorporateBond corporate(1000.0, 0.05, make_date(2021, 1, 1), make_date(2026, 1, 1), Frequency::SemiAnnual, 0.01, make_date(2021, 1, 1));

    const double treasuryPrice = treasury.price(0.05);
    const double corporatePrice = corporate.price(0.05);

    EXPECT_LT(corporatePrice, treasuryPrice);
}

TEST(BondAnalytics, FloatingRateNotePricesWithForwardCurve) {
    std::vector<double> forwardRates(10, 0.02);
    FloatingRateNote frn(1000.0, 0.005, make_date(2021, 1, 1), make_date(2026, 1, 1), Frequency::SemiAnnual, forwardRates, make_date(2021, 1, 1));
    const double price = frn.price(0.025);

    EXPECT_GT(price, 900.0);
    EXPECT_LT(price, 1100.0);
}

TEST(BondAnalytics, SettlementBetweenCouponsUsesFractionalFirstPeriod) {
    const auto issue = make_date(2024, 1, 1);
    const auto settlement = make_date(2024, 4, 1);
    const auto maturity = make_date(2026, 1, 1);
    Bond bond(1000.0, 0.05, issue, maturity, Frequency::SemiAnnual, settlement);
    const auto schedule = bond.cashFlowSchedule();

    ASSERT_FALSE(schedule.empty());
    EXPECT_LT(schedule.payments.front().yearFraction, 0.5);
    EXPECT_GT(schedule.payments.front().yearFraction, 0.0);
    if (schedule.payments.size() > 1) {
        EXPECT_NEAR(schedule.payments[1].yearFraction - schedule.payments[0].yearFraction, 0.5, 1e-12);
    }
}

TEST(BondAnalytics, RejectsNonFiniteYield) {
    Bond bond(1000.0, 0.05, make_date(2021, 1, 1), make_date(2026, 1, 1), Frequency::SemiAnnual, make_date(2021, 1, 1));
    EXPECT_THROW(bond.price(std::numeric_limits<double>::quiet_NaN()), std::invalid_argument);
}
