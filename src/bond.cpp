#include "fixed_income/bond.hpp"

#include <cmath>
#include <limits>
#include <stdexcept>
#include <type_traits>

namespace fixed_income {

static constexpr double kDayCountBasis = 365.0;
static constexpr double kNewtonRaphsonTolerance = 1e-12;
static constexpr int kNewtonRaphsonMaxIterations = 120;

sys_days add_months(sys_days date, int months) noexcept {
    const auto ymd = year_month_day(date);
    const int monthOffset = static_cast<unsigned>(ymd.month()) - 1 + months;
    const int yearShift = monthOffset / 12;
    int newMonth = monthOffset % 12 + 1;
    if (newMonth <= 0) {
        newMonth += 12;
    }

    auto candidate = year_month_day(ymd.year() + std::chrono::years(yearShift), std::chrono::month{static_cast<unsigned>(newMonth)}, ymd.day());
    if (!candidate.ok()) {
        candidate = std::chrono::year_month_day_last{candidate.year(), std::chrono::month_day_last{candidate.month()}};
    }

    return sys_days(candidate);
}

double year_fraction(sys_days from, sys_days to) noexcept {
    const auto days = std::chrono::duration_cast<std::chrono::days>(to - from).count();
    return static_cast<double>(days) / kDayCountBasis;
}

double CashFlowSchedule::totalAmount() const noexcept {
    double total = 0.0;
    for (const auto& payment : payments) {
        total += payment.amount;
    }
    return total;
}

bool CashFlowSchedule::empty() const noexcept {
    return payments.empty();
}

Bond::Bond(double notional,
           double couponRate,
           sys_days issueDate,
           sys_days maturityDate,
           Frequency frequency,
           sys_days settlementDate)
    : notional_(notional),
      couponRate_(couponRate),
      issueDate_(issueDate),
      maturityDate_(maturityDate),
      frequency_(frequency),
      settlementDate_(settlementDate >= issueDate ? settlementDate : issueDate) {
    if (!std::isfinite(notional_) || notional_ <= 0.0) {
        throw std::invalid_argument("Notional must be finite and greater than zero.");
    }
    if (!std::isfinite(couponRate_) || couponRate_ < 0.0) {
        throw std::invalid_argument("Coupon rate must be finite and non-negative.");
    }
    const int periodsPerYear = static_cast<int>(frequency_);
    if (periodsPerYear != 1 && periodsPerYear != 2 && periodsPerYear != 4 && periodsPerYear != 12) {
        throw std::invalid_argument("Unsupported coupon frequency.");
    }
    if (maturityDate_ <= issueDate_) {
        throw std::invalid_argument("Maturity date must be after issue date.");
    }
    if (settlementDate_ >= maturityDate_) {
        throw std::invalid_argument("Settlement date must be before maturity date.");
    }
}

double Bond::notional() const noexcept {
    return notional_;
}

double Bond::couponRate() const noexcept {
    return couponRate_;
}

Frequency Bond::frequency() const noexcept {
    return frequency_;
}

sys_days Bond::issueDate() const noexcept {
    return issueDate_;
}

sys_days Bond::maturityDate() const noexcept {
    return maturityDate_;
}

sys_days Bond::settlementDate() const noexcept {
    return settlementDate_;
}

double Bond::couponPayment() const noexcept {
    return notional_ * couponRate_ / static_cast<int>(frequency_);
}

CashFlowSchedule Bond::cashFlowSchedule() const {
    CashFlowSchedule schedule;
    const int periodsPerYear = static_cast<int>(frequency_);
    const int monthsPerPeriod = 12 / periodsPerYear;
    sys_days previousPaymentDate = issueDate_;
    sys_days paymentDate = add_months(issueDate_, monthsPerPeriod);
    double firstRemainingPeriod = 0.0;
    bool firstRemainingPayment = true;
    std::size_t remainingPaymentIndex = 0;

    while (paymentDate <= maturityDate_) {
        if (paymentDate > settlementDate_) {
            if (firstRemainingPayment) {
                const auto fullPeriodDays = std::chrono::duration<double, std::ratio<86400>>(
                    paymentDate - previousPaymentDate).count();
                const auto remainingDays = std::chrono::duration<double, std::ratio<86400>>(
                    paymentDate - settlementDate_).count();
                if (fullPeriodDays <= 0.0 || remainingDays <= 0.0) {
                    throw std::logic_error("Invalid coupon schedule period.");
                }
                firstRemainingPeriod = remainingDays / fullPeriodDays;
                firstRemainingPayment = false;
            }

            double amount = couponPayment();
            if (paymentDate == maturityDate_) {
                amount += redemptionAmount();
            }
            // Express time in coupon periods so fractional settlement periods are
            // discounted consistently with the bond's periodic-compounding convention.
            const double yearFraction = (firstRemainingPeriod + static_cast<double>(remainingPaymentIndex)) /
                                        static_cast<double>(periodsPerYear);
            schedule.payments.push_back(CashFlow{paymentDate, amount, yearFraction});
            ++remainingPaymentIndex;
        }
        previousPaymentDate = paymentDate;
        paymentDate = add_months(paymentDate, monthsPerPeriod);
    }
    return schedule;
}

double Bond::discountFactor(double ytm, double yearFraction) const noexcept {
    const double periodsPerYear = static_cast<int>(frequency_);
    const double perPeriodYield = ytm / periodsPerYear;
    const double exponent = yearFraction * periodsPerYear;
    return std::pow(1.0 + perPeriodYield, -exponent);
}

double Bond::price(double ytm) const {
    if (!std::isfinite(ytm) || ytm <= -static_cast<double>(static_cast<int>(frequency_))) {
        throw std::invalid_argument("Yield must be finite and above the periodic discounting limit.");
    }
    const auto schedule = cashFlowSchedule();
    double presentValue = 0.0;
    for (const auto& payment : schedule.payments) {
        presentValue += payment.amount * discountFactor(ytm, payment.yearFraction);
    }
    return presentValue;
}

double Bond::derivativePrice(double ytm) const {
    const auto schedule = cashFlowSchedule();
    const double periodsPerYear = static_cast<int>(frequency_);
    const double perPeriodYield = ytm / periodsPerYear;
    double derivative = 0.0;

    for (const auto& payment : schedule.payments) {
        const double exponent = payment.yearFraction * periodsPerYear;
        const double base = 1.0 + perPeriodYield;
        if (base <= 0.0) {
            continue;
        }
        derivative += -payment.amount * exponent * std::pow(base, -exponent - 1.0) / periodsPerYear;
    }
    return derivative;
}

double Bond::yieldToMaturity(double cleanPrice) const {
    if (cleanPrice <= 0.0) {
        throw std::invalid_argument("Clean price must be positive.");
    }

    double ytm = 0.05;
    for (int iteration = 0; iteration < kNewtonRaphsonMaxIterations; ++iteration) {
        double functionValue = price(ytm) - cleanPrice;
        double derivative = derivativePrice(ytm);
        if (std::abs(derivative) < std::numeric_limits<double>::epsilon()) {
            break;
        }
        const double update = functionValue / derivative;
        ytm -= update;
        if (std::abs(update) < kNewtonRaphsonTolerance) {
            return ytm;
        }
    }
    return ytm;
}

double Bond::macaulayDuration(double ytm) const {
    const auto schedule = cashFlowSchedule();
    const double priceValue = price(ytm);
    if (priceValue <= 0.0 || schedule.empty()) {
        return 0.0;
    }

    double weightedSum = 0.0;
    for (const auto& payment : schedule.payments) {
        const double presentValue = payment.amount * discountFactor(ytm, payment.yearFraction);
        weightedSum += payment.yearFraction * presentValue;
    }
    return weightedSum / priceValue;
}

double Bond::modifiedDuration(double ytm) const {
    const double macaulay = macaulayDuration(ytm);
    const double periodsPerYear = static_cast<int>(frequency_);
    return macaulay / (1.0 + ytm / periodsPerYear);
}

double Bond::dv01(double ytm) const {
    const double derivative = derivativePrice(ytm);
    return -derivative * 1e-4;
}

double Bond::pv01(double ytm) const {
    return dv01(ytm) / notional_;
}

double Bond::convexity(double ytm) const {
    const auto schedule = cashFlowSchedule();
    const double priceValue = price(ytm);
    if (priceValue <= 0.0 || schedule.empty()) {
        return 0.0;
    }

    const double periodsPerYear = static_cast<int>(frequency_);
    const double perPeriodYield = ytm / periodsPerYear;
    double convexitySum = 0.0;

    for (const auto& payment : schedule.payments) {
        const double exponent = payment.yearFraction * periodsPerYear;
        const double base = 1.0 + perPeriodYield;
        if (base <= 0.0) {
            continue;
        }
        convexitySum += payment.amount * exponent * (exponent + 1.0) * std::pow(base, -exponent - 2.0);
    }

    return convexitySum / (priceValue * periodsPerYear * periodsPerYear);
}

double Bond::redemptionAmount() const noexcept {
    return notional_;
}

CorporateBond::CorporateBond(double notional,
                             double couponRate,
                             sys_days issueDate,
                             sys_days maturityDate,
                             Frequency frequency,
                             double creditSpread,
                             sys_days settlementDate)
    : Bond(notional, couponRate, issueDate, maturityDate, frequency, settlementDate),
      creditSpread_(creditSpread) {
}

double CorporateBond::creditSpread() const noexcept {
    return creditSpread_;
}

double CorporateBond::price(double ytm) const {
    return Bond::price(ytm + creditSpread_);
}

FloatingRateNote::FloatingRateNote(double notional,
                                   double spread,
                                   sys_days issueDate,
                                   sys_days maturityDate,
                                   Frequency frequency,
                                   std::vector<double> referenceRates,
                                   sys_days settlementDate)
    : Bond(notional, 0.0, issueDate, maturityDate, frequency, settlementDate),
      referenceRates_(std::move(referenceRates)),
      spread_(spread) {
}

double FloatingRateNote::spread() const noexcept {
    return spread_;
}

double FloatingRateNote::floatingCoupon(double referenceRate) const noexcept {
    return referenceRate + spread_;
}

double FloatingRateNote::price(double discountRate) const {
    return price(referenceRates_, discountRate);
}

double FloatingRateNote::price(const std::vector<double>& forwardReferenceRates, double discountRate) const {
    const auto schedule = cashFlowSchedule();
    const double periodsPerYear = static_cast<int>(frequency());
    const double perPeriodDiscount = discountRate / periodsPerYear;
    double presentValue = 0.0;

    for (std::size_t idx = 0; idx < schedule.payments.size(); ++idx) {
        const auto& payment = schedule.payments[idx];
        const double referenceRate = idx < forwardReferenceRates.size() ? forwardReferenceRates[idx] : forwardReferenceRates.back();
        const double couponAmount = notional() * floatingCoupon(referenceRate) / periodsPerYear;
        const bool isMaturity = payment.date == maturityDate();
        const double amount = couponAmount + (isMaturity ? notional() : 0.0);
        const double discount = std::pow(1.0 + perPeriodDiscount, -payment.yearFraction * periodsPerYear);
        presentValue += amount * discount;
    }
    return presentValue;
}

} // namespace fixed_income
