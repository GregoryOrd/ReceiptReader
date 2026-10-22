#ifndef MESSAGEUTILS_H
#define MESSAGEUTILS_H

#include <QMessageBox>

namespace ui {

inline void showQueryWarning(QWidget* parent, const QString& message) {
#ifdef BUILD_WASM
    auto* warning = new QMessageBox(QMessageBox::Warning, "Query Failed", message,
                                    QMessageBox::Ok, parent);
    warning->setAttribute(Qt::WA_DeleteOnClose);
    warning->open();
#else
    QMessageBox::warning(parent, "Query Failed", message);
#endif
}

} // namespace ui

#endif // MESSAGEUTILS_H
