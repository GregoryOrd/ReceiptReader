#include "ResultList.h"
#include "common/Item.h"

#include <QMessageBox>

void ResultList::mouseDoubleClickEvent(QMouseEvent* event) {
    if(event->button() == Qt::LeftButton){
        graphSelected();
        emit doubleClicked();
    }
    QListWidget::mouseDoubleClickEvent(event); // Call base for default behaviour
}

void ResultList::setChartView(PriceChart* chartView)
{
    m_chartView = chartView;
}

void ResultList::graphSelected() {
    int row = this->currentRow();
    if (row < 0) {
        QMessageBox::warning(this, "No Selection", "Please select an item to graph.");
        return;
    }

    QString text = this->item(row)->text();
    QString code = text.split(",")[0].split(":")[1].trimmed();

    std::vector<Item> items;
    std::string error;
    if (!m_serverClient->queryItemCode(
            code.toStdString(),
            items,
            error)) {
        QMessageBox::warning(this, "Query Failed", QString::fromStdString(error));
        return;
    }

    if (items.empty()) {
        return;
    }

    m_chartView->plotItems(items, code);
}

void ResultList::colourRow(int row, const QColor& color) {
    if (row < 0 || row >= this->count()) {
        return;
    }
    QListWidgetItem* item = this->item(row);
    if (!item) {
        return;
    }

    item->setBackground(QBrush(color));
}

void ResultList::addItems(std::vector<PriceHistorySummary> items)
{
    for (const auto& item : items) {
        QString text = QString("Code: %1, Desc: %2, Price: %3, Date: %4")
                           .arg(QString::fromStdString(item._code))
                           .arg(QString::fromStdString(item._description))
                           .arg(item._currentPrice)
                           .arg(QString::fromStdString(item._timestampLast));
        addItem(text);

        QColor rowColor;
        if(item._category == InflationCategory::LOW) {
            rowColor = QColor(Qt::green);
        } else if(item._category == InflationCategory::NORMAL) {
            rowColor = QColor(Qt::yellow);
        } else {
            rowColor = QColor(Qt::red);
        }

        colourRow(count() - 1, rowColor);
    }
}