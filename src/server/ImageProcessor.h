#ifndef IMAGE_PROCESSOR_H
#define IMAGE_PROCESSOR_H

#include "common/Item.h"
#include "db/database.h"
#include "processor.pb.h"

#include <vector>
#include <functional>

class ImageProcessor {
public:
    ImageProcessor(Database* _db);
    ~ImageProcessor();

public:

public:
    std::vector<Item> processImageBytes(const std::string& filename, const std::string& imageData);
    bool confirmProcessedItems(const std::vector<Item>& items);
    bool processImagesDirectory(const std::string& receiptDir,
                                const std::function<bool(const receiptreaderproto::ProcessProgress&)>& progressCallback,
                                receiptreaderproto::ProcessComplete& complete);

private:
    static bool writeBytesToTempFile(const std::string& filename, const std::string& data, std::string& outPath);

private:
    Database* _db;
};

#endif