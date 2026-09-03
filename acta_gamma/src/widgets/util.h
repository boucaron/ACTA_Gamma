#pragma once

#include "acta_db.h"

#include <QAbstractScrollArea>
#include <QColor>
#include <QDateTime>
#include <QIcon>
#include <QLabel>
#include <QKeySequence>
#include <QList>
#include <QPushButton>
#include <QString>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QWidget>

#include <cstdlib>
#include <cstring>
#include <functional>

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

// User-facing message for an acta_db error code (P6 / UR #45).
//
// `noun`     – lowercase entity word for the message ("skill", "model",
//              "folder", "context").
// `name`     – the affected entity's name (used for DUPLICATE and
//              NOT_FOUND; may be empty).
// `fallback` – generic message template with two placeholders, e.g.
//              "Could not create %1: %2" (%1 = noun, %2 = error text).
// `detail`   – optional extra detail appended to the error text for
//              ACTA_DB_ERR_SQL (e.g. DbHandle::lastError()).
//
// Mapping:
//   ACTA_DB_ERR_DUPLICATE – "A %1 named \"%2\" already exists in this folder"
//   ACTA_DB_ERR_FK        – "The target folder no longer exists"
//   ACTA_DB_ERR_NOT_FOUND – "\"%1\" could not be found (it may have been deleted)"
//   otherwise             – fallback with noun + acta_db_strerror(rc)
//                           (+ detail for ERR_SQL).
inline QString friendlyDbError(int rc, const QString &noun,
                              const QString &name,
                              const QString &fallback,
                              const char *detail = nullptr)
{
    switch (rc) {
    case ACTA_DB_ERR_DUPLICATE:
        return QStringLiteral("A %1 named \"%2\" already exists in this folder")
            .arg(noun, name);
    case ACTA_DB_ERR_FK:
        return QStringLiteral("The target folder no longer exists");
    case ACTA_DB_ERR_NOT_FOUND:
        return QStringLiteral("\"%1\" could not be found (it may have been deleted)")
            .arg(name);
    default:
        QString err = QString::fromUtf8(acta_db_strerror(rc));
        if (rc == ACTA_DB_ERR_SQL && detail && *detail)
            err = QStringLiteral("%1: %2")
                .arg(err, QString::fromUtf8(detail));
        return fallback.arg(noun, err);
    }
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

// Meaning of an execution status, for tooltips (UR #37). The status
// vocabulary is the DB's (acta_db/include/execution.h); unknown values
// fall back to echoing the raw status.
inline QString statusMeaning(const QString &status)
{
    const QString s = status.toLower();
    if (s == QLatin1String("pending"))
        return QObject::tr("Pending: created, but not started yet.");
    if (s == QLatin1String("running"))
        return QObject::tr("Running: the execution is currently in progress.");
    if (s == QLatin1String("completed"))
        return QObject::tr("Completed: the execution finished successfully.");
    if (s == QLatin1String("failed"))
        return QObject::tr("Failed: the execution ended with an error.");
    if (s == QLatin1String("cancelled"))
        return QObject::tr("Cancelled: the execution was stopped before completion.");
    return QObject::tr("Unknown status: %1").arg(status);
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

// Case-insensitive substring filter for a QTreeWidget (H4 / UR #38).
// A row is visible if it matches any column itself, or if any of its
// descendants matches, so folder rows stay visible above their
// matching children. An empty needle shows everything.
//
// `extraTextRole` (-1 = off) adds one more searchable payload stored in
// item data: the context panel stores each row's content there, since
// the list only shows Type + Date.
inline void applyTreeFilter(QTreeWidget *tree, const QString &needle,
                            int extraTextRole = -1)
{
    const QString hay = needle.trimmed();
    std::function<bool(QTreeWidgetItem *)> visit =
        [&](QTreeWidgetItem *item) -> bool {
        bool match = hay.isEmpty();
        if (!hay.isEmpty()) {
            for (int c = 0; c < item->columnCount(); ++c) {
                if (item->text(c).contains(hay, Qt::CaseInsensitive)) {
                    match = true;
                    break;
                }
            }
            if (!match && extraTextRole != -1) {
                const QString extra =
                    item->data(0, extraTextRole).toString();
                match = extra.contains(hay, Qt::CaseInsensitive);
            }
        }
        bool childMatch = false;
        for (int i = 0; i < item->childCount(); ++i)
            childMatch = visit(item->child(i)) || childMatch;
        const bool show = match || childMatch;
        item->setHidden(!show);
        return show;
    };
    for (int i = 0; i < tree->topLevelItemCount(); ++i)
        visit(tree->topLevelItem(i));
}

// Centered placeholder over an empty tree/view viewport (P5 / UR #31).
// Owned by the viewport, so it dies with the view. Shown/hidden by the
// caller in reload(); kept centered on resize via the panel's existing
// viewport eventFilter (QEvent::Resize -> placeEmptyStateLabel).
// Works for QTreeWidget and QTableView alike (both derive from
// QAbstractScrollArea, which is where viewport() lives).
inline QLabel *makeEmptyStateLabel(QAbstractScrollArea *view, const QString &text)
{
    auto *label = new QLabel(text, view->viewport());
    label->setObjectName(QStringLiteral("emptyState"));
    label->setAlignment(Qt::AlignCenter);
    label->setEnabled(false); // greyed via the stylesheet; no focus
    label->setGeometry(view->viewport()->rect());
    label->hide();
    return label;
}

inline void placeEmptyStateLabel(QLabel *label, QAbstractScrollArea *view)
{
    if (label)
        label->setGeometry(view->viewport()->rect());
}

// Icon-only toolbar button for the panel toolbars (P2 / UR #22): the
// tooltip carries the meaning (icons are shared across button groups,
// e.g. trash for "Delete" and "Delete Folder"), and the optional
// keyboard shortcut is the Alt+letter accelerator (UR #39).
// NoFocus: the button is only ever clicked or reached via its
// shortcut, never tabbed into.
inline QPushButton *makeActionButton(const QIcon &icon,
                                    const QString &tooltip,
                                    const QKeySequence &shortcut = QKeySequence())
{
    auto *btn = new QPushButton;
    btn->setIcon(icon);
    btn->setToolTip(tooltip);
    btn->setFocusPolicy(Qt::NoFocus);
    if (!shortcut.isEmpty())
        btn->setShortcut(shortcut);
    return btn;
}
