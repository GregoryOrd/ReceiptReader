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

// Note: For a real image test, you would need a sample image file
// TEST_F(OCRTest, ExtractTextFromValidImage) {
//     std::string result = extractTextFromImage("test/sample.png");
//     EXPECT_FALSE(result.empty());
//     // Add more assertions based on expected content
// }