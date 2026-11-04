#include "MainWindow.h"

#include "ArtworkResolver.h"
#include "DiscordPresence.h"
#include "DiscordSettingsDialog.h"
#include "ThemeBridge.h"
#include "ThemePanel.h"

#include <QAction>
#include <QApplication>
#include <QActionGroup>
#include <QAudioOutput>
#include <QCoreApplication>
#include <QCursor>
#include <QDesktopServices>
#include <QDir>
#include <QEvent>
#include <QFileDialog>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QMediaPlayer>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QShortcut>
#include <QSlider>
#include <QSplitter>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QStatusBar>
#include <QStyle>
#include <QTimer>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <QUrl>
#include <QVBoxLayout>
#include <QVideoWidget>

namespace {
constexpr int kPathRole = Qt::UserRole;
constexpr int kCoverRole = Qt::UserRole + 1;   // album cover file (music leaves and albums)
constexpr int kTitleRole = Qt::UserRole + 2;   // clean episode title (TV leaves)
constexpr int kSeasonRole = Qt::UserRole + 3;
constexpr int kEpisodeRole = Qt::UserRole + 4;
constexpr const char *kLegacyId = "legacy";
constexpr const char *kThemeSetting = "ui/theme";
constexpr const char *kRootKey = "library/root";
constexpr const char *kDiscordEnabledKey = "discord/enabled";

QString episodeLabel(const mv::MediaItem &ep)
{
    if (ep.episode > 0)
        return QStringLiteral("E%1 – %2").arg(ep.episode, 2, 10, QLatin1Char('0')).arg(ep.title);
    return ep.title;
}

QTreeWidgetItem *makeLeaf(QTreeWidgetItem *parent, const QString &text, const QString &path)
{
    auto *it = new QTreeWidgetItem(parent, QStringList(text));
    it->setData(0, kPathRole, path);
    return it;
}
} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle("MediaVault");
    resize(1200, 720);

    // --- Library tree ---------------------------------------------------
    m_search = new QLineEdit;
    m_search->setPlaceholderText(tr("Filter…"));
    m_search->setClearButtonEnabled(true);

    m_tree = new QTreeWidget;
    m_tree->setHeaderHidden(true);
    m_tree->setUniformRowHeights(true);

    m_left = new QWidget;
    auto *leftLayout = new QVBoxLayout(m_left);
    leftLayout->setContentsMargins(6, 6, 0, 6);
    leftLayout->addWidget(m_search);
    leftLayout->addWidget(m_tree);

    // --- Player ---------------------------------------------------------
    m_video = new QVideoWidget;
    m_video->setMinimumSize(320, 180);
    m_video->setStyleSheet("background:black;");

    m_nowPlaying = new QLabel(tr("Nothing playing"));
    m_nowPlaying->setAlignment(Qt::AlignCenter);

    auto makeBtn = [this](QStyle::StandardPixmap icon, const QString &tip) {
        auto *b = new QPushButton(style()->standardIcon(icon), QString());
        b->setToolTip(tip);
        b->setFlat(true);
        return b;
    };
    auto *prevBtn = makeBtn(QStyle::SP_MediaSkipBackward, tr("Previous"));
    m_playBtn = makeBtn(QStyle::SP_MediaPlay, tr("Play / Pause (Space)"));
    auto *stopBtn = makeBtn(QStyle::SP_MediaStop, tr("Stop"));
    auto *nextBtn = makeBtn(QStyle::SP_MediaSkipForward, tr("Next"));
    auto *fullBtn = makeBtn(QStyle::SP_TitleBarMaxButton, tr("Fullscreen (F11)"));

    m_seek = new QSlider(Qt::Horizontal);
    m_seek->setRange(0, 0);
    m_timeLabel = new QLabel("0:00 / 0:00");
    m_volume = new QSlider(Qt::Horizontal);
    m_volume->setRange(0, 100);
    m_volume->setValue(80);
    m_volume->setMaximumWidth(110);
    m_volume->setToolTip(tr("Volume"));

    m_controlsBar = new QWidget;
    auto *controls = new QHBoxLayout(m_controlsBar);
    controls->setContentsMargins(0, 0, 0, 0);
    controls->addWidget(prevBtn);
    controls->addWidget(m_playBtn);
    controls->addWidget(stopBtn);
    controls->addWidget(nextBtn);
    controls->addWidget(m_seek, 1);
    controls->addWidget(m_timeLabel);
    controls->addWidget(m_volume);
    controls->addWidget(fullBtn);

    m_right = new QWidget;
    auto *rightLayout = new QVBoxLayout(m_right);
    rightLayout->setContentsMargins(0, 6, 6, 6);
    m_stage = new QStackedWidget;
    m_stage->addWidget(m_video);   // the theme panel is added when a web theme is first used
    rightLayout->addWidget(m_stage, 1);
    rightLayout->addWidget(m_nowPlaying);
    rightLayout->addWidget(m_controlsBar);

    // Two layouts share the same player widgets:
    //  - Legacy: library list on the left, player on the right.
    //  - Full UI: a theme's web page is the whole window. The video widget floats
    //    over the region the page reserves, so the page's own bars stay around it.
    m_splitter = new QSplitter;
    m_splitter->addWidget(m_left);
    m_splitter->addWidget(m_right);
    m_splitter->setStretchFactor(0, 0);
    m_splitter->setStretchFactor(1, 1);
    m_splitter->setSizes({340, 860});
    m_fullPage = new QWidget;
    auto *fullLayout = new QVBoxLayout(m_fullPage);
    fullLayout->setContentsMargins(0, 0, 0, 0);
    m_shell = new QStackedWidget;
    m_shell->addWidget(m_splitter);
    m_shell->addWidget(m_fullPage);
    setCentralWidget(m_shell);

    // --- Playback engine -----------------------------------------------
    m_player = new QMediaPlayer(this);
    m_audio = new QAudioOutput(this);
    m_audio->setVolume(0.8f);
    m_player->setAudioOutput(m_audio);
    m_player->setVideoOutput(m_video);

    // --- Menu -----------------------------------------------------------
    auto *fileMenu = menuBar()->addMenu(tr("&File"));
    auto *openAct = fileMenu->addAction(tr("&Open Media Folder…"), QKeySequence::Open, this, &MainWindow::chooseFolder);
    Q_UNUSED(openAct);
    fileMenu->addAction(tr("&Rescan Library"), QKeySequence::Refresh, this, &MainWindow::rescan);
    fileMenu->addSeparator();

    // Discord Rich Presence ("Watching ..." / "Listening to ...")
    m_discord = new mv::DiscordPresence(this);
    m_art = new mv::ArtworkResolver(this);
    connect(m_art, &mv::ArtworkResolver::resolved, this, [this](const QString &key, const QString &url) {
        // Only act when a picture was found; on failure the fallback is already
        // showing, and refreshing would just retry the failed lookup.
        if (key == m_artKey && !url.isEmpty())
            updatePresence();
    });
    applyDiscordSettings(mv::DiscordSettings::load());
    connect(m_discord, &mv::DiscordPresence::statusChanged, this,
            [this](const QString &text) { statusBar()->showMessage(text, 6000); });
    m_discordToggle = fileMenu->addAction(tr("Show on &Discord"));
    m_discordToggle->setCheckable(true);
    m_discordToggle->setChecked(QSettings().value(kDiscordEnabledKey, false).toBool());
    connect(m_discordToggle, &QAction::toggled, this, &MainWindow::onDiscordToggled);
    // Restore last session's choice (only if an Application ID is saved).
    if (m_discordToggle->isChecked() && !m_discordCfg.clientId.isEmpty())
        m_discord->setEnabled(true);
    else
        m_discordToggle->setChecked(false);
    fileMenu->addAction(tr("Discord &Settings…"), this, &MainWindow::openDiscordSettings);
    fileMenu->addSeparator();
    fileMenu->addAction(tr("&Quit"), QKeySequence::Quit, this, &QWidget::close);

    auto *viewMenu = menuBar()->addMenu(tr("&View"));
    m_themeMenu = viewMenu->addMenu(tr("&Theme"));
    m_themeGroup = new QActionGroup(this);
    m_themeGroup->setExclusive(true);

    // --- Wiring ---------------------------------------------------------
    connect(m_tree, &QTreeWidget::itemActivated, this, &MainWindow::onItemActivated);
    connect(m_tree, &QTreeWidget::itemDoubleClicked, this, &MainWindow::onItemActivated);
    connect(m_tree, &QTreeWidget::itemClicked, this, &MainWindow::onItemClicked);
    connect(m_search, &QLineEdit::textChanged, this, &MainWindow::applyFilter);

    connect(m_playBtn, &QPushButton::clicked, this, &MainWindow::togglePlay);
    connect(stopBtn, &QPushButton::clicked, m_player, &QMediaPlayer::stop);
    connect(nextBtn, &QPushButton::clicked, this, &MainWindow::playNext);
    connect(prevBtn, &QPushButton::clicked, this, &MainWindow::playPrevious);
    connect(fullBtn, &QPushButton::clicked, this, &MainWindow::toggleFullscreen);
    // The theme draws its own status bar in the full UI: forward messages to it.
    connect(statusBar(), &QStatusBar::messageChanged, this, [this](const QString &text) {
        if (m_layoutMode == LayoutMode::Full && m_themePanel && !text.isEmpty())
            m_themePanel->bridge()->postStatus(text);
    });

    connect(m_volume, &QSlider::valueChanged, this, [this](int v) {
        m_audio->setVolume(v / 100.0f);
        pushThemeState();
    });

    connect(m_seek, &QSlider::sliderPressed, this, [this] { m_seeking = true; });
    connect(m_seek, &QSlider::sliderReleased, this, [this] {
        m_seeking = false;
        seekTo(m_seek->value());
    });

    connect(m_player, &QMediaPlayer::durationChanged, this, [this](qint64 d) {
        m_seek->setRange(0, static_cast<int>(d));
        m_timeLabel->setText(formatTime(m_player->position()) + " / " + formatTime(d));
        updatePresence();   // duration is now known, so the progress bar can show
    });
    connect(m_player, &QMediaPlayer::positionChanged, this, [this](qint64 p) {
        if (!m_seeking)
            m_seek->setValue(static_cast<int>(p));
        m_timeLabel->setText(formatTime(p) + " / " + formatTime(m_player->duration()));
        if (m_themePushTimer.elapsed() > 500) {   // keep the web theme's clock moving
            m_themePushTimer.restart();
            pushThemeState();
        }
    });
    connect(m_player, &QMediaPlayer::playbackStateChanged, this, [this](QMediaPlayer::PlaybackState s) {
        m_playBtn->setIcon(style()->standardIcon(
            s == QMediaPlayer::PlayingState ? QStyle::SP_MediaPause : QStyle::SP_MediaPlay));
        updatePresence();
    });
    connect(m_player, &QMediaPlayer::mediaStatusChanged, this, [this](QMediaPlayer::MediaStatus s) {
        if (s == QMediaPlayer::EndOfMedia)
            playNext();   // auto-advance through the list
    });
    connect(m_player, &QMediaPlayer::errorOccurred, this,
            [this](QMediaPlayer::Error, const QString &msg) {
                statusBar()->showMessage(tr("Playback error: %1").arg(msg), 8000);
            });

    // Fullscreen: the whole window goes fullscreen; controls auto-hide and
    // reappear on mouse movement. Esc / F11 / double-click on the video exit.
    m_hideTimer = new QTimer(this);
    m_hideTimer->setSingleShot(true);
    m_hideTimer->setInterval(3000);
    connect(m_hideTimer, &QTimer::timeout, this, [this] {
        if (isFullScreen() && !m_controlsBar->underMouse()) {
            m_controlsBar->hide();
            m_video->setCursor(Qt::BlankCursor);
            m_stage->setCursor(Qt::BlankCursor);
        } else if (isFullScreen()) {
            m_hideTimer->start();
        }
    });
    // The video surface can swallow mouse events (especially once it has been
    // resized to fill the screen), so watch the cursor position directly.
    m_pollTimer = new QTimer(this);
    m_pollTimer->setInterval(100);
    connect(m_pollTimer, &QTimer::timeout, this, [this] {
        if (!isFullScreen())
            return;
        const QPoint pos = QCursor::pos();
        if (pos != m_lastCursor) {
            m_lastCursor = pos;
            showControls();   // shows the bar and restarts the hide countdown
        }
    });
    m_video->setMouseTracking(true);
    m_video->installEventFilter(this);
    new QShortcut(Qt::Key_Escape, this, this, [this] {
        if (isFullScreen())
            setFullscreenMode(false);
    });

    new QShortcut(Qt::Key_Space, this, this, &MainWindow::togglePlay);
    new QShortcut(Qt::Key_F11, this, this, &MainWindow::toggleFullscreen);
    new QShortcut(Qt::Key_Right, this, this, [this] { seekTo(m_player->position() + 10000); });
    new QShortcut(Qt::Key_Left, this, this, [this] { seekTo(qMax<qint64>(0, m_player->position() - 10000)); });

    m_themePushTimer.start();
    reloadThemes();
    // Classic (the full web UI) is the default; "legacy" is the original layout.
    applyTheme(QSettings().value(kThemeSetting, QStringLiteral("classic")).toString());
}

