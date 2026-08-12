#ifndef ITEM_H
#define ITEM_H 

#include <string>

struct Item {
    std::string description;
    std::string code;
    double price;
    bool isUnitPrice = false;
    std::string timestamp;
};

#endif