QT += core gui
QT += network

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    AuxFileReader.cpp \
    file_monitor.cpp \
    image_transfer.cpp \
    image_utils.cpp \
    logmanager.cpp \
    main.cpp \
    mainwindow.cpp \
    message_transfer.cpp \
    package_sar_data.cpp

HEADERS += \
    AuxFileReader.h \
    file_monitor.h \
    image_transfer.h \
    image_utils.h \
    logmanager.h \
    mainwindow.h \
    message_transfer.h \
    package_sar_data.h

FORMS += \
    mainwindow.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
