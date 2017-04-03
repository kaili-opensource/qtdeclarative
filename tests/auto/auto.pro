TEMPLATE = subdirs
SUBDIRS += \
    accessibility \
    applicationwindow \
    calendar \
    controls \
    drawer \
    menu \
    platform \
    popup \
    pressandhold \
    qquickmaterialstyle \
    qquickmaterialstyleconf \
    qquickstyle \
    qquickstyleselector \
    qquickuniversalstyle \
    qquickuniversalstyleconf \
    revisions \
    sanity \
    scrollbar \
    snippets

# QTBUG-50295
!linux: SUBDIRS += \
    focus
