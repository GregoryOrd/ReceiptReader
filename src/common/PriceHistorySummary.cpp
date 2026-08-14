#include "PriceHistorySummary.h"
#include <QDateTime>

Item PriceHistorySummary::itemToCompare(InflationRatePeriod period, const std::vector<Item>& timestampSortedItems) {
    QDateTime now = QDateTime::currentDateTime();
    QDateTime targetTimestamp;
    switch (period) {
        case InflationRatePeriod::Lifetime:
            targetTimestamp = QDateTime::fromString(QString::fromStdString(timestampSortedItems.front().timestamp), Qt::ISODate);
            break;
        case InflationRatePeriod::OneYear:
            targetTimestamp = now.addYears(-1);
            break;
        case InflationRatePeriod::SixMonths:
            targetTimestamp = now.addMonths(-6);
            break;
        case InflationRatePeriod::ThreeMonths:
            targetTimestamp = now.addMonths(-3);
            break;
        case InflationRatePeriod::OneMonth:
            targetTimestamp = now.addMonths(-1);
            break;
        default:
            return Item();
    }


    for (const auto& item : timestampSortedItems) {
        QDateTime itemTimestamp = QDateTime::fromString(QString::fromStdString(item.timestamp), Qt::ISODate);
        if (itemTimestamp >= targetTimestamp) {
            return item;
        }
    }

    return Item();
}

double PriceHistorySummary::calculateInflationRate(InflationRatePeriod period, const std::vector<Item>& timestampSortedItems) {
    if (timestampSortedItems.size() < 2) {
        return 0.0;
    }

    const Item& firstItem = itemToCompare(period, timestampSortedItems);
    if(firstItem.price == 0.0) {
        return 0.0;
    }

    const Item& lastItem = timestampSortedItems.back();

    double daysBetween = (double)abs(QDateTime::fromString(QString::fromStdString(lastItem.timestamp), Qt::ISODate)
                        .daysTo(QDateTime::fromString(QString::fromStdString(firstItem.timestamp), Qt::ISODate)));
    if (daysBetween == 0.0) {
        return 0.0;
    }


    double priceChange = lastItem.price - firstItem.price;
    double priceChangePerDay = priceChange / static_cast<double>(daysBetween);
    double priceChangePerYear = priceChangePerDay * 365.0;
    double inflationRate = priceChangePerYear / firstItem.price;

    return inflationRate;
}

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

    summary.liftimeInflationRate = calculateInflationRate(InflationRatePeriod::Lifetime, timestampSortedItems);
    summary.oneYearInflationRate = calculateInflationRate(InflationRatePeriod::OneYear, timestampSortedItems);
    summary.sixMonthInflationRate = calculateInflationRate(InflationRatePeriod::SixMonths, timestampSortedItems);
    summary.threeMonthInflationRate = calculateInflationRate(InflationRatePeriod::ThreeMonths, timestampSortedItems);
    summary.oneMonthInflationRate = calculateInflationRate(InflationRatePeriod::OneMonth, timestampSortedItems);

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