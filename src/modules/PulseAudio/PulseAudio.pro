TEMPLATE = lib
CONFIG += plugin

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

DESTDIR = ../../../app/share/qmplay2/modules

QMAKE_LIBDIR += ../../../app/lib
LIBS += -lqmplay2 -lpulse-simple

RCC_DIR = build/rcc
OBJECTS_DIR = build/obj
MOC_DIR = build/moc

RESOURCES += icon.qrc

INCLUDEPATH += . ../../qmplay2/headers
DEPENDPATH += . ../../qmplay2/headers

HEADERS += PulseAudio.hpp PulseAudioWriter.hpp pulse.hpp
SOURCES += PulseAudio.cpp PulseAudioWriter.cpp pulse.cpp