void MainWindow::openSavedLibrary()
{
    const QString saved = QSettings().value(kRootKey).toString();
    if (!saved.isEmpty())
        openLibrary(saved);
    else
        statusBar()->showMessage(tr("Choose your media folder with File > Open Media Folder…"));
}

void MainWindow::chooseFolder()
{
    const QString start = m_root.isEmpty() ? QDir::homePath() : m_root;
    const QString dir = QFileDialog::getExistingDirectory(
        this, tr("Select media folder (containing Music, Movies and TV Shows)"), start);
    if (!dir.isEmpty())
        openLibrary(dir);
}

void MainWindow::rescan()
{
    if (!m_root.isEmpty())
        openLibrary(m_root);
}

void MainWindow::openLibrary(const QString &root)
{
    m_root = root;
    QSettings().setValue(kRootKey, root);

    m_player->stop();
    m_current = nullptr;
    m_library = mv::scanLibrary(root);
    populate();

    setWindowTitle(QStringLiteral("MediaVault – %1").arg(root));
    QString msg = tr("%1 movies, %2 shows, %3 artists")
                      .arg(m_library.movies.size())
                      .arg(m_library.shows.size())
                      .arg(m_library.artists.size());
    if (!m_library.warnings.isEmpty())
        msg += QStringLiteral("  |  ") + m_library.warnings.join(QStringLiteral("; "));
    statusBar()->showMessage(msg);
    pushThemeState();   // themes show the new folder (and an empty "now playing")
}

