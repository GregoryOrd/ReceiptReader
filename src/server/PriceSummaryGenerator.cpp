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

    return inflationRate * 100;
}

std::vector<PriceHistorySummary> PriceSummaryGenerator::generateSummaries(const std::string& code,
                                                const std::string& desc,                                            
                                                const std::string& priceMin,
                                                const std::string& priceMax,
                                                const std::string& dateStart,
                                                const std::string& dateEnd)
{
    std::vector<PriceHistorySummary> summaries;
    std::cout << "Generating summaries" << std::endl;
    if(!code.empty()) {
        std::cout << "Generating summaries Code Not Empty" << std::endl;
        summaries.push_back(fromItems(_db->queryItems(code, desc, priceMin, priceMax, dateStart, dateEnd, true)));
    }
    else
    {
        std::cout << "Generating summaries Code Empty" << std::endl;
        std::vector<std::string> codes = _db->queryDistinctItemCodes(desc, priceMin, priceMax, dateStart, dateEnd);
        std::cout << "Found " << codes.size() << " distinct codes." << std::endl;

        for(const auto& code : codes) {
            std::cout << "Querying code: " << code << std::endl;
            std::vector<Item> items = _db->queryItems(code, desc, priceMin, priceMax, dateStart, dateEnd, true);
            std::cout << "Found " << items.size() << " items." << std::endl;
            PriceHistorySummary summary = fromItems(items);
            std::cout << "Got summary for " << summary.code() << std::endl;
            summaries.push_back(summary);
            std::cout << "Pushed back summary" << std::endl;
        }
    }
    std::cout << "Generated " << summaries.size() << " summaries" << std::endl;

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
        return PriceHistorySummary("", "", "1970-01-01", "1970-01-01", 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0);
    }

    double liftimeInflationRate = calculateInflationRate(InflationRatePeriod::Lifetime, timestampSortedItems);
    double oneYearInflationRate = calculateInflationRate(InflationRatePeriod::OneYear, timestampSortedItems);
    double sixMonthInflationRate = calculateInflationRate(InflationRatePeriod::SixMonths, timestampSortedItems);
    double threeMonthInflationRate = calculateInflationRate(InflationRatePeriod::ThreeMonths, timestampSortedItems);
    double oneMonthInflationRate = calculateInflationRate(InflationRatePeriod::OneMonth, timestampSortedItems);
    
    int numItems = timestampSortedItems.size();
    double latestPriceChange = 0.0;
    if(numItems >= 2)
    {
        Item last = timestampSortedItems[numItems - 1];
        Item secondLast = timestampSortedItems[numItems - 2];
        std::cout << "Last Price: " << last.price << ", Second Last Price: " << secondLast.price << std::endl;
        latestPriceChange = last.price - secondLast.price;
        std::cout << "Lastest Price Change: " << latestPriceChange << std::endl;
    }


    double minPrice = 0.0;
    double maxPrice = 0.0;
    for(Item i : timestampSortedItems)
    {
        double itemPrice = i.price;
        if(minPrice == 0.0 || itemPrice < minPrice)
        {
            minPrice = itemPrice;
        }

        if(itemPrice > maxPrice)
        {
            maxPrice = itemPrice;
        }
    }

    return PriceHistorySummary(
        timestampSortedItems[0].description,
        timestampSortedItems[0].code,
        timestampSortedItems.front().timestamp,
        timestampSortedItems.back().timestamp,
        liftimeInflationRate,
        oneYearInflationRate,
        sixMonthInflationRate,
        threeMonthInflationRate,
        oneMonthInflationRate,
        latestPriceChange,
        timestampSortedItems.size(),
        minPrice,
        maxPrice,
        timestampSortedItems.back().price,
        timestampSortedItems[0].isUnitPrice
    );
}

PriceHistorySummary PriceSummaryGenerator::fromProto(const receiptreaderproto::PriceHistorySummary& proto) {
    return PriceHistorySummary(
        proto.description(),
        proto.code(),
        proto.timestampfirst(),
        proto.timestamplast(),
        proto.liftimeinflationrate(),
        proto.oneyearinflationrate(),
        proto.sixmonthinflationrate(),
        proto.threemonthinflationrate(),
        proto.onemonthinflationrate(),
        proto.latestpricechange(),
        proto.timespurchased(),
        proto.minprice(),
        proto.maxprice(),
        proto.currentprice(),
        proto.isunitprice()
    );
}