#ifndef PRICE_SUMMARY_GENERATOR_H
#define PRICE_SUMMARY_GENERATOR_H

#include "db/database.h"
#include "common/PriceHistorySummary.h"

enum class InflationRatePeriod {
    Lifetime,
    OneYear,
    SixMonths,
    ThreeMonths,
    OneMonth
};

class PriceSummaryGenerator
{
public:
    PriceSummaryGenerator(Database* db);
    ~PriceSummaryGenerator();

public:
    std::vector<PriceHistorySummary> generateSummaries(const std::string& code,
                                                const std::string& priceMin,
                                                const std::string& priceMax,
                                                const std::string& dateStart,
                                                const std::string& dateEnd);

    Item itemToCompare(InflationRatePeriod period, const std::vector<Item>& timestampSortedItems);
    PriceHistorySummary fromItems(const std::vector<Item>& timestampSortedItems);
    PriceHistorySummary fromProto(const receiptreaderproto::PriceHistorySummary& proto);

private:
    double calculateInflationRate(InflationRatePeriod period, const std::vector<Item>& timestampSortedItems);
    InflationCategory calculateCategory(const std::vector<Item>& timestampSortedItems);

private:
    Database* _db;
};

#endif