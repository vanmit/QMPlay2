TEMPLATE = lib
CONFIG += plugin

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

win32: DESTDIR = ../../../app/modules
else: DESTDIR = ../../../app/share/qmplay2/modules

win32: QMAKE_LIBDIR += ../../../app
else: QMAKE_LIBDIR += ../../../app/lib
LIBS += -lqmplay2
win32: LIBS += -Wl,-Bstatic -lcdio -lcddb -lregex -Wl,-Bdynamic -lwinmm -lws2_32
else: LIBS += -lcdio -lcddb

OBJECTS_DIR = build/obj
RCC_DIR = build/rcc
MOC_DIR = build/moc

RESOURCES += icons.qrc

INCLUDEPATH += . ../../qmplay2/headers
DEPENDPATH += . ../../qmplay2/headers

HEADERS += Inputs.hpp ToneGenerator.hpp PCM.hpp Rayman2.hpp AudioCD.hpp
SOURCES += Inputs.cpp ToneGenerator.cpp PCM.cpp Rayman2.cpp AudioCD.cpp
