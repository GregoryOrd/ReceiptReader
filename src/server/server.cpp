#include "server.h"
#include "ocr/ocr.h"
#include "parser/parser.h"
#include <filesystem>
#include <fstream>
#include <chrono>

namespace fs = std::filesystem;

Server::Server(const std::string& dbPath)
    : _db(std::make_unique<Database>(dbPath)), 
      _imageProcessor(std::make_unique<ImageProcessor>(_db.get())),
      _summaryGenerator(std::make_unique<PriceSummaryGenerator>(_db.get())) {
}

bool Server::initialize() {
    return _db->createTableIfNotExists();
}

std::vector<PriceHistorySummary> Server::queryPriceHistorySummaries(const std::string& code,
                                             const std::string& priceMin,
                                             const std::string& priceMax,
                                             const std::string& dateStart,
                                             const std::string& dateEnd) {  
    
    return _summaryGenerator->generateSummaries(code, priceMin, priceMax, dateStart, dateEnd);
}

std::vector<Item> Server::queryItemCode(const std::string& code) {
    return _db->queryItems(code);
}

std::vector<Item> Server::processImageBytes(const std::string& filename, const std::string& imageData) {
    return _imageProcessor->processImageBytes(filename, imageData);
}

bool Server::confirmProcessedItems(const std::vector<Item>& items) {
    return _imageProcessor->confirmProcessedItems(items);
}

bool Server::processImagesDirectory(const std::string& receiptDir,
                                    const std::function<bool(const receiptreaderproto::ProcessProgress&)>& progressCallback,
                                    receiptreaderproto::ProcessComplete& complete) {
    return _imageProcessor->processImagesDirectory(receiptDir, progressCallback, complete);
}