#include "mainwindow.h"
#include <QDateTime>
#include <QDialog>
#include <QFileDialog>
#include <QMessageBox>
#include <QtCharts/QDateTimeAxis>
#include <QtCharts/QValueAxis>
#include <fstream>

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
    m_processButton = new QPushButton("Process Image");
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
    connect(m_processButton, &QPushButton::clicked, this, &MainWindow::processImage);

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

void MainWindow::processImage() {
    if (!m_serverClient->isConnected()) {
        QMessageBox::warning(this, "Not Connected", "Please connect to the server before processing an image.");
        return;
    }

    QDialog dialog(this);
    dialog.setWindowTitle("Process Image");
    QVBoxLayout* dialogLayout = new QVBoxLayout(&dialog);

    QPushButton* selectButton = new QPushButton("Select Image");
    QLabel* selectedFileLabel = new QLabel("No image selected");
    dialogLayout->addWidget(selectButton);
    dialogLayout->addWidget(selectedFileLabel);

    QListWidget* itemList = new QListWidget;
    dialogLayout->addWidget(itemList);

    QHBoxLayout* buttonLayout = new QHBoxLayout;
    QPushButton* confirmButton = new QPushButton("Confirm");
    QPushButton* cancelButton = new QPushButton("Cancel");
    confirmButton->setEnabled(false);
    buttonLayout->addWidget(confirmButton);
    buttonLayout->addWidget(cancelButton);
    dialogLayout->addLayout(buttonLayout);

    std::vector<Item> foundItems;
    QString selectedPath;

    connect(selectButton, &QPushButton::clicked, this, [&]() {
        QString fileName = QFileDialog::getOpenFileName(&dialog, "Select Receipt Image", QString(), "Images (*.png *.jpg *.jpeg *.heic)");
        if (fileName.isEmpty()) {
            return;
        }
        selectedPath = fileName;
        selectedFileLabel->setText(fileName);
        itemList->clear();
        confirmButton->setEnabled(false);

        std::ifstream file(fileName.toStdString(), std::ios::binary);
        if (!file) {
            QMessageBox::warning(&dialog, "Read Failed", "Could not open the selected image file.");
            return;
        }

        std::vector<uint8_t> imageData((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        file.close();

        std::string error;
        if (!m_serverClient->processImage(imageData, selectedPath.toStdString(), foundItems, error)) {
            QMessageBox::warning(&dialog, "Process Failed", QString::fromStdString(error));
            return;
        }

        itemList->clear();
        for (const auto& item : foundItems) {
            QString line = QString("Code: %1, Desc: %2, Price: %3, Date: %4")
                               .arg(QString::fromStdString(item.code))
                               .arg(QString::fromStdString(item.description))
                               .arg(item.price)
                               .arg(QString::fromStdString(item.timestamp));
            itemList->addItem(line);
        }

        confirmButton->setEnabled(!foundItems.empty());
    });

    connect(cancelButton, &QPushButton::clicked, &dialog, &QDialog::reject);
    connect(confirmButton, &QPushButton::clicked, this, [&]() {
        if (foundItems.empty()) {
            QMessageBox::warning(&dialog, "Nothing to Confirm", "No items have been processed yet.");
            return;
        }

        std::string error;
        if (!m_serverClient->confirmProcessedItems(foundItems, error)) {
            QMessageBox::warning(&dialog, "Confirm Failed", QString::fromStdString(error));
            return;
        }

        dialog.accept();
    });

    dialog.exec();
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
