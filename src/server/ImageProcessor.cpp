#include "ImageProcessor.h"

#include "ocr/ocr.h"
#include "parser/parser.h"

#include <filesystem>
#include <fstream>
#include <chrono>

ImageProcessor::ImageProcessor(Database* db) : _db(db) 
{
    // Do nothing.
}

ImageProcessor::~ImageProcessor() 
{
    // Do nothing, we don't own the _db pointer.
}

std::vector<Item> ImageProcessor::processImageBytes(const std::string& filename, const std::string& imageData)
{
    std::string tempPath;
    if (!writeBytesToTempFile(filename, imageData, tempPath)) {
        return {};
    }

    std::string text = extractTextFromImage(tempPath);
    std::filesystem::remove(tempPath);
    return parseReceiptText(text);
}

bool ImageProcessor::confirmProcessedItems(const std::vector<Item>& items) 
{
    for (const auto& item : items) {
        _db->insertItem(item);
    }
    return true;
}

bool ImageProcessor::writeBytesToTempFile(const std::string& filename, const std::string& data, std::string& outPath) 
{
    try {
        std::filesystem::path tempDir = std::filesystem::temp_directory_path();
        std::string extension = std::filesystem::path(filename).extension().string();
        if (extension.empty()) {
            extension = ".png";
        }
        std::string uniqueName = "receipt_image_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + extension;
        std::filesystem::path path = tempDir / uniqueName;
        std::ofstream out(path, std::ios::binary);
        if (!out) {
            return false;
        }
        out.write(data.data(), data.size());
        out.close();
        outPath = path.string();
        return true;
    } catch (...) {
        return false;
    }
}
