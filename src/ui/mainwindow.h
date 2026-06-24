#ifndef MAINWINDOW_H
#define MAINWINDOW_H

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
#include "ui/serverclient.h"

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
    void colourResultListRow(int row, const QColor& color);

    std::unique_ptr<ServerClient> m_serverClient;
    QLabel* m_connectionStatusLabel;
    QPushButton* m_connectButton;
    QPushButton* m_processButton;

    QLineEdit* m_codeEdit;
    QLineEdit* m_priceMinEdit;
    QLineEdit* m_priceMaxEdit;
    QLineEdit* m_dateStartEdit;
    QLineEdit* m_dateEndEdit;
    QListWidget* m_resultsList;
    QPushButton* m_searchButton;
    QPushButton* m_graphButton;
    QChartView* m_chartView;
};

#endif // MAINWINDOW_H
