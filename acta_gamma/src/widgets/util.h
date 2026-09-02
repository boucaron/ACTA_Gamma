#pragma once

#include <QColor>
#include <QDateTime>
#include <QLocale>
#include <QList>
#include <QString>
#include <QWidget>

#include <cstdlib>
#include <cstring>

// Shared helpers for the widget dialogs (previously copy-pasted into
// each .cpp, UR #6) and for date/status display (UR #24, #25).

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

// Locale-formatted display of a DB timestamp (UR #24). The numeric
// pattern renders "yyyy-MM-dd HH:mm" in every locale (locale-neutral
// digits), so the Date columns stay text-sortable; the exact ISO value
// with seconds is kept for tooltips. Falls back to the raw string if
// parsing fails.
inline QString displayDateTime(const char *iso)
{
    if (!iso || !*iso)
        return QString();
    const QDateTime dt = toDateTime(iso);
    if (!dt.isValid())
        return QString::fromUtf8(iso);
    return QLocale::system().toString(dt, "yyyy-MM-dd HH:mm");
}

// Color coding for execution status and log level (UR #25).
inline QColor statusColor(const QString &status)
{
    const QString s = status.toLower();
    if (s == QLatin1String("completed"))
        return QColor(0x2e, 0x7d, 0x32); // green
    if (s == QLatin1String("running"))
        return QColor(0xe6, 0x7e, 0x22); // orange
    if (s == QLatin1String("failed"))
        return QColor(0xc0, 0x39, 0x2b); // red
    return QColor(Qt::gray); // pending, cancelled, unknown
}

inline QColor logLevelColor(const QString &level)
{
    const QString l = level.toLower();
    if (l == QLatin1String("error"))
        return QColor(0xc0, 0x39, 0x2b); // red
    if (l == QLatin1String("warn") || l == QLatin1String("warning"))
        return QColor(0xe6, 0x7e, 0x22); // orange
    if (l == QLatin1String("debug"))
        return QColor(Qt::gray);
    return {}; // info: default text color
}

// Color the text of a widget (no-op for an invalid color, e.g. the
// default "info" level).
inline void applyTextColor(QWidget *w, const QColor &c)
{
    if (!c.isValid())
        return;
    w->setStyleSheet(QStringLiteral("color: ") + c.name());
}
