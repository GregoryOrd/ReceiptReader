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

TEST_F(DatabaseTest, InsertAndQueryItem) {
    Database db(testDbPath);
    Item item{"001", 19.99, "2025-01-15 10:30:45"};
    db.insertItem(item);

    auto items = db.queryItems();
    ASSERT_EQ(items.size(), 1);
    EXPECT_EQ(items[0].code, "001");
    EXPECT_DOUBLE_EQ(items[0].price, 19.99);
    EXPECT_EQ(items[0].timestamp, "2025-01-15 10:30:45");
}

TEST_F(DatabaseTest, QueryFilteredItems) {
    Database db(testDbPath);
    Item item1{"001", 19.99, "2025-01-15 10:30:45"};
    Item item2{"002", 29.50, "2025-01-16 11:00:00"};
    db.insertItem(item1);
    db.insertItem(item2);

    auto items = db.queryItemsFiltered("001", "", "", "", "", false);
    ASSERT_EQ(items.size(), 1);
    EXPECT_EQ(items[0].code, "001");
}