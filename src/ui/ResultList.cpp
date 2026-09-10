#include "ResultList.h"
#include "common/Item.h"

#include <QMessageBox>
#include <QLineSeries>
#include <QDateTime>
#include <QChart>
#include <QDateTimeAxis>
#include <QValueAxis>

void ResultList::mouseDoubleClickEvent(QMouseEvent* event) {
    if(event->button() == Qt::LeftButton){
        graphSelected();
        emit doubleClicked();
    }
    QListWidget::mouseDoubleClickEvent(event); // Call base for default behaviour
}

void ResultList::setChartView(QChartView* chartView)
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

    QLineSeries* series = new QLineSeries;
    series->setName("Price over Time for " + code);
    series->setPointsVisible(true);
    series->setPointLabelsVisible(false);

    qint64 minX = std::numeric_limits<qint64>::max();
    qint64 maxX = std::numeric_limits<qint64>::min();
    double minY = std::numeric_limits<double>::infinity();
    double maxY = -std::numeric_limits<double>::infinity();

    for (const auto& item : items) {
        const QString timestamp = QString::fromStdString(item.timestamp);
        QDateTime dt = QDateTime::fromString(timestamp, Qt::ISODate);
        if (!dt.isValid()) {
            dt = QDateTime::fromString(timestamp, "yyyy-MM-dd");
        }
        if (!dt.isValid()) {
            continue;
        }

        qint64 x = dt.toMSecsSinceEpoch();
        series->append(x, item.price);
        minX = std::min(minX, x);
        maxX = std::max(maxX, x);
        minY = std::min(minY, item.price);
        maxY = std::max(maxY, item.price);
    }

    if (series->count() == 0) {
        QMessageBox::information(this, "No Graph Data", "Selected item has no valid timestamp data to graph.");
        return;
    }

    QChart* chart = new QChart;
    chart->addSeries(series);
    chart->legend()->setVisible(true);
    chart->legend()->setAlignment(Qt::AlignBottom);

    auto* axisX = new QDateTimeAxis;
    axisX->setFormat("yyyy-MM-dd");
    axisX->setTitleText("Time");
    axisX->setTickCount(6);
    axisX->setRange(QDateTime::fromMSecsSinceEpoch(minX), QDateTime::fromMSecsSinceEpoch(maxX));
    chart->addAxis(axisX, Qt::AlignBottom);
    series->attachAxis(axisX);

    auto* axisY = new QValueAxis;
    axisY->setTitleText("Price");
    axisY->setTickCount(6);
    if (minY == maxY) {
        axisY->setRange(minY * 0.9, maxY * 1.1);
    } else {
        axisY->setRange(minY - (maxY - minY) * 0.1, maxY + (maxY - minY) * 0.1);
    }
    chart->addAxis(axisY, Qt::AlignLeft);
    series->attachAxis(axisY);

    m_chartView->setChart(chart);
}