TEMPLATE = app
CONFIG -= qt
CONFIG += console c++20
CONFIG += static

win32: {
    INCLUDEPATH += "C:/boost"
    INCLUDEPATH += "C:/Program Files/FireDaemon OpenSSL 3/include"
    LIBS += -L"C:/boost/lib64-msvc-14.3" -lboost_system*  -lboost_json* -lboost_url*
    LIBS += -L"C:/Program Files/FireDaemon OpenSSL 3/lib" -llibssl -llibcrypto
}
linux {
    LIBS += -lboost_system -lboost_json -lboost_url
    LIBS += -lcrypto -lssl
}
SOURCES += \
        geocode.cpp \
        httpsclient.cpp \
        httpssyncclient.cpp \
        main.cpp \
        meteobot.cpp \
        telegrambot.cpp

HEADERS += \
    geocode.h \
    httpsclient.h \
    httpssyncclient.h \
    meteobot.h \
    telegrambot.h

DISTFILES += \
    README.md
