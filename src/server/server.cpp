#include "server.h"
#include "ocr/ocr.h"
#include "parser/parser.h"
#include <filesystem>
#include <fstream>
#include <chrono>

namespace fs = std::filesystem;

Server::Server(const std::string& dbPath)
    : m_db(dbPath) {
}

bool Server::initialize() {
    return m_db.createTableIfNotExists();
}

std::vector<Item> Server::queryItemsFiltered(const std::string& code,
                                             const std::string& priceMin,
                                             const std::string& priceMax,
                                             const std::string& dateStart,
                                             const std::string& dateEnd,
                                             bool orderByTimestamp) {
    return m_db.queryItemsFiltered(code, priceMin, priceMax, dateStart, dateEnd, orderByTimestamp);
}

std::vector<Item> Server::processImageBytes(const std::string& filename, const std::string& imageData) {
    std::string tempPath;
    if (!writeBytesToTempFile(filename, imageData, tempPath)) {
        return {};
    }

    std::string text = extractTextFromImage(tempPath);
    fs::remove(tempPath);
    return parseReceiptText(text);
}

bool Server::confirmProcessedItems(const std::vector<Item>& items) {
    for (const auto& item : items) {
        m_db.insertItem(item);
    }
    return true;
}

bool Server::processImagesDirectory(const std::string& receiptDir,
                                    const std::function<bool(const receiptreader::ProcessProgress&)>& progressCallback,
                                    receiptreader::ProcessComplete& complete) {
    fs::path directory(receiptDir);
    if (!fs::exists(directory) || !fs::is_directory(directory)) {
        complete.set_success(false);
        complete.set_message("Receipt directory does not exist: " + receiptDir);
        complete.set_total_items(0);
        return false;
    }

    std::vector<fs::path> imagePaths;
    for (const auto& entry : fs::recursive_directory_iterator(directory)) {
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
            m_db.insertItem(item);
            totalItems += 1;
        }

        processedImages += 1;
        receiptreader::ProcessProgress progress;
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

bool Server::writeBytesToTempFile(const std::string& filename, const std::string& data, std::string& outPath) {
    try {
        fs::path tempDir = fs::temp_directory_path();
        std::string extension = fs::path(filename).extension().string();
        if (extension.empty()) {
            extension = ".png";
        }
        std::string uniqueName = "receipt_image_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + extension;
        fs::path path = tempDir / uniqueName;
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