void MainWindow::populate()
{
    m_tree->clear();

    auto *movies = new QTreeWidgetItem(m_tree, QStringList(tr("Movies (%1)").arg(m_library.movies.size())));
    for (const auto &m : m_library.movies)
        makeLeaf(movies, m.title, m.path);

    auto *tv = new QTreeWidgetItem(m_tree, QStringList(tr("TV Shows (%1)").arg(m_library.shows.size())));
    for (const auto &show : m_library.shows) {
        auto *showItem = new QTreeWidgetItem(tv, QStringList(show.name));
        for (const auto &season : show.seasons) {
            const QString label = season.number > 0 ? tr("Season %1").arg(season.number) : tr("Episodes");
            auto *seasonItem = new QTreeWidgetItem(showItem, QStringList(label));
            for (const auto &ep : season.episodes) {
                QTreeWidgetItem *leaf = makeLeaf(seasonItem, episodeLabel(ep), ep.path);
                leaf->setData(0, kTitleRole, ep.title);
                leaf->setData(0, kSeasonRole, ep.season);
                leaf->setData(0, kEpisodeRole, ep.episode);
            }
        }
    }

    auto *music = new QTreeWidgetItem(m_tree, QStringList(tr("Music (%1)").arg(m_library.artists.size())));
    for (const auto &artist : m_library.artists) {
        auto *artistItem = new QTreeWidgetItem(music, QStringList(artist.name));
        for (const auto &album : artist.albums) {
            auto *albumItem = new QTreeWidgetItem(artistItem, QStringList(album.name));
            albumItem->setData(0, kCoverRole, album.coverPath);
            for (const auto &t : album.tracks)
                makeLeaf(albumItem, t.title, t.path)->setData(0, kCoverRole, album.coverPath);
        }
    }

    if (m_themePanel)
        m_themePanel->bridge()->setLibrary(libraryJson());

    m_tree->expandToDepth(0);
    applyFilter(m_search->text());
}

