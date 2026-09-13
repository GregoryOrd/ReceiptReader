#ifndef RESULTLIST_H
#define RESULTLIST_H

#include <QTableWidget>
#include <QMouseEvent>
#include "PriceChart.h"
#include "ui/serverConn/serverclient.h"

class PriceSummaryTable : public QTableWidget {
    Q_OBJECT

public:
    PriceSummaryTable(ServerClient* serverClient);

public:
    void setChartView(PriceChart* chartView);
    void graphSelected();
    void setItems(std::vector<PriceHistorySummary> items);

private:
    void colourCell(int row, int col, QColor color);
    void colourInflationRate(int row, int col, double inflationRate);

private:
    ServerClient* m_serverClient;
    PriceChart* m_chartView;

    std::unique_ptr<QTableWidgetItem> _codeHdr;
    std::unique_ptr<QTableWidgetItem> _descHdr;
    std::unique_ptr<QTableWidgetItem> _priceHdr;
    std::unique_ptr<QTableWidgetItem> _dateHdr;
    std::unique_ptr<QTableWidgetItem> _lifetimeHeader;
    std::unique_ptr<QTableWidgetItem> _oneYearHdr;
    std::unique_ptr<QTableWidgetItem> _sixMnthHdr;
    std::unique_ptr<QTableWidgetItem> _threeMnthHdr;
    std::unique_ptr<QTableWidgetItem> _oneMnthHdr;
    std::unique_ptr<QTableWidgetItem> _timesPurchasedHdr;

signals:
    void doubleClicked();

protected:
    void mouseDoubleClickEvent(QMouseEvent* event) override;
};

#endif