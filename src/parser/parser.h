#ifndef PARSER_H
#define PARSER_H

#include <string>
#include <vector>

struct Item {
    std::string description;
    std::string code;
    double price;
    bool isUnitPrice = false;
    std::string timestamp;
};

std::vector<Item> parseReceiptText(const std::string& text);

#endif