#include "fixed_income/bond.hpp"
#include "fixed_income/risk.hpp"

#include <gtest/gtest.h>

using namespace fixed_income;

static std::chrono::sys_days make_date(int year, unsigned month, unsigned day) {
    return std::chrono::sys_days(std::chrono::year{year} / std::chrono::month{month} / std::chrono::day{day});
}

TEST(RiskEngine, CS01IsPositiveForFixedRateBond) {
    Bond bond(1000.0, 0.05, make_date(2021, 1, 1), make_date(2026, 1, 1), Frequency::SemiAnnual, make_date(2021, 1, 1));
    const double cs01 = RiskEngine::cs01(bond, 0.05);
    EXPECT_GT(cs01, 0.0);
}

TEST(RiskEngine, PortfolioMarketValueAggregatesPositions) {
    Portfolio portfolio;
    auto bond = std::make_shared<Bond>(1000.0, 0.05, make_date(2021, 1, 1), make_date(2026, 1, 1), Frequency::SemiAnnual, make_date(2021, 1, 1));
    portfolio.addPosition({"BOND1", bond, 2.0});
    EXPECT_GT(portfolio.totalMarketValue(0.05), 0.0);
}

TEST(RiskEngine, ScenarioAnalysisProducesPnL) {
    Portfolio portfolio;
    auto bond = std::make_shared<Bond>(1000.0, 0.05, make_date(2021, 1, 1), make_date(2026, 1, 1), Frequency::SemiAnnual, make_date(2021, 1, 1));
    portfolio.addPosition({"BOND1", bond, 1.0});
    const auto scenarios = RiskEngine::scenarioAnalysis(portfolio, 0.05, {{"Up50bp", 0.005}, {"Down50bp", -0.005}});
    EXPECT_EQ(scenarios.size(), 2u);
    EXPECT_NE(scenarios[0].name, scenarios[1].name);
}
