#pragma once

#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QVector>

#include "MediaLibrary.h"

namespace mv {

// A theme is a folder with a theme.json:
//   {
//     "id": "san-andreas",            (optional, defaults to the folder name)
//     "name": "San Andreas",
//     "author": "you", "version": "1.0", "description": "...",
//     "entry": "index.html",          (optional) web page shown instead of the video when music is open
//     "qss": "app.qss",               (optional) Qt style sheet applied to the whole app
//     "fonts": ["fonts/A.ttf"]        (optional) fonts registered while the theme is active
//   }
// At least one of "entry" or "qss" must exist. All paths are relative to the
// theme folder and may not point outside it.
struct ThemeInfo {
    QString id;
    QString name;
    QString author;
    QString version;
    QString description;
    QString dir;            // absolute folder
    QString entryPath;      // absolute path of the web page, or empty
    QString qssPath;        // absolute path of the style sheet, or empty
    QStringList fontPaths;  // absolute paths
    bool hasEntry() const { return !entryPath.isEmpty(); }
    bool hasQss() const { return !qssPath.isEmpty(); }
};

bool loadThemeInfo(const QString &dir, ThemeInfo &out, QString *error = nullptr);

// Looks one level down in every root for theme folders. A later root replaces
// an earlier theme with the same id (so user themes can override bundled ones).
QVector<ThemeInfo> scanThemes(const QStringList &roots);

// Reads the theme's style sheet. "{{theme}}" is replaced by the theme folder's
// absolute path (forward slashes), so url({{theme}}/img/bg.png) works.
QString themeStyleSheet(const ThemeInfo &theme);

// --- What a web theme sees --------------------------------------------------

struct PlaybackSnapshot {
    QString kind = QStringLiteral("none");   // "music", "video" or "none"
    QString title, artist, album;
    QString cover;                           // file:// URL or empty
    bool playing = false;
    qint64 positionMs = 0;
    qint64 durationMs = 0;
    int volume = 80;                         // 0..100
    // What the panel should be showing: "nowplaying", "artist" or "album".
    QString viewType = QStringLiteral("nowplaying");
    QString viewArtist, viewAlbum;
};

QJsonObject snapshotToJson(const PlaybackSnapshot &s);

// {"artists":[{"name","image","albums":[{"name","cover","tracks":[{"title","path"}]}]}]}
// image/cover are file:// URLs (or ""), path is the file path to hand back to playTrack().
QJsonObject musicLibraryToJson(const Library &lib);

QString fileUrl(const QString &path);   // "" for an empty path

} // namespace mv
