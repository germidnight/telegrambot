TEMPLATE = app
CONFIG -= qt
CONFIG += console c++20
CONFIG += static

win32: {
    INCLUDEPATH += "C:/boost"
    INCLUDEPATH += "C:/Program Files/FireDaemon OpenSSL 3/include"
    LIBS += -L"C:/boost/lib64-msvc-14.3" -lboost_system*  -lboost_json* -lboost_url* -lboost_log-*
    LIBS += -L"C:/Program Files/FireDaemon OpenSSL 3/lib" -llibssl -llibcrypto
}
linux {
    LIBS += -lboost_system -lboost_json -lboost_url -lboost_log_setup -lboost_log -lboost_thread
    LIBS += -lcrypto -lssl
}
SOURCES += \
    geocode.cpp \
    httpsclient.cpp \
    httpssyncclient.cpp \
    logger.cpp \
    main.cpp \
    meteobot.cpp \
    telegrambot.cpp

HEADERS += \
    geocode.h \
    httpsclient.h \
    httpssyncclient.h \
    logger.h \
    meteobot.h \
    telegrambot.h

DISTFILES += \
    README.md
