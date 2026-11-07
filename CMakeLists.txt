cmake_minimum_required(VERSION 3.16)
project(ReceiptReader VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenCV REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(TESSERACT REQUIRED tesseract)
pkg_check_modules(LEPTONICA REQUIRED lept)
find_package(Qt6 REQUIRED COMPONENTS Core Widgets WebEngineWidgets)
find_package(GTest REQUIRED)
find_program(NPM_EXECUTABLE npm)
if(NOT NPM_EXECUTABLE)
    message(FATAL_ERROR "npm is required to build the Vue frontend")
endif()

enable_testing()

include_directories(${OpenCV_INCLUDE_DIRS})
include_directories(${TESSERACT_INCLUDE_DIRS})
include_directories(${LEPTONICA_INCLUDE_DIRS})
include_directories(${CMAKE_SOURCE_DIR}/src)
include_directories(${CMAKE_SOURCE_DIR}/src/server)

add_custom_target(ReceiptReaderWebBuild
    COMMAND ${NPM_EXECUTABLE} run build
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/web
    BYPRODUCTS ${CMAKE_SOURCE_DIR}/web/dist/index.html
    COMMENT "Building the Vue web application"
)

set(SERVER_CORE_SOURCES
    src/server/server.cpp
    src/server/ImageProcessor.cpp
    src/server/PriceSummaryGenerator.cpp
    src/server/guiComms/WebServer.cpp
    src/server/ocr/ocr.cpp
    src/server/parser/parser.cpp
    src/server/db/database.cpp
    src/common/PriceHistorySummary.cpp
)

add_executable(ReceiptReaderGUI
    src/ui/main.cpp
    src/ui/mainwindow.cpp
)
target_include_directories(ReceiptReaderGUI PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(ReceiptReaderGUI
    Qt6::Core
    Qt6::Widgets
    Qt6::WebEngineWidgets
)
set_target_properties(ReceiptReaderGUI PROPERTIES
    AUTOMOC ON
)

add_executable(ReceiptReaderServer
    src/server/main.cpp
    ${SERVER_CORE_SOURCES}
)
target_include_directories(ReceiptReaderServer PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(ReceiptReaderServer
    Qt6::Core
    ${OpenCV_LIBS}
    ${TESSERACT_LIBRARIES}
    ${LEPTONICA_LIBRARIES}
    sqlite3
)
add_dependencies(ReceiptReaderServer ReceiptReaderWebBuild)

add_executable(ReceiptReaderTests
    test/server/db/database_test.cpp
    test/server/ocr/ocr_test.cpp
    test/server/parser/parser_test.cpp
    test/server/server_integration_test.cpp
    ${SERVER_CORE_SOURCES}
)
target_include_directories(ReceiptReaderTests PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(ReceiptReaderTests
    Qt6::Core
    GTest::gtest_main
    GTest::gmock
    ${OpenCV_LIBS}
    ${TESSERACT_LIBRARIES}
    ${LEPTONICA_LIBRARIES}
    sqlite3
)
add_dependencies(ReceiptReaderTests ReceiptReaderWebBuild)
add_test(NAME ReceiptReaderTests COMMAND ReceiptReaderTests)
set_tests_properties(ReceiptReaderTests PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
