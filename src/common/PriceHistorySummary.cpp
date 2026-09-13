#include "PriceHistorySummary.h"

PriceHistorySummary::PriceHistorySummary(
    std::string description,
    std::string code,
    std::string timestampFirst,
    std::string timestampLast,
    double liftimeInflationRate,
    double oneYearInflationRate,
    double sixMonthInflationRate,
    double threeMonthInflationRate,
    double oneMonthInflationRate,
    int timesPurchased,
    double minPrice,
    double maxPrice,
    double currentPrice,
    bool isUnitPrice
) :
    _description(description),
    _code(code),
    _timestampFirst(timestampFirst),
    _timestampLast(timestampLast),
    _minPrice(minPrice),
    _maxPrice(maxPrice),
    _currentPrice(currentPrice),
    _isUnitPrice(isUnitPrice),
    _liftimeInflationRate(liftimeInflationRate),
    _oneYearInflationRate(oneYearInflationRate),
    _sixMonthInflationRate(sixMonthInflationRate),
    _threeMonthInflationRate(threeMonthInflationRate),
    _oneMonthInflationRate(oneMonthInflationRate),
    _timesPurchased(timesPurchased)
{
    // Do nothing
}

std::string PriceHistorySummary::description() const { return _description; }
std::string PriceHistorySummary::code() const { return _code; }
double PriceHistorySummary::minPrice() const { return _minPrice; }
double PriceHistorySummary::maxPrice() const { return _maxPrice; }
double PriceHistorySummary::currentPrice() const { return _currentPrice; }
bool PriceHistorySummary::isUnitPrice() const { return _isUnitPrice; }
std::string PriceHistorySummary::timestampFirst() const { return _timestampFirst; }
std::string PriceHistorySummary::timestampLast() const { return _timestampLast; }
double PriceHistorySummary::liftimeInflationRate() const { return _liftimeInflationRate; }
double PriceHistorySummary::oneYearInflationRate() const { return _oneYearInflationRate; }
double PriceHistorySummary::sixMonthInflationRate() const { return _sixMonthInflationRate; }
double PriceHistorySummary::threeMonthInflationRate() const { return _threeMonthInflationRate; }
double PriceHistorySummary::oneMonthInflationRate() const { return _oneMonthInflationRate; }
int PriceHistorySummary::timesPurchased() const { return _timesPurchased; }