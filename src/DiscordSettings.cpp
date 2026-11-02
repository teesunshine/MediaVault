#include "DiscordSettings.h"

#include <QSettings>

namespace mv {

DiscordSettings DiscordSettings::load()
{
    QSettings s;
    DiscordSettings d;
    d.clientId = s.value("discord/clientId").toString().trimmed();
    const int mode = s.value("discord/imageMode", 0).toInt();
    d.imageMode = (mode == 1) ? ImageMode::Custom : (mode == 2) ? ImageMode::Artwork : ImageMode::None;
    d.imageId = s.value("discord/imageId").toString().trimmed();
    d.tmdbKey = s.value("discord/tmdbKey").toString().trimmed();
    return d;
}

void DiscordSettings::save() const
{
    QSettings s;
    s.setValue("discord/clientId", clientId.trimmed());
    s.setValue("discord/imageMode", static_cast<int>(imageMode));
    s.setValue("discord/imageId", imageId.trimmed());
    s.setValue("discord/tmdbKey", tmdbKey.trimmed());
}

} // namespace mv
