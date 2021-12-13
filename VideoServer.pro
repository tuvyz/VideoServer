QT -= gui

CONFIG += c++11 console
CONFIG -= app_bundle

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
        main.cpp

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target



INCLUDEPATH += $$PWD/include
LIBS += -L$$PWD/bin
LIBS += -lopencv_core440 \
 -lopencv_core440d \
 -lopencv_highgui440 \
 -lopencv_imgcodecs440 \
 -lopencv_imgcodecs440d \
 -lopencv_imgproc440 \
 -lopencv_imgproc440d \
 -lopencv_videoio440 \
 -lopencv_videoio440d \
