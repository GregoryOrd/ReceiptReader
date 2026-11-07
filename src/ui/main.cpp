#include "ui/mainwindow.h"

#include <QApplication>
#include <QMessageBox>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    QUrl applicationUrl("http://127.0.0.1:53000/");
    for (int i = 1; i < argc; ++i) {
        const QString argument = QString::fromLocal8Bit(argv[i]);
        if (argument == "--url" && i + 1 < argc) {
            applicationUrl = QUrl(QString::fromLocal8Bit(argv[++i]));
        }
    }

    const QString scheme = applicationUrl.scheme();
    if (!applicationUrl.isValid() || (scheme != "http" && scheme != "https")) {
        QMessageBox::critical(nullptr, "Invalid URL", "Use --url with a valid HTTP or HTTPS address.");
        return 1;
    }

    MainWindow window(applicationUrl);
    window.show();
    return app.exec();
}
