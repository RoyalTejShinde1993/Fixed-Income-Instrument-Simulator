#include "fixed_income/bond.hpp"
#include "fixed_income/yield_curve.hpp"

#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace fixed_income;

namespace {

double parseNumber(const char* value, const char* name, double min, double max) {
    if (value == nullptr) return min;
    char* end = nullptr;
    const double parsed = std::strtod(value, &end);
    if (end == value || *end != '\0' || !std::isfinite(parsed) || parsed < min || parsed > max) {
        throw std::invalid_argument(std::string("Invalid ") + name);
    }
    return parsed;
}

void printNumber(double value) {
    if (std::isfinite(value)) std::cout << std::setprecision(12) << value;
    else std::cout << "null";
}

} // namespace

int main(int argc, char** argv) {
    try {
        const double notional = argc > 1 ? parseNumber(argv[1], "notional", 1.0, 1.0e12) : 1000.0;
        const double coupon = argc > 2 ? parseNumber(argv[2], "coupon", 0.0, 1.0) : 0.05;
        const double ytm = argc > 3 ? parseNumber(argv[3], "ytm", -0.01, 1.0) : 0.05;

        const auto issue = sys_days(std::chrono::year{2024}/1/1);
        const auto maturity = sys_days(std::chrono::year{2029}/1/1);
        const auto settlement = issue;
        Bond bond(notional, coupon, issue, maturity, Frequency::SemiAnnual, settlement);
        const double price = bond.price(ytm);
        const double duration = bond.modifiedDuration(ytm);
        const double convexity = bond.convexity(ytm);
        const double dv01 = bond.dv01(ytm);
        const double pv01 = bond.pv01(ytm);

        std::cout << "{\"source\":\"C++ fixed_income library\",\"inputs\":{\"notional\":";
        printNumber(notional);
        std::cout << ",\"couponRate\":"; printNumber(coupon);
        std::cout << ",\"yieldToMaturity\":"; printNumber(ytm);
        std::cout << "},\"analytics\":{\"price\":"; printNumber(price);
        std::cout << ",\"macaulayDuration\":"; printNumber(bond.macaulayDuration(ytm));
        std::cout << ",\"modifiedDuration\":"; printNumber(duration);
        std::cout << ",\"convexity\":"; printNumber(convexity);
        std::cout << ",\"dv01\":"; printNumber(dv01);
        std::cout << ",\"pv01\":"; printNumber(pv01);
        std::cout << "},\"priceYield\":[";
        for (int i = 0; i <= 20; ++i) {
            if (i) std::cout << ',';
            const double scenarioYield = std::max(-0.009, ytm - 0.02 + i * 0.002);
            std::cout << "{\"yield\":"; printNumber(scenarioYield);
            std::cout << ",\"price\":"; printNumber(bond.price(scenarioYield));
            std::cout << '}';
        }
        std::cout << "],\"stress\":[";
        const std::vector<std::pair<const char*, double>> scenarios = {
            {"Rates -100 bp", -0.01}, {"Rates -50 bp", -0.005},
            {"Base case", 0.0}, {"Rates +50 bp", 0.005}, {"Rates +100 bp", 0.01}
        };
        for (std::size_t i = 0; i < scenarios.size(); ++i) {
            if (i) std::cout << ',';
            const double stressedPrice = bond.price(ytm + scenarios[i].second);
            std::cout << "{\"scenario\":\"" << scenarios[i].first << "\",\"yieldShift\":";
            printNumber(scenarios[i].second);
            std::cout << ",\"price\":"; printNumber(stressedPrice);
            std::cout << ",\"pnl\":"; printNumber(stressedPrice - price);
            std::cout << '}';
        }
        std::cout << "]}\n";
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << ex.what() << '\n';
        return 2;
    }
}
