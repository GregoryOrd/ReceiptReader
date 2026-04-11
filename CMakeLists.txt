cmake_minimum_required(VERSION 3.16)
project(ReceiptReader VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages
find_package(OpenCV REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(TESSERACT REQUIRED tesseract)
pkg_check_modules(LEPTONICA REQUIRED lept)
find_package(Qt6 REQUIRED COMPONENTS Core Widgets Charts)
find_package(Protobuf REQUIRED)
find_package(GTest REQUIRED)

# Enable testing
enable_testing()

# Include directories
include_directories(${OpenCV_INCLUDE_DIRS})
include_directories(${TESSERACT_INCLUDE_DIRS})
include_directories(${LEPTONICA_INCLUDE_DIRS})
include_directories(${CMAKE_SOURCE_DIR}/src)
include_directories(${CMAKE_SOURCE_DIR}/src/server)
include_directories(${CMAKE_CURRENT_BINARY_DIR})

protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS proto/processor.proto)

set(GUI_SOURCES
    src/ui/main.cpp
    src/ui/mainwindow.cpp
    src/ui/serverclient.cpp
    src/ipc/message_io.cpp
    ${PROTO_SRCS}
)

set(SERVER_SOURCES
    src/server/main.cpp
    src/server/server.cpp
    src/server/ProtobufServer.cpp
    src/server/WebServer.cpp
    src/ipc/message_io.cpp
    src/server/ocr/ocr.cpp
    src/server/parser/parser.cpp
    src/server/db/database.cpp
    ${PROTO_SRCS}
)

add_executable(ReceiptReaderGUI ${GUI_SOURCES})
target_include_directories(ReceiptReaderGUI PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(ReceiptReaderGUI
    Qt6::Core
    Qt6::Widgets
    Qt6::Charts
    ${Protobuf_LIBRARIES}
)
set_target_properties(ReceiptReaderGUI PROPERTIES
    AUTOMOC ON
    AUTORCC ON
    AUTOUIC ON
)

add_executable(ReceiptReaderServer ${SERVER_SOURCES})
target_include_directories(ReceiptReaderServer PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(ReceiptReaderServer
    ${OpenCV_LIBS}
    ${TESSERACT_LIBRARIES}
    ${LEPTONICA_LIBRARIES}
    ${Protobuf_LIBRARIES}
    sqlite3
)

# Add test executable
add_executable(ReceiptReaderTests
    test/server/ocr/ocr_test.cpp
    test/server/parser/parser_test.cpp
    test/server/db/database_test.cpp
    test/server/server_integration_test.cpp
    src/server/ocr/ocr.cpp
    src/server/parser/parser.cpp
    src/server/db/database.cpp
    src/server/server.cpp
    src/server/ProtobufServer.cpp
    src/server/WebServer.cpp
    src/ipc/message_io.cpp
    ${PROTO_SRCS}
)

target_include_directories(ReceiptReaderTests PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(ReceiptReaderTests
    GTest::gtest_main
    GTest::gmock
    ${OpenCV_LIBS}
    ${TESSERACT_LIBRARIES}
    ${LEPTONICA_LIBRARIES}
    ${Protobuf_LIBRARIES}
    sqlite3
)

add_test(NAME ReceiptReaderTests COMMAND ReceiptReaderTests)
