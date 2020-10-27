#-------------------------------------------------
#
# Project created by QtCreator 2020-07-14T09:36:08
#
#-------------------------------------------------

QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = file_hook
TEMPLATE = app


SOURCES += main.cpp\
        mainwindow.cpp \
    WatchData.cpp \
    Win32FSHook.cpp

HEADERS  += mainwindow.h \
    WatchData.h \
    Win32FSHook.h

FORMS    += mainwindow.ui
