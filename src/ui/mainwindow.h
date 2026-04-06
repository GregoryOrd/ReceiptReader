#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QLineEdit>
#include <QPushButton>
#include <QListWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QtCharts/QChartView>
#include <QtCharts/QChart>
#include <QtCharts/QLineSeries>
#include "db/database.h"

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    MainWindow(Database* db, QWidget* parent = nullptr);
private slots:
    void search();
    void graphSelected();
private:
    Database* m_db;
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

#endif