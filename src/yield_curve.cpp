#include "fixed_income/yield_curve.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace fixed_income {

static double clamp(double value, double minValue, double maxValue) {
    return std::max(minValue, std::min(value, maxValue));
}

YieldCurve::YieldCurve(std::vector<CurvePoint> points, Interpolation interpolation)
    : points_(std::move(points)),
      interpolation_(interpolation) {
    if (points_.size() < 2) {
        throw std::invalid_argument("At least two curve points are required.");
    }
    for (const auto& point : points_) {
        if (!std::isfinite(point.yearFraction) || point.yearFraction <= 0.0 ||
            !std::isfinite(point.zeroRate)) {
            throw std::invalid_argument("Curve maturities must be positive and all curve values finite.");
        }
    }
    std::sort(points_.begin(), points_.end(), [](const CurvePoint& a, const CurvePoint& b) {
        return a.yearFraction < b.yearFraction;
    });
    for (std::size_t i = 1; i < points_.size(); ++i) {
        if (points_[i].yearFraction <= points_[i - 1].yearFraction) {
            throw std::invalid_argument("Curve maturities must be unique and strictly increasing.");
        }
    }
    buildSpline();
}

const std::vector<CurvePoint>& YieldCurve::points() const noexcept {
    return points_;
}

Interpolation YieldCurve::interpolation() const noexcept {
    return interpolation_;
}

void YieldCurve::buildSpline() {
    const std::size_t n = points_.size();
    splineA_.assign(n, 0.0);
    splineB_.assign(n, 0.0);
    splineC_.assign(n, 0.0);
    splineD_.assign(n, 0.0);

    if (interpolation_ != Interpolation::CubicSpline) {
        return;
    }

    std::vector<double> h(n - 1);
    for (std::size_t i = 0; i < n - 1; ++i) {
        h[i] = points_[i + 1].yearFraction - points_[i].yearFraction;
        if (h[i] <= 0.0) {
            throw std::invalid_argument("Curve points must have strictly increasing year fractions.");
        }
    }

    std::vector<double> alpha(n - 1);
    for (std::size_t i = 1; i < n - 1; ++i) {
        alpha[i] = 3.0 * ((points_[i + 1].zeroRate - points_[i].zeroRate) / h[i] -
                          (points_[i].zeroRate - points_[i - 1].zeroRate) / h[i - 1]);
    }

    std::vector<double> l(n);
    std::vector<double> mu(n);
    std::vector<double> z(n);
    l[0] = 1.0;
    mu[0] = 0.0;
    z[0] = 0.0;

    for (std::size_t i = 1; i < n - 1; ++i) {
        l[i] = 2.0 * (points_[i + 1].yearFraction - points_[i - 1].yearFraction) - h[i - 1] * mu[i - 1];
        mu[i] = h[i] / l[i];
        z[i] = (alpha[i] - h[i - 1] * z[i - 1]) / l[i];
    }

    l[n - 1] = 1.0;
    z[n - 1] = 0.0;
    splineC_[n - 1] = 0.0;

    for (std::size_t j = n - 1; j-- > 0;) {
        splineC_[j] = z[j] - mu[j] * splineC_[j + 1];
        splineB_[j] = (points_[j + 1].zeroRate - points_[j].zeroRate) / h[j] - h[j] * (splineC_[j + 1] + 2.0 * splineC_[j]) / 3.0;
        splineD_[j] = (splineC_[j + 1] - splineC_[j]) / (3.0 * h[j]);
        splineA_[j] = points_[j].zeroRate;
    }
}

static std::size_t locateInterval(const std::vector<CurvePoint>& points, double yearFraction) {
    const std::size_t n = points.size();
    if (yearFraction <= points.front().yearFraction) {
        return 0;
    }
    if (yearFraction >= points.back().yearFraction) {
        return n - 2;
    }
    auto it = std::upper_bound(points.begin(), points.end(), yearFraction,
        [](double value, const CurvePoint& point) {
            return value < point.yearFraction;
        });
    return static_cast<std::size_t>(std::distance(points.begin(), it)) - 1;
}

