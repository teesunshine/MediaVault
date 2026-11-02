#pragma once

#include <QDialog>

#include "DiscordSettings.h"

class QLineEdit;
class QRadioButton;

class DiscordSettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit DiscordSettingsDialog(const mv::DiscordSettings &initial, QWidget *parent = nullptr);

    mv::DiscordSettings result() const;

private:
    void updateEnabled();

    QLineEdit *m_clientId = nullptr;
    QLineEdit *m_imageId = nullptr;
    QLineEdit *m_tmdbKey = nullptr;
    QRadioButton *m_none = nullptr;
    QRadioButton *m_custom = nullptr;
    QRadioButton *m_artwork = nullptr;
};
