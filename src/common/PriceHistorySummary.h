#ifndef PriceHistorySummary_H
#define PriceHistorySummary_H 

#include "common/Item.h"
#include "common/InflationCategory.h"
#include <vector>
#include "processor.pb.h"

class PriceHistorySummary {
public:
    PriceHistorySummary(
        std::string description,
        std::string code,
        std::string timestampFirst,
        std::string timestampLast,
        double minPrice = 0.0,
        double maxPrice = 0.0,
        double currentPrice = 0.0,
        bool isUnitPrice = false,
        InflationCategory category = InflationCategory::NORMAL
    );

//TODO: Make these private and add getters
public:
    std::string _description;
    std::string _code;

    double _minPrice = 0.0;
    double _maxPrice = 0.0;
    double _currentPrice = 0.0;
    bool _isUnitPrice = false;

    std::string _timestampFirst;
    std::string _timestampLast;

    InflationCategory _category;
};

#endif