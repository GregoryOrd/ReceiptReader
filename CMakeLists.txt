cmake_minimum_required(VERSION 3.16)

project(ReceiptReader VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ============================================================
# Build options
# ============================================================

option(BUILD_SERVER "Build the native server" ON)
option(BUILD_GUI "Build the GUI" ON)
option(BUILD_TESTS "Build tests" ON)

# Detect whether we're being configured by Emscripten.
if(EMSCRIPTEN)
    set(BUILD_WASM ON)
else()
    set(BUILD_WASM OFF)
endif()

message(STATUS "========================================")
message(STATUS "ReceiptReader configuration")
message(STATUS "  Emscripten: ${EMSCRIPTEN}")
message(STATUS "  BUILD_WASM: ${BUILD_WASM}")
message(STATUS "  BUILD_GUI:  ${BUILD_GUI}")
message(STATUS "  BUILD_SERVER: ${BUILD_SERVER}")
message(STATUS "  BUILD_TESTS: ${BUILD_TESTS}")
message(STATUS "========================================")


# ============================================================
# Common dependencies
# ============================================================

if(BUILD_WASM)
    set(Protobuf_DIR "/usr/local/protobuf-3.21.12-wasm/lib/cmake/protobuf")
    set(QT_CMAKE_DIR "/usr/local/qt-6.4.2-wasm/lib/cmake")

    file(GLOB _qt_subdirs "${QT_CMAKE_DIR}/*")
    foreach(dir ${_qt_subdirs})
        if(IS_DIRECTORY ${dir})
            get_filename_component(_name ${dir} NAME)
            set(${_name}_DIR "${dir}")
        endif()
    endforeach() 

    set(protobuf_MODULE_COMPATIBLE ON)
    find_package(Protobuf CONFIG REQUIRED)
        set_target_properties(protobuf::protoc PROPERTIES
        IMPORTED_LOCATION "/usr/bin/protoc"
    )
else()
    find_package(Protobuf REQUIRED)
    set(Qt6_DIR "/usr/local/qt-6.4.2-host/lib/cmake/Qt6")
endif()
 
find_package(Qt6 REQUIRED COMPONENTS Core Widgets Charts)  

# ============================================================
# Native/server-only dependencies
#
# These are deliberately NOT searched for during WASM builds.
# ============================================================

if(NOT BUILD_WASM)

    find_package(Qt6 REQUIRED COMPONENTS Network WebSockets)
    find_package(OpenCV REQUIRED)
    find_package(PkgConfig REQUIRED)

    pkg_check_modules(TESSERACT REQUIRED tesseract)
    pkg_check_modules(LEPTONICA REQUIRED lept)

endif()


# ============================================================
# Include directories
# ============================================================

include_directories(
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/server
    ${CMAKE_SOURCE_DIR}/src/common
    ${CMAKE_CURRENT_BINARY_DIR}
)

if(NOT BUILD_WASM)

    include_directories(${OpenCV_INCLUDE_DIRS})
    include_directories(${TESSERACT_INCLUDE_DIRS})
    include_directories(${LEPTONICA_INCLUDE_DIRS})

endif()

# Generate protobuf sources under build/proto to match includes such as
# "proto/processor.pb.h" from the source and generated directories.
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/proto")
protobuf_generate(
    OUT_VAR PROTO_FILES
    LANGUAGE cpp
    APPEND_PATH
    IMPORT_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/proto"
    PROTOS "${CMAKE_CURRENT_SOURCE_DIR}/proto/processor.proto"
    PROTOC_OUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/proto"
)

add_custom_target(ReceiptReaderProto DEPENDS ${PROTO_FILES})

set(COMMON_SOURCES
    src/common/PriceHistorySummary.cpp
    ${PROTO_FILES}
)

# ============================================================
# GUI
# ============================================================

if(BUILD_GUI)

    set(GUI_SOURCES
        src/ui/main.cpp
        src/ui/mainwindow.cpp
        src/ui/serverConn/serverclient.cpp
        src/ipc/message_io.cpp

        src/ui/components/PriceSummaryTable.h
        src/ui/components/PriceSummaryTable.cpp

        src/ui/components/NumericTableWidgetItem.cpp

        src/ui/components/PriceChart.h
        src/ui/components/PriceChart.cpp

        ${COMMON_SOURCES}
    )

    add_executable(
        ReceiptReaderGUI
        ${GUI_SOURCES}
    )
    add_dependencies(ReceiptReaderGUI ReceiptReaderProto)

    if(BUILD_WASM)
        target_compile_definitions(ReceiptReaderGUI PRIVATE BUILD_WASM)
        target_link_options(ReceiptReaderGUI PRIVATE
            "-sMODULARIZE=1"
            "-sEXPORT_NAME=createQtAppInstance"
            "-sALLOW_MEMORY_GROWTH=1"
            "-sEXPORTED_RUNTIME_METHODS=UTF16ToString,stringToUTF16,JSEvents,specialHTMLTargets,FS,SOCKFS,callMain"
        )
        # Copy qtloader.js into build dir
        add_custom_command(TARGET ReceiptReaderGUI POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                /usr/local/qt-6.4.2-wasm/plugins/platforms/qtloader.js
                $<TARGET_FILE_DIR:ReceiptReaderGUI>/qtloader.js
        )
    endif()

    target_include_directories(
        ReceiptReaderGUI
        PRIVATE
            ${CMAKE_SOURCE_DIR}/src
            ${CMAKE_CURRENT_BINARY_DIR}
    )

    set_target_properties(
        ReceiptReaderGUI
        PROPERTIES
            AUTOMOC ON
            AUTORCC ON
            AUTOUIC ON
    )

endif()

if(TARGET protobuf::libprotobuf)
    target_link_libraries(
        ReceiptReaderGUI
        PRIVATE
        Qt6::Core
        Qt6::Widgets
        Qt6::Charts
        protobuf::libprotobuf
    )
else()
    target_link_libraries(
        ReceiptReaderGUI
        PRIVATE
        Qt6::Core
        Qt6::Widgets
        Qt6::Charts
        ${Protobuf_LIBRARIES}
    )
endif()

if(BUILD_WASM)
    target_link_libraries(ReceiptReaderGUI PRIVATE embind)
endif()


# ============================================================
# Native server
#
# Never build the server for WASM.
# ============================================================

if(BUILD_SERVER AND NOT BUILD_WASM)

    set(SERVER_SOURCES
        src/server/main.cpp
        src/server/server.cpp
        src/server/ImageProcessor.cpp
        src/server/PriceSummaryGenerator.cpp
        src/server/guiComms/ProtobufServer.cpp
        src/server/guiComms/WebSocketBridge.cpp
        src/server/guiComms/WebServer.cpp
        src/ipc/message_io.cpp
        src/server/ocr/ocr.cpp
        src/server/parser/parser.cpp
        src/server/db/database.cpp
        ${COMMON_SOURCES}
    )

    add_executable(
        ReceiptReaderServer
        ${SERVER_SOURCES}
    )
    add_dependencies(ReceiptReaderServer ReceiptReaderProto)

    target_include_directories(
        ReceiptReaderServer
        PRIVATE
            ${CMAKE_SOURCE_DIR}/src
            ${CMAKE_CURRENT_BINARY_DIR}
    )

    target_link_libraries(
        ReceiptReaderServer
        Qt6::Core
        Qt6::Network
        Qt6::WebSockets
        ${OpenCV_LIBS}
        ${TESSERACT_LIBRARIES}
        ${LEPTONICA_LIBRARIES}
        ${Protobuf_LIBRARIES}
        sqlite3
    )

endif()


# ============================================================
# Tests
#
# Tests are native-only.
# ============================================================

if(BUILD_TESTS AND NOT BUILD_WASM)

    find_package(GTest REQUIRED)

    add_executable(
        ReceiptReaderTests
        ${SERVER_SOURCES}
    )
    add_dependencies(ReceiptReaderTests ReceiptReaderProto)

    target_include_directories(
        ReceiptReaderTests
        PRIVATE
            ${CMAKE_SOURCE_DIR}/src
            ${CMAKE_CURRENT_BINARY_DIR}
    )

    target_link_libraries(
        ReceiptReaderTests
        Qt6::Core
        Qt6::Network
        Qt6::WebSockets
        GTest::gtest_main
        GTest::gmock
        ${OpenCV_LIBS}
        ${TESSERACT_LIBRARIES}
        ${LEPTONICA_LIBRARIES}
        ${Protobuf_LIBRARIES}
        sqlite3
    )

    add_test(
        NAME ReceiptReaderTests
        COMMAND ReceiptReaderTests
    )

endif()
