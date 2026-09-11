#include "InflationCategory.h"



InflationCategory inflationCatFromProto(receiptreaderproto::InflationCategory protoCat)
{
    InflationCategory cat = InflationCategory::NORMAL;
    if(protoCat == receiptreaderproto::InflationCategory::LOW)
    {
        cat = InflationCategory::LOW;
    }
    else if(protoCat == receiptreaderproto::InflationCategory::HIGH)
    {
        cat = InflationCategory::HIGH;
    }

    return cat;
}

receiptreaderproto::InflationCategory inflationCatToProto(InflationCategory cat)
{
    receiptreaderproto::InflationCategory protoCat = receiptreaderproto::InflationCategory::NORMAL;
    if(cat == InflationCategory::LOW)
    {
        protoCat = receiptreaderproto::InflationCategory::LOW;
    }
    else if(cat == InflationCategory::HIGH)
    {
        protoCat = receiptreaderproto::InflationCategory::HIGH;
    }

    return protoCat;    
}