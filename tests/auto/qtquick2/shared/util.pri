
HEADERS += $$PWD/visualtestutil.h \
           $$PWD/viewtestutil.h
SOURCES += $$PWD/visualtestutil.cpp \
           $$PWD/viewtestutil.cpp

DEFINES += QT_DECLARATIVETEST_DATADIR=\\\"$${_PRO_FILE_PWD_}/data\\\"
