#ifndef SERVER_H
#define SERVER_H

#include "db/database.h"
#include "parser/parser.h"
#include "processor.pb.h"
#include "common/PriceHistorySummary.h"
#include "ImageProcessor.h"
#include "PriceSummaryGenerator.h"

#include <functional>
#include <string>
#include <vector>

class Server {
public:
    explicit Server(const std::string& dbPath);
    bool initialize();

    std::vector<PriceHistorySummary> queryPriceHistorySummaries(const std::string& code,
                                         const std::string& priceMin,
                                         const std::string& priceMax,
                                         const std::string& dateStart,
                                         const std::string& dateEnd);

    std::vector<Item> queryItemCode(const std::string& code);

    std::vector<Item> processImageBytes(const std::string& filename, const std::string& imageData);
    bool confirmProcessedItems(const std::vector<Item>& items);
    bool processImagesDirectory(const std::string& receiptDir,
                                const std::function<bool(const receiptreaderproto::ProcessProgress&)>& progressCallback,
                                receiptreaderproto::ProcessComplete& complete);

private:
    std::unique_ptr<Database> _db;
    std::unique_ptr<ImageProcessor> _imageProcessor;
    std::unique_ptr<PriceSummaryGenerator> _summaryGenerator;
};

#endif // SERVER_H
