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

// TEST_F(ParserTest, ParseValidReceipt) {
//     auto items = parseReceiptText(sampleReceipt);
//     ASSERT_EQ(items.size(), 2);
//     EXPECT_EQ(items[0].code, "001");
//     EXPECT_DOUBLE_EQ(items[0].price, 19.99);
//     EXPECT_EQ(items[0].timestamp, "2025-01-15 10:30:45");
//     EXPECT_EQ(items[1].code, "002");
//     EXPECT_DOUBLE_EQ(items[1].price, 29.50);
//     EXPECT_EQ(items[1].timestamp, "2025-01-15 10:30:45");
// }

// TEST_F(ParserTest, ParseEmptyText) {
//     auto items = parseReceiptText(emptyReceipt);
//     EXPECT_EQ(items.size(), 0);
// }

// TEST_F(ParserTest, ParseMalformedText) {
//     auto items = parseReceiptText(malformedReceipt);
//     // Should parse the valid item
//     ASSERT_EQ(items.size(), 1);
//     EXPECT_EQ(items[0].code, "001");
//     EXPECT_DOUBLE_EQ(items[0].price, 10.00);
//     // Timestamp should be current time or empty
// }

// TEST_F(ParserTest, ParseReceiptWithoutTimestamp) {
//     std::string receipt = "001 Widget $19.99\n002 Gadget $29.50";
//     auto items = parseReceiptText(receipt);
//     ASSERT_EQ(items.size(), 2);
//     EXPECT_FALSE(items[0].timestamp.empty()); // Should have current timestamp
// }

TEST_F(ParserTest, ParseOCRLineWithSAsDollar) {
    std::string receipt = "TABLE SALT 1 627735012960 S170";
    auto items = parseReceiptText(receipt);
    ASSERT_EQ(items.size(), 1);
    EXPECT_EQ(items[0].code, "627735012960");
    EXPECT_DOUBLE_EQ(items[0].price, 170.0);
    EXPECT_FALSE(items[0].timestamp.empty());
}

TEST_F(ParserTest, ParseOCRLineWithSpaceInPrice) {
    std::string receipt = "DEM 12 GRAI 068721704470 $2 98 D";
    auto items = parseReceiptText(receipt);
    ASSERT_EQ(items.size(), 1);
    EXPECT_EQ(items[0].code, "068721704470");
    EXPECT_DOUBLE_EQ(items[0].price, 2.98);
}

TEST_F(ParserTest, ParseOCRLineWithLowercaseSOAndSuffix) {
    std::string receipt = "GARLIC 3 PK 894242000180 so 98 D";
    auto items = parseReceiptText(receipt);
    ASSERT_EQ(items.size(), 1);
    EXPECT_EQ(items[0].code, "894242000180");
    EXPECT_DOUBLE_EQ(items[0].price, 0.98);
}

TEST_F(ParserTest, ParseOCRLineWithCommaPriceAndSuffix) {
    std::string receipt = "BC CRF 078742519440 $0,06 H";
    auto items = parseReceiptText(receipt);
    ASSERT_EQ(items.size(), 1);
    EXPECT_EQ(items[0].code, "078742519440");
    EXPECT_DOUBLE_EQ(items[0].price, 0.06);
}

TEST_F(ParserTest, ParseOCRLineWithDigitSuffix) {
    std::string receipt = "ONIONS 628916409300 $1.68 0";
    auto items = parseReceiptText(receipt);
    ASSERT_EQ(items.size(), 1);
    EXPECT_EQ(items[0].code, "628916409300");
    EXPECT_DOUBLE_EQ(items[0].price, 1.68);
}

TEST_F(ParserTest, ParseOCRLineWithSectionSymbolAndSuffix) {
    std::string receipt = "AYLMER DICED 068807665610 §2.27 5";
    auto items = parseReceiptText(receipt);
    ASSERT_EQ(items.size(), 1);
    EXPECT_EQ(items[0].code, "068807665610");
    EXPECT_DOUBLE_EQ(items[0].price, 2.27);
}

TEST_F(ParserTest, WarnsOnInvalidSingleDigitPriceToken) {
    std::string receipt = "PORT PEASANT 661172018160 5 oH";
    testing::internal::CaptureStderr();
    auto items = parseReceiptText(receipt);
    std::string output = testing::internal::GetCapturedStderr();

    ASSERT_EQ(items.size(), 0);
    EXPECT_NE(output.find("Skipping item with invalid price: 5"), std::string::npos);
}

TEST_F(ParserTest, ParseOCRLineWithSingleDecimalDigit) {
    std::string receipt = "ARM HARB CHS 061120052440 $10.9 1";
    auto items = parseReceiptText(receipt);
    ASSERT_EQ(items.size(), 1);
    EXPECT_EQ(items[0].code, "061120052440");
    EXPECT_DOUBLE_EQ(items[0].price, 10.9);
}

TEST_F(ParserTest, ParseOCRLineWithOneSpaceAndSuffix) {
    std::string receipt = "QUR FINEST 628916000560 $7 36 ;";
    auto items = parseReceiptText(receipt);
    ASSERT_EQ(items.size(), 1);
    EXPECT_EQ(items[0].code, "628916000560");
    EXPECT_DOUBLE_EQ(items[0].price, 7.36);
}

TEST_F(ParserTest, ParseMultilineItemEntryWithSpacePrice) {
    std::string receipt = "BANANAS 00000040110\n2.610 kg @ $1 60 /ka $3.92 D";
    auto items = parseReceiptText(receipt);
    ASSERT_EQ(items.size(), 1);
    EXPECT_EQ(items[0].code, "00000040110");
    EXPECT_DOUBLE_EQ(items[0].price, 1.60);
    EXPECT_TRUE(items[0].isUnitPrice);
}

TEST_F(ParserTest, ParseMultilineItemEntryWithCodeSuffix) {
    std::string receipt = "YEM TOMATO C 000000046640K\n0.875 kg @ $7.20 7ka $6.30 0D";
    auto items = parseReceiptText(receipt);
    ASSERT_EQ(items.size(), 1);
    EXPECT_EQ(items[0].code, "000000046640");
    EXPECT_DOUBLE_EQ(items[0].price, 7.20);
    EXPECT_TRUE(items[0].isUnitPrice);
}

TEST_F(ParserTest, ParseMultilineItemEntryWithCommaPrice) {
    std::string receipt = "SUEET POTATO 000000040740\n2,275 ks @ $4.37 /ka $9.94 D";
    auto items = parseReceiptText(receipt);
    ASSERT_EQ(items.size(), 1);
    EXPECT_EQ(items[0].code, "000000040740");
    EXPECT_DOUBLE_EQ(items[0].price, 4.37);
    EXPECT_TRUE(items[0].isUnitPrice);
}