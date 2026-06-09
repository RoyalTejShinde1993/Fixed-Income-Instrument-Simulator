#include "fixed_income/bond.hpp"
#include "fixed_income/fix.hpp"
#include "fixed_income/network.hpp"
#include "fixed_income/yield_curve.hpp"

#include <boost/asio.hpp>
#include <iostream>

using namespace fixed_income;

int main() {
    try {
        Bond bond(1000.0, 0.05, std::chrono::sys_days(std::chrono::year{2024}/1/1),
                  std::chrono::sys_days(std::chrono::year{2029}/1/1), Frequency::SemiAnnual,
                  std::chrono::sys_days(std::chrono::year{2024}/1/1));

        const double price = bond.price(0.05);
        const double ytm = bond.yieldToMaturity(price);
        std::cout << "Bond price: " << price << "\n";
        std::cout << "Yield to maturity: " << ytm << "\n";
        std::cout << "Duration: " << bond.modifiedDuration(ytm) << "\n";

        boost::asio::io_context ioContext;
        FixSessionManager manager("DEMO_SENDER", "DEMO_TARGET");
        FixClient client(ioContext, manager);
        client.connect("127.0.0.1", 15001);
        std::cout << "Demo initialized. Run a local FIX server on port 15001 to connect.\n";
    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << "\n";
        return 1;
    }
    return 0;
}
