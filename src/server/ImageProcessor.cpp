#include "ImageProcessor.h"

#include "ocr/ocr.h"
#include "parser/parser.h"

#include <filesystem>
#include <fstream>

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

bool ImageProcessor::processImagesDirectory(const std::string& receiptDir,
                                    const std::function<bool(const receiptreaderproto::ProcessProgress&)>& progressCallback,
                                    receiptreaderproto::ProcessComplete& complete) 
{
    std::filesystem::path directory(receiptDir);
    if (!std::filesystem::exists(directory) || !std::filesystem::is_directory(directory)) {
        complete.set_success(false);
        complete.set_message("Receipt directory does not exist: " + receiptDir);
        complete.set_total_items(0);
        return false;
    }

    std::vector<std::filesystem::path> imagePaths;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(directory)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        const auto path = entry.path();
        const auto ext = path.extension().string();
        if (ext == ".jpg" || ext == ".jpeg" || ext == ".png" || ext == ".HEIC" || ext == ".heic") {
            imagePaths.push_back(path);
        }
    }

    int totalImages = static_cast<int>(imagePaths.size());
    int totalItems = 0;
    int processedImages = 0;

    for (const auto& path : imagePaths) {
        std::string text = extractTextFromImage(path.string());
        auto items = parseReceiptText(text);

        for (const auto& item : items) {
            _db->insertItem(item);
            totalItems += 1;
        }

        processedImages += 1;
        receiptreaderproto::ProcessProgress progress;
        progress.set_processed_images(processedImages);
        progress.set_total_images(totalImages);
        progress.set_current_image(path.string());
        if (!progressCallback(progress)) {
            complete.set_success(false);
            complete.set_message("Processing was interrupted.");
            complete.set_total_items(totalItems);
            return false;
        }
    }

    complete.set_success(true);
    complete.set_message("Processing complete.");
    complete.set_total_items(totalItems);
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