QList<QTreeWidgetItem *> MainWindow::leafItems() const
{
    QList<QTreeWidgetItem *> out;
    QTreeWidgetItemIterator it(m_tree);
    while (*it) {
        if ((*it)->data(0, kPathRole).isValid())
            out << *it;
        ++it;
    }
    return out;
}

void MainWindow::onItemActivated(QTreeWidgetItem *item, int)
{
    if (item && item->data(0, kPathRole).isValid()) {
        if (categoryOf(item) == 2) {
            // Started from the library list: show the now-playing page.
            m_viewType = QStringLiteral("nowplaying");
            m_viewArtist.clear();
            m_viewAlbum.clear();
        }
        playItem(item);
    }
}

int MainWindow::categoryOf(QTreeWidgetItem *item) const
{
    if (!item)
        return -1;
    while (item->parent())
        item = item->parent();
    return m_tree->indexOfTopLevelItem(item);
}

void MainWindow::onItemClicked(QTreeWidgetItem *item, int)
{
    // Clicking an artist or album in Music opens its page in the theme.
    if (!item || categoryOf(item) != 2 || item->data(0, kPathRole).isValid() || !item->parent())
        return;
    if (!item->parent()->parent())
        showMusicView(QStringLiteral("artist"), item->text(0), QString());
    else
        showMusicView(QStringLiteral("album"), item->parent()->text(0), item->text(0));
}

void MainWindow::showMusicView(const QString &type, const QString &artist, const QString &album)
{
    m_viewType = type;
    m_viewArtist = artist;
    m_viewAlbum = album;
    m_musicStage = true;
    updateStage();
    pushThemeState();
}

void MainWindow::playPath(const QString &path)
{
    for (QTreeWidgetItem *leaf : leafItems()) {
        if (leaf->data(0, kPathRole).toString() == path) {
            m_tree->setCurrentItem(leaf);
            playItem(leaf);
            // Full UI: video plays on the native screen, music stays in the theme.
            if (m_layoutMode == LayoutMode::Full) {
                if (categoryOf(leaf) == 2)
                    showLibraryPage();
                else
                    showVideoPage();
            }
            return;
        }
    }
}

void MainWindow::playItem(QTreeWidgetItem *item)
{
    const QString path = item->data(0, kPathRole).toString();
    m_current = item;
    m_musicStage = categoryOf(item) == 2;
    updateStage();
    m_player->setSource(QUrl::fromLocalFile(path));
    m_player->play();

    // Breadcrumb: "Show > Season 1 > E02 – Title"
    QStringList crumbs;
    for (QTreeWidgetItem *p = item; p && p->parent(); p = p->parent())
        crumbs.prepend(p->text(0));
    m_nowPlaying->setText(crumbs.join(QStringLiteral("  ›  ")));
    updatePresence(0);
}

void MainWindow::seekTo(qint64 ms)
{
    m_player->setPosition(ms);
    updatePresence(ms);
}

