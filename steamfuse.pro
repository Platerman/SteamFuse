TEMPLATE = app
TARGET = SteamFuse
QT += widgets
CONFIG += c++17

SOURCES += \
    src/main.cpp \
    src/MainWindow.cpp \
    src/BatchSetupDialog.cpp

HEADERS += \
    include/MainWindow.h \
    include/BatchSetupDialog.h

INCLUDEPATH += include
