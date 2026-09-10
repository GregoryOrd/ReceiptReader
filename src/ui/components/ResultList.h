#ifndef RESULTLIST_H
#define RESULTLIST_H

#include <QListWidget>
#include <QMouseEvent>
#include "PriceChart.h"
#include "ui/serverConn/serverclient.h"

class ResultList : public QListWidget {
    Q_OBJECT

public:
    ResultList(ServerClient* serverClient) 
    : QListWidget(nullptr), m_serverClient(serverClient) {}

public:
    void setChartView(PriceChart* chartView);
    void graphSelected();
    void colourRow(int row, const QColor& color);
    void addItems(std::vector<PriceHistorySummary> items);

private:
    ServerClient* m_serverClient;
    PriceChart* m_chartView;

signals:
    void doubleClicked();

protected:
    void mouseDoubleClickEvent(QMouseEvent* event) override;
};

#endif