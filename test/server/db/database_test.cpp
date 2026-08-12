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