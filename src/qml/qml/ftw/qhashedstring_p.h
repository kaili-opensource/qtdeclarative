// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef QHASHEDSTRING_P_H
#define QHASHEDSTRING_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include <QtCore/qglobal.h>
#include <QtCore/qstring.h>
#include <private/qv4string_p.h>

#if defined(Q_OS_QNX)
#include <stdlib.h>
#endif

QT_BEGIN_NAMESPACE

class QHashedStringRef;
class Q_QML_PRIVATE_EXPORT QHashedString : public QString
{
public:
    inline QHashedString();
    inline QHashedString(const QString &string);
    inline QHashedString(const QString &string, quint32);
    inline QHashedString(const QHashedString &string);

    inline QHashedString &operator=(const QHashedString &string);
    inline bool operator==(const QHashedString &string) const;
    inline bool operator==(const QHashedStringRef &string) const;

    inline quint32 hash() const;
    inline quint32 existingHash() const;

    static inline bool compare(const QChar *lhs, const char *rhs, int length);
    static inline bool compare(const char *lhs, const char *rhs, int length);

    static inline quint32 stringHash(const QChar* data, int length);
    static inline quint32 stringHash(const char *data, int length);

private:
    friend class QHashedStringRef;
    friend class QStringHashNode;

    inline void computeHash() const;
    mutable quint32 m_hash = 0;
};

class QHashedCStringRef;
class Q_QML_PRIVATE_EXPORT QHashedStringRef
{
public:
    inline QHashedStringRef();
    inline QHashedStringRef(const QString &);
    inline QHashedStringRef(QStringView);
    inline QHashedStringRef(const QChar *, int);
    inline QHashedStringRef(const QChar *, int, quint32);
    inline QHashedStringRef(const QHashedString &);
    inline QHashedStringRef(const QHashedStringRef &);
    inline QHashedStringRef &operator=(const QHashedStringRef &);

    inline bool operator==(const QString &string) const;
    inline bool operator==(const QHashedString &string) const;
    inline bool operator==(const QHashedStringRef &string) const;
    inline bool operator==(const QHashedCStringRef &string) const;
    inline bool operator!=(const QString &string) const;
    inline bool operator!=(const QHashedString &string) const;
    inline bool operator!=(const QHashedStringRef &string) const;
    inline bool operator!=(const QHashedCStringRef &string) const;

    inline quint32 hash() const;

    inline QChar *data();
    inline const QChar &at(int) const;
    inline const QChar *constData() const;
    bool startsWith(const QString &) const;
    bool endsWith(const QString &) const;
    int indexOf(const QChar &, int from=0) const;
    QHashedStringRef mid(int, int) const;
    QVector<QHashedStringRef> split(const QChar sep) const;

    inline bool isEmpty() const;
    inline int length() const;
    inline bool startsWithUpper() const;

    QString toString() const;

    inline bool isLatin1() const;

private:
    friend class QHashedString;

    inline void computeHash() const;

    const QChar *m_data = nullptr;
    int m_length = 0;
    mutable quint32 m_hash = 0;
};

class QHashedCStringRef
{
public:
    inline QHashedCStringRef();
    inline QHashedCStringRef(const char *, int);
    inline QHashedCStringRef(const char *, int, quint32);
    inline QHashedCStringRef(const QHashedCStringRef &);

    inline quint32 hash() const;

    inline const char *constData() const;
    inline int length() const;

    Q_AUTOTEST_EXPORT QString toUtf16() const;
    inline int utf16length() const;
    inline void writeUtf16(QChar *) const;
    inline void writeUtf16(quint16 *) const;
private:
    friend class QHashedStringRef;

    inline void computeHash() const;

    const char *m_data = nullptr;
    int m_length = 0;
    mutable quint32 m_hash = 0;
};

inline size_t qHash(const QHashedString &string)
{
    return uint(string.hash());
}

inline size_t qHash(const QHashedStringRef &string)
{
    return uint(string.hash());
}

QHashedString::QHashedString()
: QString()
{
}

QHashedString::QHashedString(const QString &string)
: QString(string), m_hash(0)
{
}

QHashedString::QHashedString(const QString &string, quint32 hash)
: QString(string), m_hash(hash)
{
}

QHashedString::QHashedString(const QHashedString &string)
: QString(string), m_hash(string.m_hash)
{
}

QHashedString &QHashedString::operator=(const QHashedString &string)
{
    static_cast<QString &>(*this) = string;
    m_hash = string.m_hash;
    return *this;
}

bool QHashedString::operator==(const QHashedString &string) const
{
    return (string.m_hash == m_hash || !string.m_hash || !m_hash) &&
           static_cast<const QString &>(*this) == static_cast<const QString &>(string);
}

bool QHashedString::operator==(const QHashedStringRef &string) const
{
    if (m_hash && string.m_hash && m_hash != string.m_hash)
        return false;
    QStringView otherView {string.m_data, string.m_length};
    return static_cast<const  QString &>(*this) == otherView;
}

quint32 QHashedString::hash() const
{
    if (!m_hash) computeHash();
    return m_hash;
}

quint32 QHashedString::existingHash() const
{
    return m_hash;
}

QHashedStringRef::QHashedStringRef()
{
}

QHashedStringRef::QHashedStringRef(const QString &str)
: m_data(str.constData()), m_length(str.size()), m_hash(0)
{
}

QHashedStringRef::QHashedStringRef(QStringView str)
: m_data(str.constData()), m_length(str.size()), m_hash(0)
{
}

