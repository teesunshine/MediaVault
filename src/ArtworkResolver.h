#pragma once

#include <QHash>
#include <QObject>
#include <QSet>
#include <QString>

class QNetworkAccessManager;

namespace mv {

struct TitleGuess {
    QString title;
    int year = 0;   // 0 = unknown
};

// "The.Matrix.1999.1080p.BluRay" (already prettified) -> {"The Matrix", 1999}.
// "Alien (1979)" -> {"Alien", 1979}. The last plausible year wins, so
// "Blade Runner 2049 (2017)" -> {"Blade Runner 2049", 2017}.
TitleGuess guessTitle(const QString &name);

// Finds a public picture URL for what is playing, so Discord can show it:
//   music  -> MusicBrainz search + Cover Art Archive (no key needed)
//   movies / TV -> TMDB posters (needs a free API key)
// Lookups are asynchronous and cached by `key`.
class ArtworkResolver : public QObject {
    Q_OBJECT
public:
    explicit ArtworkResolver(QObject *parent = nullptr);

    // Accepts either a TMDB v3 API key or a v4 "read access token" (starts "eyJ").
    void setTmdbKey(const QString &key);

    // True if `key` was already looked up. `url` is empty when nothing was found.
    bool cached(const QString &key, QString &url) const;

    void requestMusic(const QString &key, const QString &artist, const QString &album);
    void requestVideo(const QString &key, bool tv, const QString &title, int year);

signals:
    // Emitted when a lookup ends. `url` is empty if nothing was found.
    void resolved(const QString &key, const QString &url);

private:
    void searchTmdb(const QString &key, bool tv, const QString &title, int year);
    void finish(const QString &key, const QString &url, bool remember = true);
    void finishLater(const QString &key);

    QNetworkAccessManager *m_nam = nullptr;
    QString m_tmdbKey;
    QHash<QString, QString> m_cache;
    QSet<QString> m_inflight;
};

} // namespace mv
