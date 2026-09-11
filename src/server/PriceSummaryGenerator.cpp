#include "PriceSummaryGenerator.h"

PriceSummaryGenerator::PriceSummaryGenerator(Database* db) : _db(db) 
{
    // Do nothing
}

PriceSummaryGenerator::~PriceSummaryGenerator()
{
    // Do nothing, we don't own the _db pointer
}

std::vector<PriceHistorySummary> PriceSummaryGenerator::generateSummaries(const std::string& code,
                                                const std::string& priceMin,
                                                const std::string& priceMax,
                                                const std::string& dateStart,
                                                const std::string& dateEnd)
{
    std::vector<PriceHistorySummary> summaries;
    if(!code.empty()) {
        summaries.push_back(PriceHistorySummary::fromItems(_db->queryItems(code, priceMin, priceMax, dateStart, dateEnd, true)));
    }
    else
    {
        std::vector<std::string> codes = _db->queryDistinctItemCodes(priceMin, priceMax, dateStart, dateEnd);

        for(const auto& code : codes) {
            summaries.push_back(PriceHistorySummary::fromItems(_db->queryItems(code, priceMin, priceMax, dateStart, dateEnd, true)));
        }
    }

    return summaries;    
}