#pragma once

#include <QDateTime>
#include <QLocale>
#include <QList>
#include <QString>

#include <cstdlib>
#include <cstring>

// Shared helpers for the widget dialogs (previously copy-pasted into
// each .cpp, UR #6).

// strdup is not part of the C standard; duplicate into a malloc block
// freed by free().
inline char *dupString(const char *s)
{
    if (!s)
        return nullptr;
    const size_t n = std::strlen(s) + 1;
    char *copy = static_cast<char *>(std::malloc(n));
    if (copy)
        std::memcpy(copy, s, n);
    return copy;
}

// Safe QString conversion of a possibly-null C string.
inline QString utf8(const char *s)
{
    return s ? QString::fromUtf8(s) : QString();
}

// created_at/updated_at come back from SQLite's now() as
// "yyyy-MM-dd HH:mm:ss"; deleted_at is NULL for live rows.
// (The former Qt::ISODate attempt could never match that shape, so it
// is dropped; Qt::ISODateWithMs still accepts ISO strings with
// milliseconds.)
inline QDateTime toDateTime(const char *iso)
{
    if (!iso || !*iso)
        return {};
    const QString s = QString::fromUtf8(iso);
    static const QList<Qt::DateFormat> formats = {Qt::ISODateWithMs};
    for (const auto f : formats) {
        const QDateTime dt = QDateTime::fromString(s, f);
        if (dt.isValid())
            return dt;
    }
    return QLocale::c().toDateTime(s, "yyyy-MM-dd HH:mm:ss");
}
