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
//     "entry": "index.html",          (optional) web page; see "layout"
//     "layout": "full" | "panel",     "full" (default): the page IS the whole app window
//                                     "panel": legacy layout, page replaces the video area for music
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
    QString layout = QStringLiteral("full");   // "full" or "panel", see above
    QString dir;            // absolute folder
    QString entryPath;      // absolute path of the web page, or empty
    QString qssPath;        // absolute path of the style sheet, or empty
    QStringList fontPaths;  // absolute paths
    bool hasEntry() const { return !entryPath.isEmpty(); }
    bool hasQss() const { return !qssPath.isEmpty(); }
    bool isFullUi() const { return hasEntry() && layout == QLatin1String("full"); }
    bool isPanel() const { return hasEntry() && layout == QLatin1String("panel"); }
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
    QString show;                            // TV shows: the show's name (title is the episode title)
    int season = 0, episode = 0;             // TV shows
    QString path;                            // file path of what's loaded, or empty
    QString cover;                           // file:// URL or empty
    bool playing = false;
    qint64 positionMs = 0;
    qint64 durationMs = 0;
    int volume = 80;                         // 0..100
    // Full-UI themes: "ui" while the web page is showing, "video" while the
    // native video screen is.
    QString page = QStringLiteral("ui");
    bool fullscreen = false;   // the app window is fullscreen
    QString root;                            // media folder, or empty if none chosen yet
    // What the panel should be showing: "nowplaying", "artist" or "album".
    QString viewType = QStringLiteral("nowplaying");
    QString viewArtist, viewAlbum;
};

QJsonObject snapshotToJson(const PlaybackSnapshot &s);

// {"root": "...",
//  "movies": [{"title","path"}],
//  "shows":  [{"name","seasons":[{"number","episodes":[{"title","season","episode","path"}]}]}],
//  "artists":[{"name","image","albums":[{"name","cover","tracks":[{"title","path"}]}]}]}
// image/cover are file:// URLs (or ""), path is the file path to hand back to playMedia().
QJsonObject libraryToJson(const Library &lib);

QString fileUrl(const QString &path);   // "" for an empty path

} // namespace mv