void MainWindow::updatePresence(qint64 positionOverrideMs)
{
    pushThemeState();   // every playback change goes to the web theme too
    if (!m_discord)
        return;

    mv::Activity a;
    if (!m_current || m_player->playbackState() == QMediaPlayer::StoppedState) {
        m_discord->setActivity(a);   // Kind::None clears the status
        return;
    }

    // Crumbs below the category row: movie [title]; TV [show, season, episode];
    // music [artist, album, track]. The category is the top-level row's index.
    QStringList crumbs;
    QTreeWidgetItem *top = m_current;
    for (; top->parent(); top = top->parent())
        crumbs.prepend(top->text(0));
    const int category = m_tree->indexOfTopLevelItem(top);   // 0 movies, 1 TV, 2 music

    if (category == 2 && crumbs.size() >= 3) {
        a.kind = mv::Activity::Kind::Listening;
        a.details = crumbs.at(2);
        a.state = tr("by %1 – %2").arg(crumbs.at(0), crumbs.at(1));
    } else if (category == 1 && crumbs.size() >= 3) {
        a.kind = mv::Activity::Kind::Watching;
        a.details = crumbs.at(0);
        a.state = tr("%1 · %2").arg(crumbs.at(1), crumbs.at(2));
    } else {
        a.kind = category == 2 ? mv::Activity::Kind::Listening : mv::Activity::Kind::Watching;
        a.details = crumbs.isEmpty() ? m_current->text(0) : crumbs.last();
        a.state = category == 0 ? tr("Movie") : QString();
    }

    // Picture: nothing, the user's own image, or looked-up cover art / posters
    // (falling back to the user's image until the lookup finishes or if it fails).
    QString image;
    QString imageText;
    m_artKey.clear();
    if (m_discordCfg.imageMode != mv::DiscordSettings::ImageMode::None) {
        image = m_discordCfg.imageId;
        if (m_discordCfg.imageMode == mv::DiscordSettings::ImageMode::Artwork) {
            if (category == 2 && crumbs.size() >= 3) {
                m_artKey = QStringLiteral("music:%1|%2").arg(crumbs.at(0), crumbs.at(1)).toLower();
                m_art->requestMusic(m_artKey, crumbs.at(0), crumbs.at(1));
                imageText = crumbs.at(1);
            } else if (category == 1 && crumbs.size() >= 3) {
                const mv::TitleGuess g = mv::guessTitle(crumbs.at(0));
                m_artKey = QStringLiteral("tv:%1|%2").arg(g.title.toLower()).arg(g.year);
                m_art->requestVideo(m_artKey, true, g.title, g.year);
                imageText = crumbs.at(0);
            } else if (category == 0) {
                const mv::TitleGuess g = mv::guessTitle(a.details);
                m_artKey = QStringLiteral("movie:%1|%2").arg(g.title.toLower()).arg(g.year);
                m_art->requestVideo(m_artKey, false, g.title, g.year);
                imageText = a.details;
            }
            QString url;
            if (!m_artKey.isEmpty() && m_art->cached(m_artKey, url) && !url.isEmpty())
                image = url;
        }
    }
    a.largeImage = image;
    a.largeText = imageText.isEmpty() ? a.details : imageText;

    a.playing = m_player->playbackState() == QMediaPlayer::PlayingState;
    a.positionMs = positionOverrideMs >= 0 ? positionOverrideMs : m_player->position();
    a.durationMs = m_player->duration();
    m_discord->setActivity(a);
}

void MainWindow::applyDiscordSettings(const mv::DiscordSettings &s)
{
    m_discordCfg = s;
    m_discord->setClientId(s.clientId);
    m_art->setTmdbKey(s.tmdbKey);
}

void MainWindow::openDiscordSettings()
{
    DiscordSettingsDialog dlg(m_discordCfg, this);
    if (dlg.exec() != QDialog::Accepted)
        return;
    const mv::DiscordSettings s = dlg.result();
    s.save();
    applyDiscordSettings(s);
    updatePresence();
}

void MainWindow::onDiscordToggled(bool on)
{
    QSettings().setValue(kDiscordEnabledKey, on);
    if (on && m_discordCfg.clientId.isEmpty()) {
        openDiscordSettings();
        // The dialog may have been cancelled; keep the checkbox honest.
        if (m_discordCfg.clientId.isEmpty()) {
            QSignalBlocker block(m_discordToggle);
            m_discordToggle->setChecked(false);
            QSettings().setValue(kDiscordEnabledKey, false);
            return;
        }
    }
    m_discord->setEnabled(on);
    updatePresence();
}

// ---------------------------------------------------------------------------
// Themes
// ---------------------------------------------------------------------------

QString MainWindow::userThemesDir() const
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/themes");
}

QStringList MainWindow::themeRoots() const
{
    const QString app = QCoreApplication::applicationDirPath();
    // Bundled first so that a user theme with the same id overrides it.
    return {app + QStringLiteral("/themes"),
            QDir::cleanPath(app + QStringLiteral("/../Resources/themes")),
            userThemesDir()};
}

void MainWindow::reloadThemes()
{
    QDir().mkpath(userThemesDir());
    m_themes = mv::scanThemes(themeRoots());
    rebuildThemeMenu();
}

