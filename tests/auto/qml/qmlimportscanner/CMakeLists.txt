# Copyright (C) 2022 The Qt Company Ltd.
# SPDX-License-Identifier: BSD-3-Clause

# Generated from qmlimportscanner.pro.

#####################################################################
## tst_qmlimportscanner Test:
#####################################################################

# Collect test data
file(GLOB_RECURSE test_data_glob
    RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
    data/*)
list(APPEND test_data ${test_data_glob})

qt_internal_add_test(tst_qmlimportscanner
    SOURCES
        tst_qmlimportscanner.cpp
    LIBRARIES
        Qt::Gui
        Qt::QuickTestUtilsPrivate
    TESTDATA ${test_data}
)

## Scopes:
#####################################################################

qt_internal_extend_target(tst_qmlimportscanner CONDITION ANDROID OR IOS
    DEFINES
        QT_QMLTEST_DATADIR=\\\":/data\\\"
)

qt_internal_extend_target(tst_qmlimportscanner CONDITION NOT ANDROID AND NOT IOS
    DEFINES
        QT_QMLTEST_DATADIR=\\\"${CMAKE_CURRENT_SOURCE_DIR}/data\\\"
)

# special case begin
string(TOLOWER "${QT_LIBINFIX}" infix_lowercase)
qt_internal_extend_target(tst_qmlimportscanner CONDITION DEFINED QT_LIBINFIX
    DEFINES
    LIBINFIX=${infix_lowercase}
)
# special case end
