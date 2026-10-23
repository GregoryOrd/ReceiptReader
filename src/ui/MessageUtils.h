#ifndef MESSAGEUTILS_H
#define MESSAGEUTILS_H

#include <QMessageBox>

namespace ui {

inline void showWarning(QWidget* parent, const QString& title, const QString& message) {
#ifdef BUILD_WASM
    auto* warning = new QMessageBox(QMessageBox::Warning, title, message,
                                    QMessageBox::Ok, parent);
    warning->setAttribute(Qt::WA_DeleteOnClose);
    warning->open();
#else
    QMessageBox::warning(parent, title, message);
#endif
}

inline void showInformation(QWidget* parent, const QString& title, const QString& message) {
#ifdef BUILD_WASM
    auto* information = new QMessageBox(QMessageBox::Information, title, message,
                                        QMessageBox::Ok, parent);
    information->setAttribute(Qt::WA_DeleteOnClose);
    information->open();
#else
    QMessageBox::information(parent, title, message);
#endif
}

inline void showQueryWarning(QWidget* parent, const QString& message) {
    showWarning(parent, "Query Failed", message);
}

} // namespace ui

#endif // MESSAGEUTILS_H
