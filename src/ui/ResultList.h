#ifndef RESULTLIST_H
#define RESULTLIST_H

#include <QListWidget>
#include <QMouseEvent>
#include <QChartView>
#include "serverclient.h"

class ResultList : public QListWidget {
    Q_OBJECT

public:
    ResultList(ServerClient* serverClient) 
    : QListWidget(nullptr), m_serverClient(serverClient) {}

public:
    void setChartView(QChartView* chartView);
    void graphSelected();

private:
    ServerClient* m_serverClient;
    QChartView* m_chartView;

signals:
    void doubleClicked();

protected:
    void mouseDoubleClickEvent(QMouseEvent* event) override;
};

#endif