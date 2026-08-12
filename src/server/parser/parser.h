#ifndef PARSER_H
#define PARSER_H

#include <string>
#include <vector>

#include "common/Item.h"

std::vector<Item> parseReceiptText(const std::string& text);

#endif