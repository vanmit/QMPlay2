TEMPLATE = lib

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = qmplay2

win32: DESTDIR = ../../app
else: DESTDIR = ../../app/lib

LIBS += -lswscale -lswresample -lavutil
win32: LIBS += -Wl,-Bstatic -lass -lfontconfig -lexpat -lfreetype -lfribidi -Wl,-Bdynamic -lwinmm -lshell32
else: LIBS += -lass

OBJECTS_DIR = build/obj
MOC_DIR = build/moc

INCLUDEPATH += . headers
DEPENDPATH  += . headers

HEADERS += headers/QMPlay2Core.hpp headers/Functions.hpp headers/Settings.hpp headers/Module.hpp headers/ModuleParams.hpp headers/ModuleCommon.hpp headers/Playlist.hpp headers/Reader.hpp headers/Demuxer.hpp headers/Decoder.hpp headers/VideoFilters.hpp headers/VideoFilter.hpp headers/DeintFilter.hpp headers/AudioFilter.hpp headers/Writer.hpp headers/QMPlay2Extensions.hpp headers/LineEdit.hpp headers/Slider.hpp headers/QMPlay2_OSD.hpp headers/InDockW.hpp headers/LibASS.hpp headers/ColorButton.hpp headers/ImgScaler.hpp headers/SndResampler.hpp headers/VideoWriter.hpp headers/SubsDec.hpp headers/ByteArray.hpp headers/TimeStamp.hpp headers/Packet.hpp headers/VideoFrame.hpp headers/StreamInfo.hpp headers/DockWidget.hpp headers/IOController.hpp
SOURCES +=         QMPlay2Core.cpp         Functions.cpp         Settings.cpp         Module.cpp                                                           Playlist.cpp         Reader.cpp         Demuxer.cpp         Decoder.cpp         VideoFilters.cpp         VideoFilter.cpp         DeintFilter.cpp         AudioFilter.cpp         Writer.cpp         QMPlay2Extensions.cpp         LineEdit.cpp         Slider.cpp         QMPlay2_OSD.cpp         InDockW.cpp         LibASS.cpp         ColorButton.cpp         ImgScaler.cpp         SndResampler.cpp                                 SubsDec.cpp                                                                        VideoFrame.cpp

DEFINES += __STDC_CONSTANT_MACROS
# QMAKE_CXXFLAGS += -ftree-vectorize

# Uncomment below lines for avresample:
# DEFINES += QMPLAY2_AVRESAMPLE
# LIBS -= -lswresample
# LIBS += -lavresample
