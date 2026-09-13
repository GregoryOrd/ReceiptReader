#include "NumericTableWidgetItem.h"

NumericTableWidgetItem::NumericTableWidgetItem(double value) 
        : QTableWidgetItem(QString::number(value)), _value(value) 
{
    // Do nothing
}

bool NumericTableWidgetItem::operator<(const QTableWidgetItem& other) const {
    if (const NumericTableWidgetItem* otherItem = dynamic_cast<const NumericTableWidgetItem*>(&other)) {
        return _value < otherItem->_value;
    }
    return QTableWidgetItem::operator<(other);
}
