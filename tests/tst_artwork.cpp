#include <QtTest>

#include "ArtworkResolver.h"
#include "DiscordPresence.h"

class TestArtwork : public QObject {
    Q_OBJECT

private slots:
    void guessTitle_data()
    {
        QTest::addColumn<QString>("input");
        QTest::addColumn<QString>("title");
        QTest::addColumn<int>("year");

        QTest::newRow("dots") << "The Matrix 1999 1080p BluRay" << "The Matrix" << 1999;
        QTest::newRow("parens") << "Alien (1979)" << "Alien" << 1979;
        QTest::newRow("year in title") << "Blade Runner 2049 (2017)" << "Blade Runner 2049" << 2017;
        QTest::newRow("title is a year") << "2012" << "2012" << 0;
        QTest::newRow("junk only") << "Some Movie 1080p BluRay x264" << "Some Movie" << 0;
        QTest::newRow("plain show") << "Breaking Bad" << "Breaking Bad" << 0;
        QTest::newRow("show with year") << "Doctor Who (2005)" << "Doctor Who" << 2005;
    }

    void guessTitle()
    {
        QFETCH(QString, input);
        QFETCH(QString, title);
        QFETCH(int, year);
        const mv::TitleGuess g = mv::guessTitle(input);
        QCOMPARE(g.title, title);
        QCOMPARE(g.year, year);
    }

    void resolverWithoutKeyAnswersEmpty()
    {
        mv::ArtworkResolver r;   // no TMDB key set
        QSignalSpy spy(&r, &mv::ArtworkResolver::resolved);
        r.requestVideo("movie:alien", false, "Alien", 1979);
        QVERIFY(spy.wait(1000));
        QCOMPARE(spy.at(0).at(0).toString(), QString("movie:alien"));
        QVERIFY(spy.at(0).at(1).toString().isEmpty());

        QString url = "x";
        QVERIFY(r.cached("movie:alien", url));
        QVERIFY(url.isEmpty());
    }

    void unlookableMusicIsSkipped()
    {
        mv::ArtworkResolver r;
        QSignalSpy spy(&r, &mv::ArtworkResolver::resolved);
        r.requestMusic("music:x", "Unknown Artist", "Singles");
        QVERIFY(spy.wait(1000));
        QVERIFY(spy.at(0).at(1).toString().isEmpty());
    }

    void activityAssets()
    {
        mv::Activity a;
        a.kind = mv::Activity::Kind::Listening;
        a.details = "In The Flesh";
        a.playing = true;
        a.largeImage = "https://example.com/cover.jpg";
        a.largeText = "The Wall";
        const QJsonObject assets = mv::buildActivity(a, 5'000'000).value("assets").toObject();
        QCOMPARE(assets.value("large_image").toString(), QString("https://example.com/cover.jpg"));
        QCOMPARE(assets.value("large_text").toString(), QString("The Wall"));

        a.largeImage.clear();
        QVERIFY(!mv::buildActivity(a, 5'000'000).contains("assets"));
    }
};

QTEST_MAIN(TestArtwork)
#include "tst_artwork.moc"
