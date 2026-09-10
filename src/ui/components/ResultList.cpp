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