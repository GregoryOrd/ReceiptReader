#include "parser.h"
#include <regex>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <ctime>

static std::string getCurrentTimestamp();

std::vector<Item> parseReceiptText(const std::string& text) {
    std::vector<Item> items;
    std::istringstream iss(text);
    std::string line;
    std::string timestamp = "";
    std::regex dateRegex(R"(\b(\d{1,2})/(\d{1,2})/(\d{2})\s+(\d{1,2}):(\d{2}):(\d{2})\b)");
    std::regex itemRegex(R"(\b(\d+)\s+(.+?)\s+(\d+\.\d{2})\b)");

    while (std::getline(iss, line)) {
        std::smatch match;
        if (std::regex_search(line, match, dateRegex)) {
            // Assume year is 20YY
            int month = std::stoi(match[1]);
            int day = std::stoi(match[2]);
            int year = 2000 + std::stoi(match[3]);
            int hour = std::stoi(match[4]);
            int min = std::stoi(match[5]);
            int sec = std::stoi(match[6]);
            std::ostringstream oss;
            oss << std::setfill('0') << std::setw(4) << year << "-"
                << std::setw(2) << month << "-"
                << std::setw(2) << day << " "
                << std::setw(2) << hour << ":"
                << std::setw(2) << min << ":"
                << std::setw(2) << sec;
            timestamp = oss.str();
        } else if (std::regex_search(line, match, itemRegex)) {
            Item item;
            item.code = match[1];
            item.price = std::stod(match[3]);
            item.timestamp = timestamp.empty() ? getCurrentTimestamp() : timestamp;
            items.push_back(item);
        }
    }
    return items;
}

std::string getCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::tm tm = *std::localtime(&time);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}