#ifndef NUMERIC_TABLE_WIDGET_ITEM
#define NUMERIC_TABLE_WIDGET_ITEM

#include <QTableWidget>

class NumericTableWidgetItem : public QTableWidgetItem {
public:
    NumericTableWidgetItem(double value);

    bool operator<(const QTableWidgetItem& other) const override;

private:
    double _value;
};

#endif