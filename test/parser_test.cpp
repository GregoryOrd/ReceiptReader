#include <gtest/gtest.h>
#include "parser/parser.h"

class ParserTest : public ::testing::Test {
protected:
    std::string sampleReceipt = R"(
01/15/25 10:30:45
001 Widget $19.99
002 Gadget $29.50
)";

    std::string emptyReceipt = "";
    std::string malformedReceipt = "Invalid line\n001 Item $10.00";
};

TEST_F(ParserTest, ParseValidReceipt) {
    auto items = parseReceiptText(sampleReceipt);
    ASSERT_EQ(items.size(), 2);
    EXPECT_EQ(items[0].code, "001");
    EXPECT_DOUBLE_EQ(items[0].price, 19.99);
    EXPECT_EQ(items[0].timestamp, "2025-01-15 10:30:45");
    EXPECT_EQ(items[1].code, "002");
    EXPECT_DOUBLE_EQ(items[1].price, 29.50);
    EXPECT_EQ(items[1].timestamp, "2025-01-15 10:30:45");
}

TEST_F(ParserTest, ParseEmptyText) {
    auto items = parseReceiptText(emptyReceipt);
    EXPECT_EQ(items.size(), 0);
}

TEST_F(ParserTest, ParseMalformedText) {
    auto items = parseReceiptText(malformedReceipt);
    // Should parse the valid item
    ASSERT_EQ(items.size(), 1);
    EXPECT_EQ(items[0].code, "001");
    EXPECT_DOUBLE_EQ(items[0].price, 10.00);
    // Timestamp should be current time or empty
}

TEST_F(ParserTest, ParseReceiptWithoutTimestamp) {
    std::string receipt = "001 Widget $19.99\n002 Gadget $29.50";
    auto items = parseReceiptText(receipt);
    ASSERT_EQ(items.size(), 2);
    EXPECT_FALSE(items[0].timestamp.empty()); // Should have current timestamp
}