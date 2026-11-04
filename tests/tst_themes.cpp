#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QTemporaryDir>
#include <QtTest>

#include "ThemeData.h"

class TestThemes : public QObject {
    Q_OBJECT

    static void write(const QString &path, const QByteArray &data)
    {
        QDir().mkpath(QFileInfo(path).absolutePath());
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(data);
    }

private slots:
    void loadsWebTheme()
    {
        QTemporaryDir tmp;
        const QString d = tmp.path() + "/sa";
        write(d + "/theme.json",
              R"({"id":"san-andreas","name":"San Andreas","author":"me","entry":"index.html",
                  "qss":"app.qss","fonts":["fonts/a.ttf","fonts/missing.ttf"]})");
        write(d + "/index.html", "<html></html>");
        write(d + "/app.qss", "QWidget { background: url({{theme}}/bg.png); }");
        write(d + "/fonts/a.ttf", "x");

        mv::ThemeInfo t;
        QString err;
        QVERIFY2(mv::loadThemeInfo(d, t, &err), qPrintable(err));
        QCOMPARE(t.id, QString("san-andreas"));
        QCOMPARE(t.name, QString("San Andreas"));
        QVERIFY(t.hasEntry());
        QVERIFY(t.hasQss());
        QCOMPARE(t.fontPaths.size(), 1);   // the missing one is skipped
        QVERIFY(mv::themeStyleSheet(t).contains(t.dir + "/bg.png"));
        QVERIFY(!mv::themeStyleSheet(t).contains("{{theme}}"));
    }

    void defaultsAndValidation()
    {
        QTemporaryDir tmp;
        write(tmp.path() + "/plain/theme.json", R"({"name":"Plain"})");
        write(tmp.path() + "/plain/index.html", "x");
        mv::ThemeInfo t;
        QVERIFY(mv::loadThemeInfo(tmp.path() + "/plain", t));
        QCOMPARE(t.id, QString("plain"));   // folder name
        QVERIFY(t.hasEntry());              // index.html picked up automatically
        QVERIFY(t.isFullUi());              // layout defaults to the full UI
        QVERIFY(!t.isPanel());

        write(tmp.path() + "/empty/theme.json", R"({"name":"Nothing"})");
        QVERIFY(!mv::loadThemeInfo(tmp.path() + "/empty", t));   // no entry, no qss

        write(tmp.path() + "/bad/theme.json", "{ not json");
        QVERIFY(!mv::loadThemeInfo(tmp.path() + "/bad", t));
    }

    void layoutPanelAndInvalid()
    {
        QTemporaryDir tmp;
        write(tmp.path() + "/p/theme.json", R"({"layout":"panel"})");
        write(tmp.path() + "/p/index.html", "x");
        mv::ThemeInfo t;
        QVERIFY(mv::loadThemeInfo(tmp.path() + "/p", t));
        QVERIFY(t.isPanel());
        QVERIFY(!t.isFullUi());

        write(tmp.path() + "/q/theme.json", R"({"layout":"sideways"})");
        write(tmp.path() + "/q/index.html", "x");
        QVERIFY(!mv::loadThemeInfo(tmp.path() + "/q", t));
    }

    void rejectsPathsOutsideTheFolder()
    {
        QTemporaryDir tmp;
        write(tmp.path() + "/secret.html", "x");
        write(tmp.path() + "/evil/theme.json", R"({"entry":"../secret.html"})");
        mv::ThemeInfo t;
        QString err;
        QVERIFY(!mv::loadThemeInfo(tmp.path() + "/evil", t, &err));
        QVERIFY(!err.isEmpty());
    }

    void laterRootOverridesEarlier()
    {
        QTemporaryDir bundled, user;
        write(bundled.path() + "/classic/theme.json", R"({"id":"classic","name":"Bundled"})");
        write(bundled.path() + "/classic/index.html", "x");
        write(bundled.path() + "/other/theme.json", R"({"name":"Other"})");
        write(bundled.path() + "/other/index.html", "x");
        write(user.path() + "/my/theme.json", R"({"id":"classic","name":"Mine"})");
        write(user.path() + "/my/index.html", "x");

        const auto themes = mv::scanThemes({bundled.path(), user.path()});
        QCOMPARE(themes.size(), 2);
        for (const auto &t : themes)
            if (t.id == "classic")
                QCOMPARE(t.name, QString("Mine"));
    }

    void snapshotJson()
    {
        mv::PlaybackSnapshot s;
        s.kind = "music";
        s.title = "In The Flesh";
        s.artist = "Pink Floyd";
        s.album = "The Wall";
        s.playing = true;
        s.positionMs = 1500;
        s.durationMs = 200000;
        s.viewType = "album";
        s.viewArtist = "Pink Floyd";
        s.viewAlbum = "The Wall";
        s.show = "Firefly";
        s.season = 1;
        s.episode = 2;
        s.path = "/m/x.flac";
        s.page = "video";
        const QJsonObject o = mv::snapshotToJson(s);
        QCOMPARE(o["kind"].toString(), QString("music"));
        QCOMPARE(o["playing"].toBool(), true);
        QCOMPARE(o["position"].toDouble(), 1500.0);
        QCOMPARE(o["view"].toObject()["type"].toString(), QString("album"));
        QCOMPARE(o["show"].toString(), QString("Firefly"));
        QCOMPARE(o["episode"].toInt(), 2);
        QCOMPARE(o["path"].toString(), QString("/m/x.flac"));
        QCOMPARE(o["page"].toString(), QString("video"));
    }

    void libraryJson()
    {
        mv::Library lib;
        mv::Artist a;
        a.name = "Pink Floyd";
        mv::Album al;
        al.name = "The Wall";
        al.coverPath = "/music/PF/Wall/cover.jpg";
        mv::MediaItem t;
        t.title = "In The Flesh";
        t.path = "/music/PF/Wall/01.flac";
        al.tracks.push_back(t);
        a.albums.push_back(al);
        lib.artists.push_back(a);

        mv::MediaItem movie;
        movie.title = "Alien";
        movie.path = "/m/alien.mkv";
        lib.movies.push_back(movie);
        mv::Show show;
        show.name = "Firefly";
        mv::Season season;
        season.number = 1;
        mv::MediaItem ep;
        ep.title = "Serenity";
        ep.season = 1;
        ep.episode = 1;
        ep.path = "/m/ff/s01e01.mkv";
        season.episodes.push_back(ep);
        show.seasons.push_back(season);
        lib.shows.push_back(show);
        lib.root = "/m";

        const QJsonObject o = mv::libraryToJson(lib);
        QCOMPARE(o["root"].toString(), QString("/m"));
        QCOMPARE(o["movies"].toArray().at(0).toObject()["title"].toString(), QString("Alien"));
        const QJsonObject sh = o["shows"].toArray().at(0).toObject();
        QCOMPARE(sh["name"].toString(), QString("Firefly"));
        const QJsonObject e0 = sh["seasons"].toArray().at(0).toObject()["episodes"].toArray().at(0).toObject();
        QCOMPARE(e0["episode"].toInt(), 1);
        QCOMPARE(e0["path"].toString(), QString("/m/ff/s01e01.mkv"));
        const QJsonObject art = o["artists"].toArray().at(0).toObject();
        QCOMPARE(art["name"].toString(), QString("Pink Floyd"));
        QCOMPARE(art["image"].toString(), QString());
        const QJsonObject alb = art["albums"].toArray().at(0).toObject();
        QVERIFY(alb["cover"].toString().startsWith("file:///"));
        QCOMPARE(alb["tracks"].toArray().at(0).toObject()["path"].toString(),
                 QString("/music/PF/Wall/01.flac"));
    }
};

QTEST_APPLESS_MAIN(TestThemes)
#include "tst_themes.moc"
