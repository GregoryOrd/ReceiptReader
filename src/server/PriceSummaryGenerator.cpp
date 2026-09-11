#include "PriceSummaryGenerator.h"

#include <QDateTime>

PriceSummaryGenerator::PriceSummaryGenerator(Database* db) : _db(db) 
{
    // Do nothing
}

PriceSummaryGenerator::~PriceSummaryGenerator()
{
    // Do nothing, we don't own the _db pointer
}

double PriceSummaryGenerator::calculateInflationRate(InflationRatePeriod period, const std::vector<Item>& timestampSortedItems) {
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

InflationCategory PriceSummaryGenerator::calculateCategory(const std::vector<Item>& timestampSortedItems)
{
    double liftimeInflationRate = calculateInflationRate(InflationRatePeriod::Lifetime, timestampSortedItems);
    double oneYearInflationRate = calculateInflationRate(InflationRatePeriod::OneYear, timestampSortedItems);
    double sixMonthInflationRate = calculateInflationRate(InflationRatePeriod::SixMonths, timestampSortedItems);
    double threeMonthInflationRate = calculateInflationRate(InflationRatePeriod::ThreeMonths, timestampSortedItems);
    double oneMonthInflationRate = calculateInflationRate(InflationRatePeriod::OneMonth, timestampSortedItems);

    InflationCategory cat;
    if(threeMonthInflationRate <= 2.0) {
        cat = InflationCategory::LOW;
    } else if(threeMonthInflationRate <= 4.0) {
        cat = InflationCategory::NORMAL;
    } else if(threeMonthInflationRate <= 6.0) {
        cat = InflationCategory::HIGH;
    } else {
        cat = InflationCategory::NORMAL;
    }

    return cat;
}

std::vector<PriceHistorySummary> PriceSummaryGenerator::generateSummaries(const std::string& code,
                                                const std::string& priceMin,
                                                const std::string& priceMax,
                                                const std::string& dateStart,
                                                const std::string& dateEnd)
{
    std::vector<PriceHistorySummary> summaries;
    if(!code.empty()) {
        summaries.push_back(fromItems(_db->queryItems(code, priceMin, priceMax, dateStart, dateEnd, true)));
    }
    else
    {
        std::vector<std::string> codes = _db->queryDistinctItemCodes(priceMin, priceMax, dateStart, dateEnd);

        for(const auto& code : codes) {
            summaries.push_back(fromItems(_db->queryItems(code, priceMin, priceMax, dateStart, dateEnd, true)));
        }
    }

    return summaries;    
}

Item PriceSummaryGenerator::itemToCompare(InflationRatePeriod period, const std::vector<Item>& timestampSortedItems) {
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


PriceHistorySummary PriceSummaryGenerator::fromItems(const std::vector<Item>& timestampSortedItems) {
    if (timestampSortedItems.empty()) {
        return PriceHistorySummary("", "", "1970-01-01", "1970-01-01");
    }

    //TODO: Use current value for maxPrice
    return PriceHistorySummary(
        timestampSortedItems[0].description,
        timestampSortedItems[0].code,
        timestampSortedItems.front().timestamp,
        timestampSortedItems.back().timestamp,
        timestampSortedItems[0].price,
        timestampSortedItems[0].price,
        timestampSortedItems.back().price,
        timestampSortedItems[0].isUnitPrice,
        calculateCategory(timestampSortedItems)
    );
}

PriceHistorySummary PriceSummaryGenerator::fromProto(const receiptreaderproto::PriceHistorySummary& proto) {
    return PriceHistorySummary(
        proto.description(),
        proto.code(),
        proto.timestampfirst(),
        proto.timestamplast(),
        proto.minprice(),
        proto.maxprice(),
        proto.currentprice(),
        proto.isunitprice(),
        inflationCatFromProto(proto.category())
    );
}