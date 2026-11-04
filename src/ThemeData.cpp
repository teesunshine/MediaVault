#include "ThemeData.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QUrl>

namespace mv {

namespace {

// Resolves `rel` inside `base`. Returns "" if the file is missing or escapes
// the theme folder.
QString resolveInside(const QDir &base, const QString &rel)
{
    if (rel.trimmed().isEmpty())
        return {};
    const QString p = QDir::cleanPath(base.absoluteFilePath(rel));
    const QString root = QDir::cleanPath(base.absolutePath()) + QLatin1Char('/');
    if (!p.startsWith(root))
        return {};
    return QFileInfo(p).isFile() ? p : QString();
}

bool fail(QString *error, const QString &msg)
{
    if (error)
        *error = msg;
    return false;
}

} // namespace

QString fileUrl(const QString &path)
{
    return path.isEmpty() ? QString() : QUrl::fromLocalFile(path).toString();
}

bool loadThemeInfo(const QString &dir, ThemeInfo &out, QString *error)
{
    const QDir d(dir);
    QFile f(d.absoluteFilePath(QStringLiteral("theme.json")));
    if (!f.open(QIODevice::ReadOnly))
        return fail(error, QStringLiteral("no theme.json"));

    QJsonParseError pe;
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &pe);
    if (pe.error != QJsonParseError::NoError || !doc.isObject())
        return fail(error, QStringLiteral("theme.json is not valid JSON: %1").arg(pe.errorString()));
    const QJsonObject o = doc.object();

    ThemeInfo t;
    t.dir = d.absolutePath();
    t.id = o.value("id").toString().trimmed();
    if (t.id.isEmpty())
        t.id = QFileInfo(t.dir).fileName();
    t.name = o.value("name").toString().trimmed();
    if (t.name.isEmpty())
        t.name = t.id;
    t.author = o.value("author").toString();
    t.version = o.value("version").toString();
    t.description = o.value("description").toString();

    const QString layout = o.value("layout").toString().trimmed().toLower();
    if (!layout.isEmpty()) {
        if (layout != QLatin1String("full") && layout != QLatin1String("panel"))
            return fail(error, QStringLiteral("layout must be \"full\" or \"panel\""));
        t.layout = layout;
    }

    // "entry" defaults to index.html when that file exists.
    QString entry = o.value("entry").toString();
    if (entry.isEmpty() && QFileInfo::exists(d.absoluteFilePath(QStringLiteral("index.html"))))
        entry = QStringLiteral("index.html");
    if (!entry.isEmpty()) {
        t.entryPath = resolveInside(d, entry);
        if (t.entryPath.isEmpty())
            return fail(error, QStringLiteral("entry '%1' not found inside the theme folder").arg(entry));
    }

    const QString qss = o.value("qss").toString();
    if (!qss.isEmpty()) {
        t.qssPath = resolveInside(d, qss);
        if (t.qssPath.isEmpty())
            return fail(error, QStringLiteral("qss '%1' not found inside the theme folder").arg(qss));
    }

    for (const QJsonValue &v : o.value("fonts").toArray()) {
        const QString p = resolveInside(d, v.toString());
        if (!p.isEmpty())
            t.fontPaths << p;   // a missing font is not fatal
    }

    if (!t.hasEntry() && !t.hasQss())
        return fail(error, QStringLiteral("theme needs an 'entry' page or a 'qss' file"));

    out = t;
    return true;
}

QVector<ThemeInfo> scanThemes(const QStringList &roots)
{
    QVector<ThemeInfo> out;
    for (const QString &root : roots) {
        const QDir r(root);
        const QStringList dirs = r.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
        for (const QString &name : dirs) {
            ThemeInfo t;
            if (!loadThemeInfo(r.absoluteFilePath(name), t))
                continue;
            bool replaced = false;
            for (ThemeInfo &existing : out) {
                if (existing.id == t.id) {
                    existing = t;
                    replaced = true;
                    break;
                }
            }
            if (!replaced)
                out.push_back(t);
        }
    }
    return out;
}

QString themeStyleSheet(const ThemeInfo &theme)
{
    if (!theme.hasQss())
        return {};
    QFile f(theme.qssPath);
    if (!f.open(QIODevice::ReadOnly))
        return {};
    QString css = QString::fromUtf8(f.readAll());
    css.replace(QStringLiteral("{{theme}}"), theme.dir);   // QDir paths already use '/'
    return css;
}

QJsonObject snapshotToJson(const PlaybackSnapshot &s)
{
    QJsonObject view;
    view["type"] = s.viewType;
    view["artist"] = s.viewArtist;
    view["album"] = s.viewAlbum;

    QJsonObject o;
    o["kind"] = s.kind;
    o["title"] = s.title;
    o["artist"] = s.artist;
    o["album"] = s.album;
    o["show"] = s.show;
    o["season"] = s.season;
    o["episode"] = s.episode;
    o["path"] = s.path;
    o["cover"] = s.cover;
    o["playing"] = s.playing;
    o["position"] = static_cast<double>(s.positionMs);
    o["duration"] = static_cast<double>(s.durationMs);
    o["volume"] = s.volume;
    o["page"] = s.page;
    o["fullscreen"] = s.fullscreen;
    o["root"] = s.root;
    o["view"] = view;
    return o;
}

QJsonObject libraryToJson(const Library &lib)
{
    QJsonArray movies;
    for (const MediaItem &m : lib.movies) {
        QJsonObject mo;
        mo["title"] = m.title;
        mo["path"] = m.path;
        movies.append(mo);
    }

    QJsonArray shows;
    for (const Show &sh : lib.shows) {
        QJsonArray seasons;
        for (const Season &se : sh.seasons) {
            QJsonArray episodes;
            for (const MediaItem &ep : se.episodes) {
                QJsonObject eo;
                eo["title"] = ep.title;
                eo["season"] = ep.season;
                eo["episode"] = ep.episode;
                eo["path"] = ep.path;
                episodes.append(eo);
            }
            QJsonObject so;
            so["number"] = se.number;
            so["episodes"] = episodes;
            seasons.append(so);
        }
        QJsonObject sho;
        sho["name"] = sh.name;
        sho["seasons"] = seasons;
        shows.append(sho);
    }

    QJsonArray artists;
    for (const Artist &a : lib.artists) {
        QJsonArray albums;
        for (const Album &al : a.albums) {
            QJsonArray tracks;
            for (const MediaItem &t : al.tracks) {
                QJsonObject to;
                to["title"] = t.title;
                to["path"] = t.path;
                tracks.append(to);
            }
            QJsonObject ao;
            ao["name"] = al.name;
            ao["cover"] = fileUrl(al.coverPath);
            ao["tracks"] = tracks;
            albums.append(ao);
        }
        QJsonObject ao;
        ao["name"] = a.name;
        ao["image"] = fileUrl(a.imagePath);
        ao["albums"] = albums;
        artists.append(ao);
    }
    QJsonObject o;
    o["root"] = lib.root;
    o["movies"] = movies;
    o["shows"] = shows;
    o["artists"] = artists;
    return o;
}

} // namespace mv