void MainWindow::rebuildThemeMenu()
{
    m_themeMenu->clear();

    auto addChoice = [&](const QString &text, const QString &id, const QString &tip) {
        QAction *a = m_themeMenu->addAction(text);
        a->setCheckable(true);
        a->setData(id);
        a->setToolTip(tip);
        a->setChecked(id == m_themeId);
        m_themeGroup->addAction(a);
        connect(a, &QAction::triggered, this, [this, id] { applyTheme(id); });
    };

    for (const mv::ThemeInfo &t : std::as_const(m_themes)) {
        QString label = t.name;
        if (!t.author.isEmpty())
            label += tr("  (%1)").arg(t.author);
        addChoice(label, t.id, t.description);
    }

    m_themeMenu->addSeparator();
    addChoice(tr("Legacy (media browser on the left)"), QString::fromLatin1(kLegacyId),
              tr("The original layout: library list beside the player"));

    m_themeMenu->addSeparator();
    m_themeMenu->addAction(tr("Open Themes Folder"), this, [this] {
        QDir().mkpath(userThemesDir());
        QDesktopServices::openUrl(QUrl::fromLocalFile(userThemesDir()));
    });
    m_themeMenu->addAction(tr("Reload Themes"), this, [this] {
        reloadThemes();
        applyTheme(m_themeId);
    });
}

void MainWindow::ensureThemePanel()
{
    if (m_themePanel)
        return;
    m_themePanel = new ThemePanel;   // placed by setLayoutMode()

    ThemeBridge *b = m_themePanel->bridge();
    connect(b, &ThemeBridge::togglePlayRequested, this, &MainWindow::togglePlay);
    connect(b, &ThemeBridge::playRequested, this, [this] {
        if (m_player->playbackState() != QMediaPlayer::PlayingState)
            togglePlay();
    });
    connect(b, &ThemeBridge::pauseRequested, this, [this] {
        if (m_player->playbackState() == QMediaPlayer::PlayingState)
            m_player->pause();
    });
    connect(b, &ThemeBridge::stopRequested, m_player, &QMediaPlayer::stop);
    connect(b, &ThemeBridge::nextRequested, this, &MainWindow::playNext);
    connect(b, &ThemeBridge::previousRequested, this, &MainWindow::playPrevious);
    connect(b, &ThemeBridge::seekRequested, this, [this](qint64 ms) { seekTo(ms); });
    connect(b, &ThemeBridge::volumeRequested, this, [this](int v) { m_volume->setValue(v); });
    connect(b, &ThemeBridge::playTrackRequested, this, &MainWindow::playPath);
    connect(b, &ThemeBridge::openArtistRequested, this,
            [this](const QString &artist) { showMusicView(QStringLiteral("artist"), artist, QString()); });
    connect(b, &ThemeBridge::openAlbumRequested, this, [this](const QString &artist, const QString &album) {
        showMusicView(QStringLiteral("album"), artist, album);
    });
    connect(b, &ThemeBridge::showNowPlayingRequested, this,
            [this] { showMusicView(QStringLiteral("nowplaying"), QString(), QString()); });
    connect(b, &ThemeBridge::toggleFullscreenRequested, this, &MainWindow::toggleFullscreen);
    connect(b, &ThemeBridge::openFolderRequested, this, &MainWindow::chooseFolder);
    connect(b, &ThemeBridge::rescanRequested, this, &MainWindow::rescan);
    connect(b, &ThemeBridge::showVideoRequested, this, &MainWindow::showVideoPage);
    connect(b, &ThemeBridge::showLibraryRequested, this, &MainWindow::showLibraryPage);
    connect(b, &ThemeBridge::videoRectRequested, this, [this](int x, int y, int w, int h) {
        m_videoRect = QRect(x, y, w, h);
        positionVideo();
    });

    b->setLibrary(libraryJson());
}

void MainWindow::applyTheme(const QString &requested)
{
    // Classic is the default; an unknown or removed theme falls back to it,
    // and to the legacy layout if even that is missing.
    QString id = requested.trimmed().isEmpty() ? QStringLiteral("classic") : requested.trimmed();

    qApp->setStyleSheet(QString());
    m_hasTheme = false;
    m_activeTheme = mv::ThemeInfo();
    m_legacy = false;

    auto findTheme = [this](const QString &wanted) -> const mv::ThemeInfo * {
        for (const mv::ThemeInfo &t : std::as_const(m_themes))
            if (t.id == wanted)
                return &t;
        return nullptr;
    };

    if (id == QLatin1String(kLegacyId)) {
        m_legacy = true;
    } else if (const mv::ThemeInfo *t = findTheme(id)) {
        m_activeTheme = *t;
        m_hasTheme = true;
    } else if (const mv::ThemeInfo *c = findTheme(QStringLiteral("classic"))) {
        id = QStringLiteral("classic");
        m_activeTheme = *c;
        m_hasTheme = true;
    } else {
        id = QString::fromLatin1(kLegacyId);
        m_legacy = true;
    }

    LayoutMode mode = LayoutMode::Legacy;
    if (m_hasTheme) {
        for (const QString &font : std::as_const(m_activeTheme.fontPaths)) {
            if (!m_registeredFonts.contains(font)) {
                QFontDatabase::addApplicationFont(font);
                m_registeredFonts.insert(font);
            }
        }
        qApp->setStyleSheet(mv::themeStyleSheet(m_activeTheme));

        if (m_activeTheme.hasEntry()) {
            if (ThemePanel::isAvailable()) {
                ensureThemePanel();
                m_themePanel->loadTheme(m_activeTheme);
                if (m_activeTheme.isFullUi())
                    mode = LayoutMode::Full;
            } else {
                statusBar()->showMessage(
                    tr("This build has no web themes, so the legacy layout is used."), 8000);
            }
        }
    }

    m_themeId = id;
    QSettings().setValue(kThemeSetting, id);
    for (QAction *a : m_themeGroup->actions())
        a->setChecked(a->data().toString() == id);

    setLayoutMode(mode);
    pushThemeState();
}

