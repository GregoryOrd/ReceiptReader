#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "components/ResultList.h"
#include "components/PriceChart.h"
#include "ui/serverConn/serverclient.h"

#include <memory>
#include <QMainWindow>
#include <QLineEdit>
#include <QPushButton>
#include <QColor>
#include <QListWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QtCharts/QChartView>
#include <QtCharts/QChart>
#include <QtCharts/QLineSeries>

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);

private slots:
    void toggleConnection();
    void processImage();
    void search();
    void graphSelected();

private:
    void updateConnectionStatus();

    std::unique_ptr<ServerClient> m_serverClient;
    QLabel* m_connectionStatusLabel;
    QPushButton* m_connectButton;
    QPushButton* m_processButton;

    QLineEdit* m_codeEdit;
    QLineEdit* m_priceMinEdit;
    QLineEdit* m_priceMaxEdit;
    QLineEdit* m_dateStartEdit;
    QLineEdit* m_dateEndEdit;
    std::unique_ptr<ResultList> m_resultsList;
    QPushButton* m_searchButton;
    QPushButton* m_graphButton;
    std::unique_ptr<PriceChart> m_chartView;
};

#endif // MAINWINDOW_H
