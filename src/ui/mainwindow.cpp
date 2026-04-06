#include "mainwindow.h"
#include <QApplication>
#include <QMessageBox>
#include <QDateTime>

MainWindow::MainWindow(Database* db, QWidget* parent) : QMainWindow(parent), m_db(db) {
    setWindowTitle("Receipt Reader");
    QWidget* central = new QWidget;
    setCentralWidget(central);
    QVBoxLayout* layout = new QVBoxLayout(central);

    // Search fields
    QHBoxLayout* searchLayout = new QHBoxLayout;
    searchLayout->addWidget(new QLabel("Item Code:"));
    m_codeEdit = new QLineEdit;
    searchLayout->addWidget(m_codeEdit);
    searchLayout->addWidget(new QLabel("Price Min:"));
    m_priceMinEdit = new QLineEdit;
    searchLayout->addWidget(m_priceMinEdit);
    searchLayout->addWidget(new QLabel("Price Max:"));
    m_priceMaxEdit = new QLineEdit;
    searchLayout->addWidget(m_priceMaxEdit);
    layout->addLayout(searchLayout);

    QHBoxLayout* dateLayout = new QHBoxLayout;
    dateLayout->addWidget(new QLabel("Date Start (YYYY-MM-DD):"));
    m_dateStartEdit = new QLineEdit;
    dateLayout->addWidget(m_dateStartEdit);
    dateLayout->addWidget(new QLabel("Date End (YYYY-MM-DD):"));
    m_dateEndEdit = new QLineEdit;
    dateLayout->addWidget(m_dateEndEdit);
    layout->addLayout(dateLayout);

    m_searchButton = new QPushButton("Search");
    connect(m_searchButton, &QPushButton::clicked, this, &MainWindow::search);
    layout->addWidget(m_searchButton);

    m_resultsList = new QListWidget;
    layout->addWidget(m_resultsList);

    m_graphButton = new QPushButton("Graph Selected Item");
    connect(m_graphButton, &QPushButton::clicked, this, &MainWindow::graphSelected);
    layout->addWidget(m_graphButton);

    m_chartView = new QChartView;
    layout->addWidget(m_chartView);
}

void MainWindow::search() {
    std::string where = "";
    if (!m_codeEdit->text().isEmpty()) {
        where += "code LIKE '%" + m_codeEdit->text().toStdString() + "%'";
    }
    if (!m_priceMinEdit->text().isEmpty()) {
        if (!where.empty()) where += " AND ";
        where += "price >= " + m_priceMinEdit->text().toStdString();
    }
    if (!m_priceMaxEdit->text().isEmpty()) {
        if (!where.empty()) where += " AND ";
        where += "price <= " + m_priceMaxEdit->text().toStdString();
    }
    if (!m_dateStartEdit->text().isEmpty()) {
        if (!where.empty()) where += " AND ";
        where += "timestamp >= '" + m_dateStartEdit->text().toStdString() + "'";
    }
    if (!m_dateEndEdit->text().isEmpty()) {
        if (!where.empty()) where += " AND ";
        where += "timestamp <= '" + m_dateEndEdit->text().toStdString() + " 23:59:59'";
    }
    auto items = m_db->queryItems(where);
    m_resultsList->clear();
    for (const auto& item : items) {
        QString text = QString("Code: %1, Price: %2, Date: %3")
                       .arg(QString::fromStdString(item.code))
                       .arg(item.price)
                       .arg(QString::fromStdString(item.timestamp));
        m_resultsList->addItem(text);
    }
}

void MainWindow::graphSelected() {
    int row = m_resultsList->currentRow();
    if (row < 0) {
        QMessageBox::warning(this, "No Selection", "Please select an item to graph.");
        return;
    }
    // Get code from selected item
    QString text = m_resultsList->item(row)->text();
    QString code = text.split(",")[0].split(":")[1].trimmed();
    // Query all items with this code, ordered by timestamp
    std::string where = "code = '" + code.toStdString() + "' ORDER BY timestamp";
    auto items = m_db->queryItems(where);
    if (items.empty()) return;
    QLineSeries* series = new QLineSeries;
    series->setName("Price over Time for " + code);
    for (const auto& item : items) {
        QDateTime dt = QDateTime::fromString(QString::fromStdString(item.timestamp), "yyyy-MM-dd hh:mm:ss");
        series->append(dt.toMSecsSinceEpoch(), item.price);
    }
    QChart* chart = new QChart;
    chart->addSeries(series);
    chart->createDefaultAxes();
    chart->axes(Qt::Horizontal).first()->setTitleText("Time");
    chart->axes(Qt::Vertical).first()->setTitleText("Price");
    m_chartView->setChart(chart);
}