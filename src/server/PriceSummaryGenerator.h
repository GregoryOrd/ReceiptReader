#ifndef PRICE_SUMMARY_GENERATOR_H
#define PRICE_SUMMARY_GENERATOR_H

#include "db/database.h"
#include "common/PriceHistorySummary.h"

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
private:
    Database* _db;
};

#endif