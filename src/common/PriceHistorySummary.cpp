#include "PriceHistorySummary.h"

PriceHistorySummary PriceHistorySummary::fromItems(const std::vector<Item>& timestampSortedItems) {
    PriceHistorySummary summary;

    if (timestampSortedItems.empty()) {
        return summary;
    }

    summary.description = timestampSortedItems[0].description;
    summary.code = timestampSortedItems[0].code;
    summary.isUnitPrice = timestampSortedItems[0].isUnitPrice;

    summary.minPrice = timestampSortedItems[0].price;
    summary.maxPrice = timestampSortedItems[0].price;
    summary.currentPrice = timestampSortedItems.back().price;

    summary.timestampFirst = timestampSortedItems.front().timestamp;
    summary.timestampLast = timestampSortedItems.back().timestamp;

    return summary;
}

PriceHistorySummary PriceHistorySummary::fromProto(const receiptreaderproto::PriceHistorySummary& proto) {
    PriceHistorySummary summary;
    summary.description = proto.description();
    summary.code = proto.code();
    summary.minPrice = proto.minprice();
    summary.maxPrice = proto.maxprice();
    summary.currentPrice = proto.currentprice();
    summary.isUnitPrice = proto.isunitprice();
    summary.timestampFirst = proto.timestampfirst();
    summary.timestampLast = proto.timestamplast();
    summary.liftimeInflationRate = proto.liftimeinflationrate();
    summary.oneYearInflationRate = proto.oneyearinflationrate();
    summary.sixMonthInflationRate = proto.sixmonthinflationrate();
    summary.threeMonthInflationRate = proto.threemonthinflationrate();
    summary.oneMonthInflationRate = proto.onemonthinflationrate();
    return summary;
}