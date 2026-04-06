#include <QApplication>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include "ocr/ocr.h"
#include "parser/parser.h"
#include "db/database.h"
#include "ui/mainwindow.h"

namespace fs = std::filesystem;

fs::path findReceiptDirectory(int argc, char *argv[]) {
    if (argc > 1) {
        return fs::path(argv[1]);
    }

    fs::path candidate = fs::current_path() / "imgs";
    if (fs::exists(candidate) && fs::is_directory(candidate)) {
        return candidate;
    }

    fs::path exePath = fs::read_symlink("/proc/self/exe");
    fs::path projectRoot = exePath.parent_path().parent_path();
    candidate = projectRoot / "imgs";
    if (fs::exists(candidate) && fs::is_directory(candidate)) {
        return candidate;
    }

    throw std::runtime_error("Could not find receipt image directory. Provide it as the first argument or place imgs/ in the project root.");
}

int main(int argc, char *argv[]) {
    // Process images
    fs::path receiptDir;
    try {
        receiptDir = findReceiptDirectory(argc, argv);
    } catch (const std::exception& ex) {
        std::cerr << ex.what() << std::endl;
        return 1;
    }

    Database db("receipts.db");
    db.createTable();
    for (const auto& entry : fs::recursive_directory_iterator(receiptDir)) {
        if (entry.is_regular_file()) {
            std::string path = entry.path().string();
            if (path.find(".jpg") != std::string::npos || path.find(".png") != std::string::npos || path.find(".HEIC") != std::string::npos || path.find(".heic") != std::string::npos) {
                std::string text = extractTextFromImage(path);
                auto items = parseReceiptText(text);
                for (const auto& item : items) {
                    db.insertItem(item);
                }
            }
        }
    }

    // UI
    QApplication app(argc, argv);
    MainWindow window(&db);
    window.show();
    return app.exec();
}