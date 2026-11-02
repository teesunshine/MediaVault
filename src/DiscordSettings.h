#pragma once

#include <QString>

namespace mv {

// User settings for Discord Rich Presence, stored with QSettings.
struct DiscordSettings {
    enum class ImageMode {
        None = 0,      // no picture
        Custom = 1,    // always show imageId
        Artwork = 2    // album covers / posters, falling back to imageId
    };

    QString clientId;                  // Application ID from the developer portal
    ImageMode imageMode = ImageMode::None;
    QString imageId;                   // asset key (or https URL)
    QString tmdbKey;                   // TMDB API key, for movie/show posters

    static DiscordSettings load();
    void save() const;
};

} // namespace mv
