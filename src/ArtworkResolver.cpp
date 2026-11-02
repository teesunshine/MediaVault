#include "ArtworkResolver.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>

namespace mv {

namespace {

QNetworkRequest makeRequest(const QUrl &url)
{
    QNetworkRequest r(url);
    // MusicBrainz asks API clients to identify themselves.
    r.setHeader(QNetworkRequest::UserAgentHeader,
                QStringLiteral("MediaVault/0.1 ( https://github.com/turnedmodzun-arch/MediaVault )"));
    r.setTransferTimeout(10000);
    return r;
}

} // namespace

TitleGuess guessTitle(const QString &name)
{
    QString s = name;
    s.replace(QRegularExpression(QStringLiteral("[\\(\\)\\[\\]]")), QStringLiteral(" "));

    TitleGuess g;
    static const QRegularExpression yearRe(QStringLiteral("(?<!\\d)((?:19|20)\\d{2})(?!\\d)"));

    // Last year that isn't at the very start (so a film called "2012" survives).
    qsizetype yearStart = -1;
    auto it = yearRe.globalMatch(s);
    while (it.hasNext()) {
        const auto m = it.next();
        if (m.capturedStart() > 0) {
            yearStart = m.capturedStart();
            g.year = m.captured(1).toInt();
        }
    }

    if (yearStart > 0) {
        s = s.left(yearStart);
    } else {
        static const QRegularExpression junk(
            QStringLiteral("\\b(480p|576p|720p|1080p|2160p|4k|bluray|brrip|bdrip|webrip|web-dl|hdtv|"
                           "dvdrip|x264|x265|h264|h265|hevc)\\b"),
            QRegularExpression::CaseInsensitiveOption);
        const auto m = junk.match(s);
        if (m.hasMatch() && m.capturedStart() > 0)
            s = s.left(m.capturedStart());
    }
    g.title = s.simplified();
    return g;
}

ArtworkResolver::ArtworkResolver(QObject *parent)
    : QObject(parent)
    , m_nam(new QNetworkAccessManager(this))
{
}

void ArtworkResolver::setTmdbKey(const QString &key)
{
    const QString k = key.trimmed();
    if (k == m_tmdbKey)
        return;
    m_tmdbKey = k;
    // Videos that failed only because there was no key deserve a retry.
    for (auto it = m_cache.begin(); it != m_cache.end();) {
        if (it.key().startsWith(QLatin1String("movie:")) || it.key().startsWith(QLatin1String("tv:")))
            it = m_cache.erase(it);
        else
            ++it;
    }
}

bool ArtworkResolver::cached(const QString &key, QString &url) const
{
    const auto it = m_cache.constFind(key);
    if (it == m_cache.constEnd())
        return false;
    url = it.value();
    return true;
}

void ArtworkResolver::finish(const QString &key, const QString &url, bool remember)
{
    m_inflight.remove(key);
    if (remember)
        m_cache.insert(key, url);
    emit resolved(key, url);
}

void ArtworkResolver::finishLater(const QString &key)
{
    m_cache.insert(key, QString());
    QTimer::singleShot(0, this, [this, key] { emit resolved(key, QString()); });
}

void ArtworkResolver::requestMusic(const QString &key, const QString &artist, const QString &album)
{
    if (m_cache.contains(key) || m_inflight.contains(key))
        return;
    if (artist.isEmpty() || album.isEmpty() || artist == QLatin1String("Unknown Artist")
        || album == QLatin1String("Singles")) {
        finishLater(key);   // folder names like these can't be looked up
        return;
    }
    m_inflight.insert(key);

    auto clean = [](QString s) { s.remove(QLatin1Char('"')); return s; };
    QUrl url(QStringLiteral("https://musicbrainz.org/ws/2/release-group/"));
    QUrlQuery q;
    q.addQueryItem(QStringLiteral("query"),
                   QStringLiteral("releasegroup:\"%1\" AND artist:\"%2\"").arg(clean(album), clean(artist)));
    q.addQueryItem(QStringLiteral("fmt"), QStringLiteral("json"));
    q.addQueryItem(QStringLiteral("limit"), QStringLiteral("1"));
    url.setQuery(q);

    QNetworkReply *reply = m_nam->get(makeRequest(url));
    connect(reply, &QNetworkReply::finished, this, [this, reply, key] {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            finish(key, QString(), false);   // offline etc: don't remember the failure
            return;
        }
        const auto groups = QJsonDocument::fromJson(reply->readAll()).object()
                                .value(QStringLiteral("release-groups")).toArray();
        QString id;
        if (!groups.isEmpty()) {
            const QJsonObject g = groups.at(0).toObject();
            if (g.value(QStringLiteral("score")).toInt() >= 80)   // ignore weak matches
                id = g.value(QStringLiteral("id")).toString();
        }
        if (id.isEmpty()) {
            finish(key, QString());
            return;
        }

        // Cover Art Archive answers with a redirect to the image when one exists
        // and 404 when it doesn't, so a HEAD request is enough to check.
        const QUrl art(QStringLiteral("https://coverartarchive.org/release-group/%1/front-500").arg(id));
        QNetworkRequest req = makeRequest(art);
        req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
        QNetworkReply *head = m_nam->head(req);
        connect(head, &QNetworkReply::finished, this, [this, head, key, art] {
            head->deleteLater();
            const int code = head->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            if (code == 0)
                finish(key, QString(), false);   // network problem
            else if (code == 200 || (code >= 300 && code < 400))
                finish(key, art.toString());
            else
                finish(key, QString());
        });
    });
}

void ArtworkResolver::requestVideo(const QString &key, bool tv, const QString &title, int year)
{
    if (m_cache.contains(key) || m_inflight.contains(key))
        return;
    if (m_tmdbKey.isEmpty() || title.isEmpty()) {
        finishLater(key);
        return;
    }
    m_inflight.insert(key);
    searchTmdb(key, tv, title, year);
}

void ArtworkResolver::searchTmdb(const QString &key, bool tv, const QString &title, int year)
{
    QUrl url(QStringLiteral("https://api.themoviedb.org/3/search/")
             + (tv ? QStringLiteral("tv") : QStringLiteral("movie")));
    QUrlQuery q;
    q.addQueryItem(QStringLiteral("query"), title);
    q.addQueryItem(QStringLiteral("include_adult"), QStringLiteral("false"));
    if (year > 0)
        q.addQueryItem(tv ? QStringLiteral("first_air_date_year") : QStringLiteral("year"),
                       QString::number(year));

    QNetworkRequest req;
    const bool bearer = m_tmdbKey.startsWith(QLatin1String("eyJ"));   // v4 token
    if (!bearer)
        q.addQueryItem(QStringLiteral("api_key"), m_tmdbKey);
    url.setQuery(q);
    req = makeRequest(url);
    if (bearer)
        req.setRawHeader("Authorization", "Bearer " + m_tmdbKey.toUtf8());

    QNetworkReply *reply = m_nam->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply, key, tv, title, year] {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            finish(key, QString(), false);   // bad key, offline, rate limit: try again later
            return;
        }
        const auto results = QJsonDocument::fromJson(reply->readAll()).object()
                                 .value(QStringLiteral("results")).toArray();
        for (const QJsonValue &v : results) {
            const QString path = v.toObject().value(QStringLiteral("poster_path")).toString();
            if (!path.isEmpty()) {
                finish(key, QStringLiteral("https://image.tmdb.org/t/p/w500") + path);
                return;
            }
        }
        if (year > 0)   // the year in a file name is sometimes wrong
            searchTmdb(key, tv, title, 0);
        else
            finish(key, QString());
    });
}

} // namespace mv
