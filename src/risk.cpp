#include "fixed_income/risk.hpp"

#include <cmath>
#include <stdexcept>

namespace fixed_income {

void Portfolio::addPosition(RiskPosition position) {
    positions_.push_back(std::move(position));
}

double Portfolio::totalMarketValue(double ytm) const {
    double total = 0.0;
    for (const auto& position : positions_) {
        total += position.marketValue(ytm) * position.quantity;
    }
    return total;
}

double Portfolio::totalCS01(double ytm) const {
    double total = 0.0;
    for (const auto& position : positions_) {
        total += RiskEngine::cs01(*position.instrument, ytm) * position.quantity;
    }
    return total;
}

const std::vector<RiskPosition>& Portfolio::positions() const noexcept {
    return positions_;
}

double RiskPosition::marketValue(double ytm) const {
    return instrument->price(ytm);
}

static double shiftYtm(double ytm, double deltaBp) {
    return ytm + deltaBp * 1e-4;
}

double RiskEngine::cs01(const Bond& bond, double ytm) {
    const double up = bond.price(shiftYtm(ytm, 1.0));
    const double down = bond.price(shiftYtm(ytm, -1.0));
    return (down - up) / 2.0;
}

std::vector<KeyRateDuration> RiskEngine::keyRateDurations(const Bond& bond,
                                                         double ytm,
                                                         const std::vector<double>& keyTenors) {
    std::vector<KeyRateDuration> result;
    for (const double tenor : keyTenors) {
        const double basePrice = bond.price(ytm);
        const double bumpedPrice = bond.price(ytm + tenor);
        const double duration = -(bumpedPrice - basePrice) / (basePrice * tenor);
        result.push_back({tenor, duration});
    }
    return result;
}

std::vector<ScenarioResult> RiskEngine::scenarioAnalysis(const Portfolio& portfolio,
                                                        double ytm,
                                                        const std::vector<std::pair<std::string, double>>& scenarios) {
    std::vector<ScenarioResult> results;
    const double baseMarketValue = portfolio.totalMarketValue(ytm);
    for (const auto& scenario : scenarios) {
        const double stressedValue = portfolio.totalMarketValue(ytm + scenario.second);
        results.push_back({scenario.first, stressedValue - baseMarketValue, stressedValue});
    }
    return results;
}

} // namespace fixed_income
