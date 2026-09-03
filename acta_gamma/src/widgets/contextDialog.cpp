#include "contextDialog.h"
#include "ui_contextDialog.h"

#include <QCryptographicHash>
#include <QMessageBox>
#include <QPushButton>

#include <QtGlobal>

#include <cstdlib>

#include "util.h"

ContextDialog::ContextDialog(QWidget *parent)
    : QDialog(parent), ui(new Ui_contextDialog)
{
    ui->setupUi(this);
    setWindowTitle(tr("Context"));
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
    m_saved = false;
    m_newId = 0;

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
    ui->dateTimeEdit->setToolTip(utf8(c->created_at));
    ui->metaDataTextEdit->setPlainText(utf8(c->metadata));

    acta_db_context_free(c);

    setMode(Mode::ReadOnly);
    setWindowTitle(tr("Context: %1")
                       .arg(ui->typeLineEdit->text()));
}

void ContextDialog::newContext(db_t *db)
{
    m_db = db;
    m_saved = false;
    m_newId = 0;

    ui->typeLineEdit->clear();
    ui->contentTextEdit->clear();
    ui->contextHashLineEdit->clear();
    ui->dateTimeEdit->setDateTime(QDateTime());
    ui->metaDataTextEdit->clear();

    setMode(Mode::New);
    setWindowTitle(tr("New Context"));
}

void ContextDialog::onSaveClicked()
{
    if (!m_db || m_mode != Mode::New)
        return;

    const QString type = ui->typeLineEdit->text().trimmed();
    const QString content = ui->contentTextEdit->toPlainText();
    if (type.isEmpty() || content.isEmpty()) {
        QMessageBox::warning(this, tr("Context"),
                             tr("Type and content are required."));
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
            this, tr("Context"),
            friendlyDbError(rc, tr("context"), type,
                            tr("Could not create %1: %2"),
                            acta_db_last_error(m_db)));
        return;
    }

    m_newId = newId;
    // Contexts are immutable: show the created row read-only.
    editContext(m_db, newId);
    // The create was persisted; report it. Must come after
    // editContext(), which resets m_saved.
    m_saved = true;
}