double YieldCurve::interpolateLinear(double yearFraction) const {
    const std::size_t idx = locateInterval(points_, yearFraction);
    const auto& left = points_[idx];
    const auto& right = points_[idx + 1];
    const double weight = (yearFraction - left.yearFraction) / (right.yearFraction - left.yearFraction);
    return left.zeroRate + weight * (right.zeroRate - left.zeroRate);
}

double YieldCurve::interpolateCubicSpline(double yearFraction) const {
    const std::size_t idx = locateInterval(points_, yearFraction);
    const double dx = yearFraction - points_[idx].yearFraction;
    return splineA_[idx] + splineB_[idx] * dx + splineC_[idx] * dx * dx + splineD_[idx] * dx * dx * dx;
}

double YieldCurve::zeroRate(double yearFraction) const {
    if (!std::isfinite(yearFraction)) {
        throw std::invalid_argument("Year fraction must be finite.");
    }
    if (yearFraction <= 0.0) {
        return points_.front().zeroRate;
    }
    const double clamped = clamp(yearFraction, points_.front().yearFraction, points_.back().yearFraction);
    if (interpolation_ == Interpolation::Linear) {
        return interpolateLinear(clamped);
    }
    return interpolateCubicSpline(clamped);
}

double YieldCurve::discountFactor(double yearFraction) const {
    const double z = zeroRate(yearFraction);
    return std::exp(-z * yearFraction);
}

double YieldCurve::forwardRate(double startYearFraction, double endYearFraction) const {
    if (!std::isfinite(startYearFraction) || !std::isfinite(endYearFraction) ||
        startYearFraction < 0.0 || endYearFraction <= startYearFraction) {
        throw std::invalid_argument("End year fraction must be greater than start year fraction.");
    }
    const double dfStart = discountFactor(startYearFraction);
    const double dfEnd = discountFactor(endYearFraction);
    return std::log(dfStart / dfEnd) / (endYearFraction - startYearFraction);
}

YieldCurve YieldCurveBootstrapper::bootstrapFromDepositAndSwapRates(
    const std::vector<double>& depositRates,
    const std::vector<double>& depositMaturities,
    const std::vector<double>& swapRates,
    const std::vector<double>& swapMaturities,
    Interpolation interpolation) {

    if (depositRates.size() != depositMaturities.size() || swapRates.size() != swapMaturities.size()) {
        throw std::invalid_argument("Maturities and rates size mismatch.");
    }
    std::vector<CurvePoint> points;
    points.reserve(depositRates.size() + swapRates.size());

    for (std::size_t i = 0; i < depositRates.size(); ++i) {
        points.push_back({depositMaturities[i], depositRates[i]});
    }

    std::vector<double> discountFactors(points.size());
    for (std::size_t i = 0; i < points.size(); ++i) {
        discountFactors[i] = std::exp(-points[i].zeroRate * points[i].yearFraction);
    }

    for (std::size_t i = 0; i < swapRates.size(); ++i) {
        const double maturity = swapMaturities[i];
        const double swapRate = swapRates[i];
        double accrual = 0.0;
        double pvFixed = 0.0;
        double impliedDF = 0.0;

        for (std::size_t j = 0; j < points.size(); ++j) {
            if (points[j].yearFraction < maturity) {
                accrual += points[j].yearFraction - (j == 0 ? 0.0 : points[j - 1].yearFraction);
                pvFixed += swapRate * discountFactors[j] * (points[j].yearFraction - (j == 0 ? 0.0 : points[j - 1].yearFraction));
            }
        }

        impliedDF = (1.0 - pvFixed) / (1.0 + swapRate * accrual);
        if (!(impliedDF > 0.0)) {
            throw std::runtime_error("Could not bootstrap discount factor for swap maturity.");
        }

        const double zeroRate = -std::log(impliedDF) / maturity;
        points.push_back({maturity, zeroRate});
        discountFactors.push_back(impliedDF);
    }

    return YieldCurve(std::move(points), interpolation);
}

} // namespace fixed_income
