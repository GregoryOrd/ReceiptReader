#include <gtest/gtest.h>
#include "db/database.h"
#include <filesystem>

class DatabaseTest : public ::testing::Test {
protected:
    std::string testDbPath = "test_receipts.db";

    void SetUp() override {
        // Remove test db if exists
        std::filesystem::remove(testDbPath);
    }

    void TearDown() override {
        // Clean up test db
        std::filesystem::remove(testDbPath);
    }
};

TEST_F(DatabaseTest, CreateTable) {
    Database db(testDbPath);
    // Table should be created automatically
    SUCCEED(); // If no exception, it's good
}

TEST_F(DatabaseTest, InsertAndQueryItemWithDescription) {
    Database db(testDbPath);
    Item item{"Bananas", "001", 19.99, false, "2025-01-15"};
    db.insertItem(item);

    auto items = db.queryItems();
    ASSERT_EQ(items.size(), 1);
    EXPECT_EQ(items[0].description, "Bananas");
    EXPECT_EQ(items[0].code, "001");
    EXPECT_DOUBLE_EQ(items[0].price, 19.99);
    EXPECT_FALSE(items[0].isUnitPrice);
    EXPECT_EQ(items[0].timestamp, "2025-01-15");
}

TEST_F(DatabaseTest, InsertAndQueryUnitPriceFlag) {
    Database db(testDbPath);
    Item item{"Bananas", "001", 1.60, true, "2025-01-15"};
    db.insertItem(item);

    auto items = db.queryItems();
    ASSERT_EQ(items.size(), 1);
    EXPECT_EQ(items[0].code, "001");
    EXPECT_DOUBLE_EQ(items[0].price, 1.60);
    EXPECT_TRUE(items[0].isUnitPrice);
}

TEST_F(DatabaseTest, DuplicateItemKeepsHighestPrice) {
    Database db(testDbPath);
    Item first{"Bananas", "001", 19.99, false, "2025-01-15"};
    Item second{"Bananas", "001", 20.50, false, "2025-01-15"};
    db.insertItem(first);
    db.insertItem(second);

    auto items = db.queryItems();
    ASSERT_EQ(items.size(), 1);
    EXPECT_EQ(items[0].code, "001");
    EXPECT_DOUBLE_EQ(items[0].price, 20.50);
}