void MainWindow::setLayoutMode(LayoutMode mode)
{
    m_layoutMode = mode;
    const bool full = mode == LayoutMode::Full;
    if (!isFullScreen())
        statusBar()->setVisible(!full);

    if (full) {
        // The video widget leaves the legacy player and floats over the page.
        m_stage->removeWidget(m_video);
        m_video->setParent(m_fullPage);
        m_video->hide();
        auto *fl = m_fullPage->layout();
        if (fl->indexOf(m_themePanel) < 0)
            fl->addWidget(m_themePanel);
        m_right->setParent(nullptr);   // out of the splitter
        m_right->hide();
        m_shell->setCurrentWidget(m_fullPage);
        m_videoShown = m_current && categoryOf(m_current) != 2
                       && m_player->playbackState() != QMediaPlayer::StoppedState;
        positionVideo();
    } else {
        m_videoShown = false;
        m_video->hide();
        if (m_stage->indexOf(m_video) < 0)
            m_stage->insertWidget(0, m_video);
        if (m_splitter->indexOf(m_right) < 0)
            m_splitter->addWidget(m_right);
        m_splitter->setSizes({340, 860});
        m_right->show();
        m_video->show();
        if (m_themePanel && m_hasTheme && m_activeTheme.isPanel() && m_stage->indexOf(m_themePanel) < 0)
            m_stage->addWidget(m_themePanel);   // legacy layout with a music panel
        m_shell->setCurrentWidget(m_splitter);
    }
    updateStage();
}

void MainWindow::positionVideo()
{
    const bool show = m_layoutMode == LayoutMode::Full && m_videoShown
                      && m_videoRect.width() > 1 && m_videoRect.height() > 1;
    if (!show) {
        if (m_layoutMode == LayoutMode::Full)
            m_video->hide();
        return;
    }
    if (m_video->geometry() != m_videoRect)
        m_video->setGeometry(m_videoRect);
    m_video->show();
    m_video->raise();
}

void MainWindow::showVideoPage()
{
    if (m_layoutMode != LayoutMode::Full)
        return;
    m_videoShown = true;
    positionVideo();
    pushThemeState();
}

void MainWindow::showLibraryPage()
{
    if (m_layoutMode != LayoutMode::Full)
        return;
    m_videoShown = false;
    positionVideo();
    pushThemeState();
}

void MainWindow::updateStage()
{
    // Only the legacy layout swaps video for a theme's music panel.
    const bool panelInStage = m_layoutMode == LayoutMode::Legacy && m_themePanel && m_hasTheme
                              && m_activeTheme.isPanel() && m_stage->indexOf(m_themePanel) >= 0;
    const bool showPanel = panelInStage && m_musicStage;
    QWidget *target = showPanel ? static_cast<QWidget *>(m_themePanel) : static_cast<QWidget *>(m_video);
    if (m_stage->indexOf(target) >= 0)
        m_stage->setCurrentWidget(target);
}

QString MainWindow::libraryJson() const
{
    return QString::fromUtf8(QJsonDocument(mv::libraryToJson(m_library)).toJson(QJsonDocument::Compact));
}

void MainWindow::pushThemeState()
{
    if (!m_themePanel)
        return;

    mv::PlaybackSnapshot s;
    s.root = m_library.root;
    if (m_current) {
        QStringList crumbs;
        for (QTreeWidgetItem *p = m_current; p && p->parent(); p = p->parent())
            crumbs.prepend(p->text(0));
        const int category = categoryOf(m_current);
        s.path = m_current->data(0, kPathRole).toString();

        if (category == 2 && crumbs.size() >= 3) {
            s.kind = QStringLiteral("music");
            s.artist = crumbs.at(0);
            s.album = crumbs.at(1);
            s.title = crumbs.at(2);
            s.cover = mv::fileUrl(m_current->data(0, kCoverRole).toString());
        } else {
            s.kind = QStringLiteral("video");
            const QString clean = m_current->data(0, kTitleRole).toString();
            s.title = !clean.isEmpty() ? clean : (crumbs.isEmpty() ? m_current->text(0) : crumbs.last());
            if (category == 1) {
                s.show = crumbs.value(0);
                s.season = m_current->data(0, kSeasonRole).toInt();
                s.episode = m_current->data(0, kEpisodeRole).toInt();
            }
        }
    }
    s.playing = m_player->playbackState() == QMediaPlayer::PlayingState;
    s.positionMs = m_player->position();
    s.durationMs = m_player->duration();
    s.volume = m_volume->value();
    s.page = (m_layoutMode == LayoutMode::Full && m_videoShown)
                 ? QStringLiteral("video") : QStringLiteral("ui");
    s.fullscreen = isFullScreen();
    s.viewType = m_viewType;
    s.viewArtist = m_viewArtist;
    s.viewAlbum = m_viewAlbum;

    m_themePanel->bridge()->setState(
        QString::fromUtf8(QJsonDocument(mv::snapshotToJson(s)).toJson(QJsonDocument::Compact)));
}

