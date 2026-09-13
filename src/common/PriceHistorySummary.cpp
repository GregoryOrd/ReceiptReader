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
    double minPrice,
    double maxPrice,
    double currentPrice,
    bool isUnitPrice,
    InflationCategory category
) :
    _description(description),
    _code(code),
    _timestampFirst(timestampFirst),
    _timestampLast(timestampLast),
    _minPrice(minPrice),
    _maxPrice(maxPrice),
    _currentPrice(currentPrice),
    _isUnitPrice(isUnitPrice),
    _category(category),
    _liftimeInflationRate(liftimeInflationRate),
    _oneYearInflationRate(oneYearInflationRate),
    _sixMonthInflationRate(sixMonthInflationRate),
    _threeMonthInflationRate(threeMonthInflationRate),
    _oneMonthInflationRate(oneMonthInflationRate)
{

}