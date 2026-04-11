#include <gtest/gtest.h>
#include "ocr/ocr.h"

class OCRTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup if needed
    }

    void TearDown() override {
        // Cleanup if needed
    }
};

TEST_F(OCRTest, ExtractTextFromInvalidImagePath) {
    std::string result = extractTextFromImage("nonexistent.png");
    EXPECT_EQ(result, "");
}

TEST_F(OCRTest, ExtractTextFromEmptyImagePath) {
    std::string result = extractTextFromImage("");
    EXPECT_EQ(result, "");
}

TEST_F(OCRTest, ExtractTextFromValidImagePath) {
    std::string result = extractTextFromImage("../imgs/Receipt1/IMG_3397.HEIC");
    std::cout << "Extracted text: " << result << std::endl;
    EXPECT_EQ(result, "");
}