#include "mainwindow.h"
#include <QDateTime>
#include <QMessageBox>
#include <QtCharts/QDateTimeAxis>
#include <QtCharts/QValueAxis>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent),
      m_serverClient(std::make_unique<ServerClient>("127.0.0.1", 52000)) {
    setWindowTitle("Receipt Reader GUI");
    QWidget* central = new QWidget;
    setCentralWidget(central);
    QVBoxLayout* layout = new QVBoxLayout(central);

    QHBoxLayout* connectionLayout = new QHBoxLayout;
    connectionLayout->addWidget(new QLabel("Server Status:"));
    m_connectionStatusLabel = new QLabel("Disconnected");
    connectionLayout->addWidget(m_connectionStatusLabel);
    m_connectButton = new QPushButton("Connect");
    connectionLayout->addWidget(m_connectButton);
    m_processButton = new QPushButton("Process Images");
    connectionLayout->addWidget(m_processButton);
    layout->addLayout(connectionLayout);

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

    connect(m_connectButton, &QPushButton::clicked, this, &MainWindow::toggleConnection);
    connect(m_processButton, &QPushButton::clicked, this, &MainWindow::processImages);

    updateConnectionStatus();
}

void MainWindow::updateConnectionStatus() {
    bool connected = m_serverClient->isConnected();
    if (connected) {
        m_connectionStatusLabel->setText(QString("Connected to server port %1").arg(m_serverClient->port()));
    } else {
        m_connectionStatusLabel->setText("Disconnected");
    }
    m_connectButton->setText(connected ? "Disconnect" : "Connect");

    m_searchButton->setEnabled(connected);
    m_graphButton->setEnabled(connected);
    m_processButton->setEnabled(connected);
}

void MainWindow::toggleConnection() {
    if (m_serverClient->isConnected()) {
        m_serverClient->disconnect();
        updateConnectionStatus();
        return;
    }

    if (!m_serverClient->connectToServer()) {
        QMessageBox::warning(this, "Connection Failed", "Could not connect to ReceiptReaderServer.");
        updateConnectionStatus();
        return;
    }

    updateConnectionStatus();
}

void MainWindow::processImages() {
    if (!m_serverClient->isConnected()) {
        QMessageBox::warning(this, "Not Connected", "Please connect to the server before processing images.");
        return;
    }

    m_connectionStatusLabel->setText("Processing images...");
    std::string error;
    bool success = m_serverClient->processImages("imgs",
        [this](int processed, int total, const std::string& current) {
            QString status = QString("Processing %1/%2: %3").arg(processed).arg(total).arg(QString::fromStdString(current));
            m_connectionStatusLabel->setText(status);
        },
        error);

    if (!success) {
        QMessageBox::warning(this, "Process Failed", QString::fromStdString(error));
    }

    updateConnectionStatus();
}

void MainWindow::search() {
    std::vector<Item> items;
    std::string error;
    if (!m_serverClient->queryItems(
            m_codeEdit->text().toStdString(),
            m_priceMinEdit->text().toStdString(),
            m_priceMaxEdit->text().toStdString(),
            m_dateStartEdit->text().toStdString(),
            m_dateEndEdit->text().toStdString(),
            false,
            items,
            error)) {
        QMessageBox::warning(this, "Query Failed", QString::fromStdString(error));
        return;
    }

    m_resultsList->clear();
    for (const auto& item : items) {
        QString text = QString("Code: %1, Desc: %2, Price: %3, Date: %4")
                           .arg(QString::fromStdString(item.code))
                           .arg(QString::fromStdString(item.description))
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

    QString text = m_resultsList->item(row)->text();
    QString code = text.split(",")[0].split(":")[1].trimmed();

    std::vector<Item> items;
    std::string error;
    if (!m_serverClient->queryItems(
            code.toStdString(),
            "",
            "",
            "",
            "",
            true,
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
    for (const auto& item : items) {
        QDateTime dt = QDateTime::fromString(QString::fromStdString(item.timestamp), "yyyy-MM-dd");
        series->append(dt.toMSecsSinceEpoch(), item.price);
    }

    QChart* chart = new QChart;
    chart->addSeries(series);

    auto* axisX = new QDateTimeAxis;
    axisX->setFormat("yyyy-MM-dd");
    axisX->setTitleText("Date");
    chart->addAxis(axisX, Qt::AlignBottom);
    series->attachAxis(axisX);

    auto* axisY = new QValueAxis;
    axisY->setTitleText("Price");
    chart->addAxis(axisY, Qt::AlignLeft);
    series->attachAxis(axisY);

    m_chartView->setChart(chart);
}