void MainWindow::togglePlay()
{
    switch (m_player->playbackState()) {
    case QMediaPlayer::PlayingState:
        m_player->pause();
        break;
    case QMediaPlayer::PausedState:
        m_player->play();
        break;
    case QMediaPlayer::StoppedState:
        if (m_current) {
            m_player->play();
        } else if (auto *sel = m_tree->currentItem(); sel && sel->data(0, kPathRole).isValid()) {
            playItem(sel);
        }
        break;
    }
}

void MainWindow::playNext()
{
    const auto leaves = leafItems();
    const int i = leaves.indexOf(m_current);
    // Skip items hidden by the filter.
    for (int j = i + 1; i >= 0 && j < leaves.size(); ++j) {
        if (!leaves[j]->isHidden()) {
            m_tree->setCurrentItem(leaves[j]);
            playItem(leaves[j]);
            return;
        }
    }
}

void MainWindow::playPrevious()
{
    // Restart the current track if we're more than 3 seconds in.
    if (m_player->position() > 3000) {
        seekTo(0);
        return;
    }
    const auto leaves = leafItems();
    const int i = leaves.indexOf(m_current);
    for (int j = i - 1; i > 0 && j >= 0; --j) {
        if (!leaves[j]->isHidden()) {
            m_tree->setCurrentItem(leaves[j]);
            playItem(leaves[j]);
            return;
        }
    }
}

void MainWindow::toggleFullscreen()
{
    setFullscreenMode(!isFullScreen());
}

void MainWindow::setFullscreenMode(bool on)
{
    if (on == isFullScreen())
        return;

    if (on) {
        m_wasMaximized = isMaximized();
        m_left->hide();
        m_nowPlaying->hide();
        menuBar()->hide();
        statusBar()->hide();
        showFullScreen();
        m_lastCursor = QCursor::pos();
        m_pollTimer->start();
        showControls();   // visible first, then auto-hides after a few seconds
    } else {
        m_pollTimer->stop();
        m_hideTimer->stop();
        m_video->unsetCursor();
        m_stage->unsetCursor();
        m_left->show();
        m_nowPlaying->show();
        m_controlsBar->show();
        menuBar()->show();
        statusBar()->setVisible(m_layoutMode != LayoutMode::Full);
        if (m_wasMaximized)
            showMaximized();
        else
            showNormal();
    }
    pushThemeState();
}

void MainWindow::changeEvent(QEvent *event)
{
    QMainWindow::changeEvent(event);
    if (event->type() == QEvent::WindowStateChange)
        pushThemeState();
}

void MainWindow::showControls()
{
    if (m_layoutMode == LayoutMode::Full && m_themePanel)
        m_themePanel->bridge()->notifyActivity();
    m_controlsBar->show();
    m_video->unsetCursor();
    m_stage->unsetCursor();
    if (isFullScreen())
        m_hideTimer->start();
}

bool MainWindow::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == m_video) {
        switch (event->type()) {
        case QEvent::MouseMove:
            if (isFullScreen())
                showControls();
            break;
        case QEvent::MouseButtonDblClick:
            toggleFullscreen();
            return true;
        default:
            break;
        }
    }
    return QMainWindow::eventFilter(obj, event);
}

bool MainWindow::filterItem(QTreeWidgetItem *item, const QString &needle)
{
    const bool selfMatch = needle.isEmpty() || item->text(0).contains(needle, Qt::CaseInsensitive);
    bool childVisible = false;
    for (int i = 0; i < item->childCount(); ++i) {
        // If the parent itself matches, keep its whole subtree visible.
        QTreeWidgetItem *c = item->child(i);
        const bool v = selfMatch ? (filterItem(c, QString()), true) : filterItem(c, needle);
        childVisible = childVisible || v;
    }
    const bool visible = selfMatch || childVisible;
    item->setHidden(!visible);
    return visible;
}

void MainWindow::applyFilter(const QString &text)
{
    const QString needle = text.trimmed();
    for (int i = 0; i < m_tree->topLevelItemCount(); ++i) {
        QTreeWidgetItem *top = m_tree->topLevelItem(i);
        // Category rows always stay visible; only filter below them.
        for (int j = 0; j < top->childCount(); ++j)
            filterItem(top->child(j), needle);
        top->setHidden(false);
        if (!needle.isEmpty())
            top->setExpanded(true);
    }
}

QString MainWindow::formatTime(qint64 ms)
{
    const qint64 total = ms / 1000;
    const qint64 h = total / 3600, m = (total / 60) % 60, s = total % 60;
    if (h > 0)
        return QStringLiteral("%1:%2:%3").arg(h).arg(m, 2, 10, QLatin1Char('0')).arg(s, 2, 10, QLatin1Char('0'));
    return QStringLiteral("%1:%2").arg(m).arg(s, 2, 10, QLatin1Char('0'));
}
