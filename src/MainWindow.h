#pragma once

#include <QElapsedTimer>
#include <QMainWindow>
#include <QSet>
#include <QVector>

#include "DiscordSettings.h"
#include "MediaLibrary.h"
#include "ThemeData.h"

class QAction;
class QActionGroup;
class QMenu;
class QStackedWidget;
class ThemePanel;
namespace mv { class ArtworkResolver; class DiscordPresence; }

class QAudioOutput;
class QLabel;
class QLineEdit;
class QMediaPlayer;
class QPushButton;
class QSlider;
class QTimer;
class QEvent;
class QTreeWidget;
class QTreeWidgetItem;
class QVideoWidget;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);

    // Scans `root` (which should contain Music, Movies and TV Shows folders).
    void openLibrary(const QString &root);
    // Re-opens the folder used last time, if any.
    void openSavedLibrary();

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private slots:
    void chooseFolder();
    void rescan();
    void onItemActivated(QTreeWidgetItem *item, int column);
    void togglePlay();
    void playNext();
    void playPrevious();
    void toggleFullscreen();
    void applyFilter(const QString &text);
    void onDiscordToggled(bool on);
    void openDiscordSettings();
    void onItemClicked(QTreeWidgetItem *item, int column);

private:
    // Tells Discord what is playing. Pass a position when the player hasn't
    // caught up yet (e.g. right after a seek); -1 reads it from the player.
    void updatePresence(qint64 positionOverrideMs = -1);
    void seekTo(qint64 ms);
    void applyDiscordSettings(const mv::DiscordSettings &s);

    // --- Themes ---
    QStringList themeRoots() const;
    QString userThemesDir() const;
    void reloadThemes();
    void rebuildThemeMenu();
    void applyTheme(const QString &id);
    void ensureThemePanel();
    void updateStage();                 // video, or the theme's web page for music
    void pushThemeState();              // sends what's playing to the web theme
    void showMusicView(const QString &type, const QString &artist, const QString &album);
    int categoryOf(QTreeWidgetItem *item) const;   // 0 movies, 1 TV, 2 music, -1 none
    void playPath(const QString &path);
    QString libraryJson() const;
    void setFullscreenMode(bool on);
    void showControls();
    void populate();
    void playItem(QTreeWidgetItem *item);
    QList<QTreeWidgetItem *> leafItems() const;
    bool filterItem(QTreeWidgetItem *item, const QString &needle);
    static QString formatTime(qint64 ms);

    mv::Library m_library;
    QString m_root;

    QTreeWidget *m_tree = nullptr;
    QLineEdit *m_search = nullptr;
    QMediaPlayer *m_player = nullptr;
    QAudioOutput *m_audio = nullptr;
    QVideoWidget *m_video = nullptr;
    QLabel *m_nowPlaying = nullptr;
    QLabel *m_timeLabel = nullptr;
    QSlider *m_seek = nullptr;
    QSlider *m_volume = nullptr;
    QPushButton *m_playBtn = nullptr;
    QTreeWidgetItem *m_current = nullptr;
    bool m_seeking = false;

    QWidget *m_left = nullptr;          // library pane, hidden in fullscreen
    QWidget *m_controlsBar = nullptr;   // transport controls, auto-hide in fullscreen
    QTimer *m_hideTimer = nullptr;
    QTimer *m_pollTimer = nullptr;   // watches the cursor while fullscreen
    QPoint m_lastCursor;
    mv::DiscordPresence *m_discord = nullptr;
    mv::ArtworkResolver *m_art = nullptr;
    mv::DiscordSettings m_discordCfg;
    QString m_artKey;               // lookup key of the picture for the current item
    QAction *m_discordToggle = nullptr;

    QStackedWidget *m_stage = nullptr;      // [video | theme panel]
    ThemePanel *m_themePanel = nullptr;     // created when the first web theme is applied
    QMenu *m_themeMenu = nullptr;
    QActionGroup *m_themeGroup = nullptr;
    QVector<mv::ThemeInfo> m_themes;
    mv::ThemeInfo m_activeTheme;
    bool m_hasTheme = false;
    QSet<QString> m_registeredFonts;
    bool m_musicStage = false;              // true while music is what the stage should show
    QString m_viewType = QStringLiteral("nowplaying");
    QString m_viewArtist, m_viewAlbum;
    QElapsedTimer m_themePushTimer;
    bool m_wasMaximized = false;
};
