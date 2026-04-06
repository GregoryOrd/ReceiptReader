#ifndef PARSER_H
#define PARSER_H

#include <string>
#include <vector>

struct Item {
    std::string code;
    double price;
    std::string timestamp;
};

std::vector<Item> parseReceiptText(const std::string& text);

#endif