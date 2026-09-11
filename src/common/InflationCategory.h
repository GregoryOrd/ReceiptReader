#ifndef INFLATION_CAT_H
#define INFLATION_CAT_H

#include "processor.pb.h"

enum InflationCategory {
    LOW,
    NORMAL,
    HIGH
};

InflationCategory inflationCatFromProto(receiptreaderproto::InflationCategory);
receiptreaderproto::InflationCategory inflationCatToProto(InflationCategory);

#endif