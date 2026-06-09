#pragma once

#include <chrono>
#include <optional>
#include <vector>

namespace fixed_income {

using sys_days = std::chrono::sys_days;
using year_month_day = std::chrono::year_month_day;

enum class Frequency {
    Annual = 1,
    SemiAnnual = 2,
    Quarterly = 4,
    Monthly = 12
};

struct CashFlow {
    sys_days date;
    double amount;
    double yearFraction;
};

struct CashFlowSchedule {
    std::vector<CashFlow> payments;

    double totalAmount() const noexcept;
    bool empty() const noexcept;
};

sys_days add_months(sys_days date, int months) noexcept;
double year_fraction(sys_days from, sys_days to) noexcept;

class Bond {
public:
    Bond(double notional,
         double couponRate,
         sys_days issueDate,
         sys_days maturityDate,
         Frequency frequency,
         sys_days settlementDate = sys_days(std::chrono::year{1970}/1/1));
    virtual ~Bond() = default;

    double notional() const noexcept;
    double couponRate() const noexcept;
    Frequency frequency() const noexcept;
    sys_days issueDate() const noexcept;
    sys_days maturityDate() const noexcept;
    sys_days settlementDate() const noexcept;
    double couponPayment() const noexcept;

    CashFlowSchedule cashFlowSchedule() const;
    double price(double ytm) const;
    double yieldToMaturity(double cleanPrice) const;
    double macaulayDuration(double ytm) const;
    double modifiedDuration(double ytm) const;
    double dv01(double ytm) const;
    double pv01(double ytm) const;
    double convexity(double ytm) const;

protected:
    virtual double redemptionAmount() const noexcept;
    virtual double discountFactor(double ytm, double yearFraction) const noexcept;
    virtual double derivativePrice(double ytm) const;

private:
    double notional_;
    double couponRate_;
    sys_days issueDate_;
    sys_days maturityDate_;
    Frequency frequency_;
    sys_days settlementDate_;
};

class TreasuryBond : public Bond {
public:
    using Bond::Bond;
};

class CorporateBond : public Bond {
public:
    CorporateBond(double notional,
                  double couponRate,
                  sys_days issueDate,
                  sys_days maturityDate,
                  Frequency frequency,
                  double creditSpread,
                  sys_days settlementDate = sys_days(std::chrono::year{1970}/1/1));

    double creditSpread() const noexcept;
    double price(double ytm) const;

private:
    double creditSpread_;
};

class FloatingRateNote : public Bond {
public:
    FloatingRateNote(double notional,
                     double spread,
                     sys_days issueDate,
                     sys_days maturityDate,
                     Frequency frequency,
                     std::vector<double> referenceRates,
                     sys_days settlementDate = sys_days(std::chrono::year{1970}/1/1));

    double spread() const noexcept;
    double price(double discountRate) const;
    double price(const std::vector<double>& forwardReferenceRates, double discountRate) const;

private:
    std::vector<double> referenceRates_;
    double spread_;
    double floatingCoupon(double referenceRate) const noexcept;
};

} // namespace fixed_income
