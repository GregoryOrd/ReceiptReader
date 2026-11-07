#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QUrl>

class QWebEngineView;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(const QUrl& applicationUrl, QWidget* parent = nullptr);

private:
    QWebEngineView* m_webView;
};

#endif // MAINWINDOW_H
