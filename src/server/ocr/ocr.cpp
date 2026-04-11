#include "ocr.h"
#include <opencv2/opencv.hpp>
#include <tesseract/baseapi.h>
#include <leptonica/allheaders.h>
#include <iostream>
#include <filesystem>
#include <cstdlib>

namespace fs = std::filesystem;

const int GRID_SIZE = 10; // 10x10 grid

void fillGridWhite(int row, int col, cv::Mat* img) {
    int rowSize = img->rows / GRID_SIZE;
    int colSize = img->cols / GRID_SIZE;

    for(int i = row; i < row + rowSize && i < img->rows; i++) {
        for(int j = col; j < col + colSize && j < img->cols; j++) {
            img->at<cv::Vec3b>(i, j) = cv::Vec3b(255, 255, 255); // White grid
        }
    }
}

bool gridSquareMostlyOneColour(int row, int col, cv::Mat* img) {
    int whiteCount = 0;
    int totalCount = 0;

    int rowSize = img->rows / GRID_SIZE;
    int colSize = img->cols / GRID_SIZE;

    for(int i = row; i < row + rowSize && i < img->rows; i++) {
        for(int j = col; j < col + colSize && j < img->cols; j++) {
            if (img->at<cv::Vec3b>(i, j) == cv::Vec3b(255, 255, 255)) {
                whiteCount++;
            }
            totalCount++;
        }
    }

    
    double whiteRatio = (double)whiteCount / totalCount;
    double maxRatio = 0.95; // Consider it mostly one color if more than 95% is one color
    return whiteCount == totalCount || whiteRatio > maxRatio || whiteRatio < (1 - maxRatio); // Consider it mostly one color if more than 95% or less than 5%
}

void drawGrid(cv::Mat* img, const std::string& filename) {
    std::cout << "Draw Grid with image of size: " << img->cols << "x" << img->rows << std::endl;

    int rowSize = img->rows / GRID_SIZE;
    int colSize = img->cols / GRID_SIZE;

    // Draw horizontal grid lines
    for(int i = 0; i < img->rows - rowSize; i += rowSize) {
        for(int j = 0; j < img->cols - colSize; j++) {
            img->at<cv::Vec3b>(i, j) = cv::Vec3b(0, 255, 0); // Green grid
        }
    }

    // Draw vertical grid lines
    for(int i = 0; i < img->cols - colSize; i += colSize) {
        for(int j = 0; j < img->rows - rowSize; j++) {
            img->at<cv::Vec3b>(j, i) = cv::Vec3b(0, 255, 0); // Green grid
        }
    }

    cv::imwrite(filename, *img);
}

void whiteOut(cv::Mat* img) {
    int rowSize = img->rows / GRID_SIZE;
    int colSize = img->cols / GRID_SIZE;

    for(int i = 0; i < img->rows - rowSize; i += rowSize) {
        for(int j = 0; j < img->cols - colSize; j += colSize) {
            if(gridSquareMostlyOneColour(i, j, img)) {
                fillGridWhite(i, j, img);
            }
        }
    }

    cv::imwrite("white_overlay.png", *img);
}

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

    // //Save the thresholded image for debugging
    cv::imwrite("thresh.png", thresh);


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