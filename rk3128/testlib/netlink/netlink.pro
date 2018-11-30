#-------------------------------------------------
#
# Project created by QtCreator 2018-11-21T18:07:46
#
#-------------------------------------------------

QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = netlink
TEMPLATE = app
INCLUDEPATH += /root/Network/libnl/libnl-3.2.25/build_root/include/libnl3
#INCLUDEPATH += /opt/arm-2009q3/arm-none-linux-gnueabi/libc/usr/include
SOURCES += main.cpp\
        mainwindow.cpp

HEADERS  += mainwindow.h

FORMS    += mainwindow.ui


unix:!macx: LIBS += -L$$PWD/../../libnl-3.2.25/build_root/lib/ -lnl-genl-3

INCLUDEPATH += $$PWD/../../libnl-3.2.25/build_root/include
DEPENDPATH += $$PWD/../../libnl-3.2.25/build_root/include

unix:!macx: LIBS += -L$$PWD/../../libnl-3.2.25/build_root/lib/ -lnl-idiag-3

INCLUDEPATH += $$PWD/../../libnl-3.2.25/build_root/include
DEPENDPATH += $$PWD/../../libnl-3.2.25/build_root/include

unix:!macx: PRE_TARGETDEPS += $$PWD/../../libnl-3.2.25/build_root/lib/libnl-idiag-3.a

unix:!macx: LIBS += -L$$PWD/../../libnl-3.2.25/build_root/lib/ -lnl-3

INCLUDEPATH += $$PWD/../../libnl-3.2.25/build_root/include
DEPENDPATH += $$PWD/../../libnl-3.2.25/build_root/include

unix:!macx: PRE_TARGETDEPS += $$PWD/../../libnl-3.2.25/build_root/lib/libnl-3.a