QHashedStringRef::QHashedStringRef(const QChar *data, int length)
: m_data(data), m_length(length), m_hash(0)
{
}

QHashedStringRef::QHashedStringRef(const QChar *data, int length, quint32 hash)
: m_data(data), m_length(length), m_hash(hash)
{
}

QHashedStringRef::QHashedStringRef(const QHashedString &string)
: m_data(string.constData()), m_length(string.size()), m_hash(string.m_hash)
{
}

QHashedStringRef::QHashedStringRef(const QHashedStringRef &string)
: m_data(string.m_data), m_length(string.m_length), m_hash(string.m_hash)
{
}

QHashedStringRef &QHashedStringRef::operator=(const QHashedStringRef &o)
{
    m_data = o.m_data;
    m_length = o.m_length;
    m_hash = o.m_hash;
    return *this;
}

bool QHashedStringRef::operator==(const QString &string) const
{
    QStringView view {m_data, m_length};
    return view == string;
}

bool QHashedStringRef::operator==(const QHashedString &string) const
{
    if (m_hash && string.m_hash && m_hash != string.m_hash)
        return false;
    QStringView view {m_data, m_length};
    QStringView otherView {string.constData(), string.size()};
    return view == otherView;
}

bool QHashedStringRef::operator==(const QHashedStringRef &string) const
{
    if (m_hash && string.m_hash && m_hash != string.m_hash)
        return false;
    QStringView view {m_data, m_length};
    QStringView otherView {string.m_data, string.m_length};
    return view == otherView;
}

bool QHashedStringRef::operator==(const QHashedCStringRef &string) const
{
    return m_length == string.m_length &&
           (m_hash == string.m_hash || !m_hash || !string.m_hash) &&
           QHashedString::compare(m_data, string.m_data, m_length);
}

bool QHashedStringRef::operator!=(const QString &string) const
{
    return !(*this == string);
}

bool QHashedStringRef::operator!=(const QHashedString &string) const
{
    return !(*this == string);
}

bool QHashedStringRef::operator!=(const QHashedStringRef &string) const
{
    return !(*this == string);
}

bool QHashedStringRef::operator!=(const QHashedCStringRef &string) const
{
    return !(*this == string);
}

QChar *QHashedStringRef::data()
{
    return const_cast<QChar *>(m_data);
}

const QChar &QHashedStringRef::at(int index) const
{
    Q_ASSERT(index < m_length);
    return m_data[index];
}

const QChar *QHashedStringRef::constData() const
{
    return m_data;
}

bool QHashedStringRef::isEmpty() const
{
    return m_length == 0;
}

int QHashedStringRef::length() const
{
    return m_length;
}

bool QHashedStringRef::isLatin1() const
{
    for (int ii = 0; ii < m_length; ++ii)
        if (m_data[ii].unicode() > 127) return false;
    return true;
}

void QHashedStringRef::computeHash() const
{
    m_hash = QHashedString::stringHash(m_data, m_length);
}

bool QHashedStringRef::startsWithUpper() const
{
    if (m_length < 1) return false;
    return m_data[0].isUpper();
}

quint32 QHashedStringRef::hash() const
{
    if (!m_hash) computeHash();
    return m_hash;
}

QHashedCStringRef::QHashedCStringRef()
{
}

QHashedCStringRef::QHashedCStringRef(const char *data, int length)
: m_data(data), m_length(length), m_hash(0)
{
}

QHashedCStringRef::QHashedCStringRef(const char *data, int length, quint32 hash)
: m_data(data), m_length(length), m_hash(hash)
{
}

QHashedCStringRef::QHashedCStringRef(const QHashedCStringRef &o)
: m_data(o.m_data), m_length(o.m_length), m_hash(o.m_hash)
{
}

quint32 QHashedCStringRef::hash() const
{
    if (!m_hash) computeHash();
    return m_hash;
}

const char *QHashedCStringRef::constData() const
{
    return m_data;
}

int QHashedCStringRef::length() const
{
    return m_length;
}

int QHashedCStringRef::utf16length() const
{
    return m_length;
}

void QHashedCStringRef::writeUtf16(QChar *output) const
{
    writeUtf16((quint16 *)output);
}

void QHashedCStringRef::writeUtf16(quint16 *output) const
{
    int l = m_length;
    const char *d = m_data;
    while (l--)
        *output++ = *d++;
}

void QHashedCStringRef::computeHash() const
{
    m_hash = QHashedString::stringHash(m_data, m_length);
}

bool QHashedString::compare(const QChar *lhs, const char *rhs, int length)
{
    Q_ASSERT(lhs && rhs);
    const quint16 *l = (const quint16*)lhs;
    while (length--)
        if (*l++ != *rhs++) return false;
    return true;
}

bool QHashedString::compare(const char *lhs, const char *rhs, int length)
{
    Q_ASSERT(lhs && rhs);
    return 0 == ::memcmp(lhs, rhs, length);
}


quint32 QHashedString::stringHash(const QChar *data, int length)
{
    return QV4::String::createHashValue(data, length, nullptr);
}

quint32 QHashedString::stringHash(const char *data, int length)
{
    return QV4::String::createHashValue(data, length, nullptr);
}

void QHashedString::computeHash() const
{
    m_hash = stringHash(constData(), size());
}

namespace QtPrivate {
inline QString asString(const QHashedCStringRef &ref) { return ref.toUtf16(); }
inline QString asString(const QHashedStringRef &ref) { return ref.toString(); }
}

QT_END_NAMESPACE

#endif // QHASHEDSTRING_P_H
