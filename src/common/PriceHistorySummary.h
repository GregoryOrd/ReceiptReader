#ifndef PriceHistorySummary_H
#define PriceHistorySummary_H 

#include "common/Item.h"
#include <vector>
#include "processor.pb.h"

class PriceHistorySummary {
public:
    PriceHistorySummary() = default;

    static PriceHistorySummary fromItems(const std::vector<Item>& timestampSortedItems);
    static PriceHistorySummary fromProto(const receiptreaderproto::PriceHistorySummary& proto);

public:
    std::string description;
    std::string code;

    double minPrice = 0.0;
    double maxPrice = 0.0;
    double currentPrice = 0.0;
    bool isUnitPrice = false;

    std::string timestampFirst;
    std::string timestampLast;

    double liftimeInflationRate = 0.0;
    double oneYearInflationRate = 0.0;
    double sixMonthInflationRate = 0.0;
    double threeMonthInflationRate = 0.0;
    double oneMonthInflationRate = 0.0;
};

#endif