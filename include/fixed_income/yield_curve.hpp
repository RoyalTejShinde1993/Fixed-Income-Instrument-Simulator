#pragma once

#include <vector>

namespace fixed_income {

enum class Interpolation {
    Linear,
    CubicSpline
};

struct CurvePoint {
    double yearFraction;
    double zeroRate;
};

class YieldCurve {
public:
    YieldCurve(std::vector<CurvePoint> points, Interpolation interpolation = Interpolation::CubicSpline);

    double zeroRate(double yearFraction) const;
    double discountFactor(double yearFraction) const;
    double forwardRate(double startYearFraction, double endYearFraction) const;

    const std::vector<CurvePoint>& points() const noexcept;
    Interpolation interpolation() const noexcept;

private:
    std::vector<CurvePoint> points_;
    Interpolation interpolation_;

    std::vector<double> splineA_;
    std::vector<double> splineB_;
    std::vector<double> splineC_;
    std::vector<double> splineD_;

    void buildSpline();
    double interpolateLinear(double yearFraction) const;
    double interpolateCubicSpline(double yearFraction) const;
};

class YieldCurveBootstrapper {
public:
    static YieldCurve bootstrapFromDepositAndSwapRates(
        const std::vector<double>& depositRates,
        const std::vector<double>& depositMaturities,
        const std::vector<double>& swapRates,
        const std::vector<double>& swapMaturities,
        Interpolation interpolation = Interpolation::CubicSpline);
};

} // namespace fixed_income
