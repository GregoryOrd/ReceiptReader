#ifndef PRICECHART_H
#define PRICECHART_H

#include "common/Item.h"

#include <QChartView>

class PriceChart : public QChartView {
    Q_OBJECT

public:
    PriceChart() : QChartView() {}

public:
    void plotItems(std::vector<Item> items, QString code);

private:

};

#endif