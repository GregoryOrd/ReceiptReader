#ifndef PriceHistorySummary_H
#define PriceHistorySummary_H 

#include "common/Item.h"
#include <vector>
#include "processor.pb.h"

class PriceHistorySummary {
public:
    PriceHistorySummary(
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
        double minPrice = 0.0,
        double maxPrice = 0.0,
        double currentPrice = 0.0,
        bool isUnitPrice = false
    );


public:
    std::string description() const;
    std::string code() const;
    double minPrice() const;
    double maxPrice() const;
    double currentPrice() const;
    bool isUnitPrice() const;
    std::string timestampFirst() const;
    std::string timestampLast() const;
    double liftimeInflationRate() const;
    double oneYearInflationRate() const;
    double sixMonthInflationRate() const;
    double threeMonthInflationRate() const;
    double oneMonthInflationRate() const;
    int timesPurchased() const;

private:
    std::string _description;
    std::string _code;

    double _minPrice = 0.0;
    double _maxPrice = 0.0;
    double _currentPrice = 0.0;
    bool _isUnitPrice = false;

    std::string _timestampFirst;
    std::string _timestampLast;

    double _liftimeInflationRate = 0.0;
    double _oneYearInflationRate = 0.0;
    double _sixMonthInflationRate = 0.0;
    double _threeMonthInflationRate = 0.0;
    double _oneMonthInflationRate = 0.0;

    int _timesPurchased;
};

#endif