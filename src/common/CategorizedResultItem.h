#ifndef CATEGORIZEDRESULTITEM_H
#define CATEGORIZEDRESULTITEM_H 

#include "common/Item.h"

class CategorizedResultItem {
public:
    CategorizedResultItem() = default;
    CategorizedResultItem(const Item& item, int category = 0)
        : description(item.description),
          code(item.code),
          price(item.price),    
          isUnitPrice(item.isUnitPrice),
          timestamp(item.timestamp),
          category(category) {}   

public:
    std::string description;
    std::string code;
    double price;
    bool isUnitPrice = false;
    std::string timestamp;
    int category = 0;
};

#endif