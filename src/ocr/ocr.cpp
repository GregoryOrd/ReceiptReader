#include "ocr.h"
#include <opencv2/opencv.hpp>
#include <tesseract/baseapi.h>
#include <leptonica/allheaders.h>
#include <iostream>
#include <filesystem>
#include <cstdlib>

namespace fs = std::filesystem;

std::string extractTextFromImage(const std::string& imagePath) {
    // Check if HEIC, convert to PNG
    std::string processedPath = imagePath;
    if (imagePath.find(".HEIC") != std::string::npos || imagePath.find(".heic") != std::string::npos) {
        processedPath = imagePath + ".png";
        std::string cmd = "convert \"" + imagePath + "\" \"" + processedPath + "\"";
        system(cmd.c_str());
    }

    // Load image with OpenCV
    cv::Mat image = cv::imread(processedPath, cv::IMREAD_COLOR);
    if (image.empty()) {
        std::cerr << "Could not open or find the image: " << processedPath << std::endl;
        return "";
    }

    // Preprocess: grayscale, threshold
    cv::Mat gray;
    cv::cvtColor(image, gray, cv::COLOR_BGR2GRAY);
    cv::Mat thresh;
    cv::threshold(gray, thresh, 0, 255, cv::THRESH_BINARY | cv::THRESH_OTSU);

    // OCR
    tesseract::TessBaseAPI* api = new tesseract::TessBaseAPI();
    if (api->Init(NULL, "eng")) {
        std::cerr << "Could not initialize tesseract." << std::endl;
        return "";
    }
    api->SetImage(thresh.data, thresh.cols, thresh.rows, 1, thresh.step);
    std::string text = std::string(api->GetUTF8Text());
    api->End();
    delete api;

    // Clean up temp file if converted
    if (processedPath != imagePath) {
        fs::remove(processedPath);
    }

    return text;
}