#include "mainwindow.h"

#include <QWebEngineView>

MainWindow::MainWindow(const QUrl& applicationUrl, QWidget* parent)
    : QMainWindow(parent), m_webView(new QWebEngineView(this)) {
    setWindowTitle("ReceiptReader");
    resize(1280, 850);
    setCentralWidget(m_webView);
    m_webView->setUrl(applicationUrl);
}
