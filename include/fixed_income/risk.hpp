#pragma once

#include "fixed_income/bond.hpp"

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace fixed_income {

struct RiskPosition {
    std::string id;
    std::shared_ptr<Bond> instrument;
    double quantity;

    double marketValue(double ytm) const;
};

class Portfolio {
public:
    void addPosition(RiskPosition position);
    double totalMarketValue(double ytm) const;
    double totalCS01(double ytm) const;
    const std::vector<RiskPosition>& positions() const noexcept;

private:
    std::vector<RiskPosition> positions_;
};

struct KeyRateDuration {
    double tenor;
    double duration;
};

struct ScenarioResult {
    std::string name;
    double pnl;
    double marketValue;
};

class RiskEngine {
public:
    static double cs01(const Bond& bond, double ytm);
    static std::vector<KeyRateDuration> keyRateDurations(const Bond& bond,
                                                         double ytm,
                                                         const std::vector<double>& keyTenors);
    static std::vector<ScenarioResult> scenarioAnalysis(const Portfolio& portfolio,
                                                        double ytm,
                                                        const std::vector<std::pair<std::string, double>>& scenarios);
};

} // namespace fixed_income
