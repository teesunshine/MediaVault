#include "DiscordSettingsDialog.h"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QRadioButton>
#include <QVBoxLayout>

namespace {
QLabel *hint(const QString &text)
{
    auto *l = new QLabel(text);
    l->setWordWrap(true);
    l->setTextFormat(Qt::RichText);
    l->setOpenExternalLinks(true);
    QPalette p = l->palette();
    p.setColor(QPalette::WindowText, p.color(QPalette::PlaceholderText));
    l->setPalette(p);
    return l;
}
} // namespace

DiscordSettingsDialog::DiscordSettingsDialog(const mv::DiscordSettings &s, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Discord Settings"));
    setMinimumWidth(480);

    m_clientId = new QLineEdit(s.clientId);
    m_clientId->setPlaceholderText(tr("e.g. 123456789012345678"));

    m_none = new QRadioButton(tr("No picture"));
    m_custom = new QRadioButton(tr("My own image (same picture every time)"));
    m_artwork = new QRadioButton(tr("Cover art and posters (album covers, movie and show posters)"));

    m_imageId = new QLineEdit(s.imageId);
    m_imageId->setPlaceholderText(tr("Image ID, e.g. logo"));
    m_tmdbKey = new QLineEdit(s.tmdbKey);
    m_tmdbKey->setPlaceholderText(tr("TMDB API key (only for movie and show posters)"));

    switch (s.imageMode) {
    case mv::DiscordSettings::ImageMode::Custom:  m_custom->setChecked(true); break;
    case mv::DiscordSettings::ImageMode::Artwork: m_artwork->setChecked(true); break;
    default:                                      m_none->setChecked(true); break;
    }

    auto *appForm = new QFormLayout;
    appForm->addRow(tr("Application ID"), m_clientId);

    auto *picBox = new QGroupBox(tr("Picture"));
    auto *pic = new QVBoxLayout(picBox);
    pic->addWidget(m_none);
    pic->addWidget(m_custom);
    pic->addWidget(m_artwork);
    pic->addWidget(new QLabel(tr("Image ID (used by \"My own image\", and as the fallback for cover art):")));
    pic->addWidget(m_imageId);
    pic->addWidget(hint(tr("Upload the picture in the <a href=\"https://discord.com/developers/applications\">"
                           "developer portal</a> under <i>Rich Presence → Art Assets</i> and enter the name you "
                           "gave it. A direct https:// image link also works.")));
    pic->addSpacing(8);
    pic->addWidget(new QLabel(tr("TMDB API key (movie and show posters):")));
    pic->addWidget(m_tmdbKey);
    pic->addWidget(hint(tr("Free at <a href=\"https://www.themoviedb.org/settings/api\">themoviedb.org</a>. "
                           "Album covers need no key. Lookups are done online, so the first time a title "
                           "plays its picture can take a moment to appear.")));

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *root = new QVBoxLayout(this);
    root->addLayout(appForm);
    root->addWidget(hint(tr("Create an application in the developer portal. Its name is what Discord "
                            "shows, e.g. \"Watching MediaVault\".")));
    root->addWidget(picBox);
    root->addWidget(buttons);

    for (auto *r : {m_none, m_custom, m_artwork})
        connect(r, &QRadioButton::toggled, this, &DiscordSettingsDialog::updateEnabled);
    updateEnabled();
}

void DiscordSettingsDialog::updateEnabled()
{
    m_imageId->setEnabled(!m_none->isChecked());
    m_tmdbKey->setEnabled(m_artwork->isChecked());
}

mv::DiscordSettings DiscordSettingsDialog::result() const
{
    mv::DiscordSettings s;
    s.clientId = m_clientId->text().trimmed();
    s.imageId = m_imageId->text().trimmed();
    s.tmdbKey = m_tmdbKey->text().trimmed();
    s.imageMode = m_custom->isChecked()    ? mv::DiscordSettings::ImageMode::Custom
                  : m_artwork->isChecked() ? mv::DiscordSettings::ImageMode::Artwork
                                           : mv::DiscordSettings::ImageMode::None;
    return s;
}
