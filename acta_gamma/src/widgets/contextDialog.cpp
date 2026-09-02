#include "contextDialog.h"
#include "ui_contextDialog.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QLocale>
#include <QMessageBox>
#include <QPushButton>

#include <QtGlobal>

#include <cstdlib>
#include <cstring>

// strdup is not part of the C standard; duplicate into a malloc block
// freed by free().
static char *dupString(const char *s)
{
    if (!s)
        return nullptr;
    const size_t n = std::strlen(s) + 1;
    char *copy = static_cast<char *>(std::malloc(n));
    if (copy)
        std::memcpy(copy, s, n);
    return copy;
}

namespace {

QString utf8(const char *s)
{
    return s ? QString::fromUtf8(s) : QString();
}

// created_at comes back from SQLite's now() as "yyyy-MM-dd HH:mm:ss".
QDateTime toDateTime(const char *iso)
{
    if (!iso || !*iso)
        return {};
    const QString s = QString::fromUtf8(iso);
    static const QList<Qt::DateFormat> formats = {
        Qt::ISODateWithMs, Qt::ISODate,
    };
    for (auto f : formats) {
        const QDateTime dt = QDateTime::fromString(s, f);
        if (dt.isValid())
            return dt;
    }
    return QLocale::c().toDateTime(s, "yyyy-MM-dd HH:mm:ss");
}

} // namespace

ContextDialog::ContextDialog(QWidget *parent)
    : QDialog(parent), ui(new Ui_contextDialog)
{
    ui->setupUi(this);
    setWindowTitle("Context");
    setMode(Mode::ReadOnly);
}

ContextDialog::~ContextDialog()
{
    delete ui;
}

void ContextDialog::setMode(Mode mode)
{
    m_mode = mode;
    const bool readOnly = (mode == Mode::ReadOnly);

    ui->buttonBox->setStandardButtons(
        readOnly ? QDialogButtonBox::StandardButton::Close
                 : QDialogButtonBox::StandardButton::Save |
                       QDialogButtonBox::StandardButton::Close);
    if (!readOnly) {
        connect(ui->buttonBox->button(QDialogButtonBox::StandardButton::Save),
                &QPushButton::clicked, this, &ContextDialog::onSaveClicked);
    }
    // A QDialogButtonBox added by the .ui file is not wired to
    // accept/reject by QDialog (that only happens via QDialog::setButtons),
    // so connect Close explicitly. setStandardButtons() recreates the
    // button widgets on every call, hence the connect goes after it.
    connect(ui->buttonBox->button(QDialogButtonBox::StandardButton::Close),
            &QPushButton::clicked, this, &QDialog::reject);

    ui->typeLineEdit->setReadOnly(readOnly);
    ui->contentTextEdit->setReadOnly(readOnly);
    ui->metaDataTextEdit->setReadOnly(readOnly);
    // The hash is always computed by the application, never typed in.
    ui->contextHashLineEdit->setReadOnly(true);
}

void ContextDialog::editContext(db_t *db, int contextId)
{
    m_db = db;

    int err = ACTA_DB_OK;
    context_t *c = acta_db_context_get(db, contextId, &err);
    if (!c) {
        if (err != ACTA_DB_OK)
            qWarning("acta_db_context_get(%d) failed: %s", contextId,
                     acta_db_strerror(err));
        return;
    }

    // Data
    ui->typeLineEdit->setText(utf8(c->type));
    ui->contentTextEdit->setPlainText(utf8(c->content));

    // Metadata
    ui->contextHashLineEdit->setText(utf8(c->content_hash));
    ui->dateTimeEdit->setDateTime(toDateTime(c->created_at));
    ui->metaDataTextEdit->setPlainText(utf8(c->metadata));

    acta_db_context_free(c);

    setMode(Mode::ReadOnly);
    setWindowTitle(QStringLiteral("Context: %1")
                       .arg(ui->typeLineEdit->text()));
}

void ContextDialog::newContext(db_t *db)
{
    m_db = db;

    ui->typeLineEdit->clear();
    ui->contentTextEdit->clear();
    ui->contextHashLineEdit->clear();
    ui->dateTimeEdit->setDateTime(QDateTime());
    ui->metaDataTextEdit->clear();

    setMode(Mode::New);
    setWindowTitle("New Context");
}

void ContextDialog::onSaveClicked()
{
    if (!m_db || m_mode != Mode::New)
        return;

    const QString type = ui->typeLineEdit->text().trimmed();
    const QString content = ui->contentTextEdit->toPlainText();
    if (type.isEmpty() || content.isEmpty()) {
        QMessageBox::warning(this, "Context",
                             "Type and content are required.");
        return;
    }

    const QByteArray typeBa = type.toUtf8();
    const QByteArray contentBa = content.toUtf8();
    const QByteArray metaDataBa =
        ui->metaDataTextEdit->toPlainText().toUtf8();

    context_t c{};
    c.type = dupString(typeBa.constData());
    c.content = dupString(contentBa.constData());
    // Content hash: SHA-256 of the content, lowercase hex.
    const QByteArray hash =
        QCryptographicHash::hash(contentBa, QCryptographicHash::Sha256)
            .toHex();
    c.content_hash = dupString(hash.constData());
    c.metadata = dupString(metaDataBa.isEmpty() ? nullptr
                                                : metaDataBa.constData());

    int newId = 0;
    int rc = acta_db_context_create(m_db, &c, &newId);
    std::free(c.type);
    std::free(c.content);
    std::free(c.content_hash);
    std::free(c.metadata);
    if (rc != ACTA_DB_OK) {
        QMessageBox::warning(
            this, "Context",
            QStringLiteral("Could not create context: %1")
                .arg(acta_db_strerror(rc)));
        return;
    }

    // Contexts are immutable: show the created row read-only.
    editContext(m_db, newId);
}
