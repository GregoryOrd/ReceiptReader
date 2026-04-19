#include "parser.h"
#include <algorithm>
#include <iostream>
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
    std::regex itemRegex(R"(^\s*(.+?)\s+(\d{12})\s+([\$Ss][0-9Oo]+([.,][0-9Oo]{2}| [0-9Oo]{2})?)(\s+(\S+))?\s*$)");
    std::regex itemGenericRegex(R"(^\s*(.+?)\s+(\d{12})\s+(\S+)(\s+(\S+))?\s*$)");
    std::regex itemLine1Regex(R"(^\s*(.+?)\s+([0-9]{11,12})([A-Za-z]?)\s*$)");
    std::regex priceRegex(R"((([\$Ss]|§)[0-9Oo]+([.,][0-9Oo]+| [0-9Oo]+)?)|([0-9Oo]+([.,][0-9Oo]+| [0-9Oo]+)))");

    auto normalizeLine = [&](std::string source) {
        std::string normalized = std::move(source);
        std::string sectionSign = "§";
        size_t pos = 0;
        while ((pos = normalized.find(sectionSign, pos)) != std::string::npos) {
            normalized.replace(pos, sectionSign.size(), "$" );
            pos += 1;
        }
        return normalized;
    };

    auto normalizePrice = [&](std::string priceStr, double& out) {
        if (priceStr.empty()) {
            return false;
        }
        bool hasCurrencyPrefix = false;
        if (priceStr[0] == 'S' || priceStr[0] == 's' || priceStr[0] == '$' || priceStr.rfind("§", 0) == 0) {
            hasCurrencyPrefix = true;
            priceStr = priceStr.substr(1);
        }
        for (auto& c : priceStr) {
            if (c == 'O' || c == 'o') {
                c = '0';
            }
            if (c == ',') {
                c = '.';
            }
        }
        if (priceStr.find(' ') != std::string::npos && priceStr.find('.') == std::string::npos) {
            auto dotPos = priceStr.find(' ');
            priceStr[dotPos] = '.';
        }
        bool hasDecimal = priceStr.find('.') != std::string::npos;
        priceStr.erase(std::remove(priceStr.begin(), priceStr.end(), ' '), priceStr.end());
        if (!hasCurrencyPrefix && !hasDecimal) {
            return false;
        }
        std::regex validPrice(R"(^\d+(?:\.\d+)?$)");
        if (!std::regex_match(priceStr, validPrice)) {
            return false;
        }
        try {
            out = std::stod(priceStr);
            return true;
        } catch (...) {
            return false;
        }
    };

    auto findUnitPriceToken = [&](const std::string& line, const std::vector<std::string>& tokens) {
        auto atPos = line.find('@');
        if (atPos != std::string::npos) {
            for (const auto& token : tokens) {
                auto tokenPos = line.find(token);
                if (tokenPos != std::string::npos && tokenPos > atPos) {
                    return token;
                }
            }
        }
        if (!tokens.empty()) {
            return tokens.front();
        }
        return std::string();
    };

    std::vector<std::string> lines;
    while (std::getline(iss, line)) {
        lines.push_back(line);
    }

    for (size_t i = 0; i < lines.size(); ++i) {
        const std::string& originalLine = lines[i];
        std::string currentLine = normalizeLine(originalLine);
        std::smatch match;
        if (std::regex_search(currentLine, match, itemRegex)) {
            double priceValue;
            if (normalizePrice(match[3].str(), priceValue)) {
                Item item;
                item.description = match[1];
                item.code = match[2];
                item.price = priceValue;
                item.timestamp = getCurrentTimestamp();
                items.push_back(item);
            } else {
                std::cerr << "Skipping item with invalid price: " << match[3].str() << std::endl;
            }
        } else if (std::regex_search(currentLine, match, itemGenericRegex)) {
            double priceValue;
            if (normalizePrice(match[3].str(), priceValue)) {
                Item item;
                item.description = match[1];
                item.code = match[2];
                item.price = priceValue;
                item.timestamp = getCurrentTimestamp();
                items.push_back(item);
            } else {
                std::cerr << "Skipping item with invalid price: " << match[3].str() << std::endl;
            }
        } else if (std::regex_search(currentLine, match, itemLine1Regex) && i + 1 < lines.size()) {
            std::string nextLine = normalizeLine(lines[i + 1]);
            std::vector<std::string> priceTokens;
            for (std::sregex_iterator iter(nextLine.begin(), nextLine.end(), priceRegex), end; iter != end; ++iter) {
                priceTokens.push_back(iter->str());
            }
            if (!priceTokens.empty()) {
                std::string priceToken = findUnitPriceToken(nextLine, priceTokens);
                double priceValue;
                if (!priceToken.empty() && normalizePrice(priceToken, priceValue)) {
                    Item item;
                    item.description = match[1];
                    item.code = match[2];
                    item.price = priceValue;
                    item.isUnitPrice = nextLine.find('@') != std::string::npos;
                    item.timestamp = getCurrentTimestamp();
                    items.push_back(item);
                    ++i;
                } else {
                    std::cerr << "Skipping multiline item with invalid price: " << priceToken << std::endl;
                }
            } else if (!currentLine.empty()) {
                std::cerr << "Skipping unrecognized OCR line: " << currentLine << std::endl;
            }
        } else if (!currentLine.empty()) {
            std::cerr << "Skipping unrecognized OCR line: " << currentLine << std::endl;
        }
    }
    return items;
}

std::string getCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::tm tm = *std::localtime(&time);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d");
    return oss.str();
}