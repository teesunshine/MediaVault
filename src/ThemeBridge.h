#pragma once

#include <QObject>
#include <QString>

// The object a web theme talks to (exposed to the page as window.MediaVault by
// the script ThemePanel injects). Slots are called from JavaScript and just
// turn into signals that the main window handles; the page receives state
// through stateChanged / libraryChanged.
class ThemeBridge : public QObject {
    Q_OBJECT
public:
    explicit ThemeBridge(QObject *parent = nullptr);

    // Called by the main window.
    void setState(const QString &json);
    void setLibrary(const QString &json);

public slots:   // --- called from JavaScript ---------------------------------
    QString getState() const { return m_stateJson; }
    QString getLibrary() const { return m_libraryJson; }

    void play() { emit playRequested(); }
    void pause() { emit pauseRequested(); }
    void togglePlay() { emit togglePlayRequested(); }
    void next() { emit nextRequested(); }
    void previous() { emit previousRequested(); }
    void seek(qint64 ms) { emit seekRequested(ms); }
    void setVolume(int percent) { emit volumeRequested(percent); }
    void playTrack(const QString &path) { emit playTrackRequested(path); }
    void playMedia(const QString &path) { emit playTrackRequested(path); }   // any song, movie or episode
    void stop() { emit stopRequested(); }
    void openFolder() { emit openFolderRequested(); }
    void rescan() { emit rescanRequested(); }
    void showVideo() { emit showVideoRequested(); }
    void showLibrary() { emit showLibraryRequested(); }
    void openArtist(const QString &artist) { emit openArtistRequested(artist); }
    void openAlbum(const QString &artist, const QString &album) { emit openAlbumRequested(artist, album); }
    void showNowPlaying() { emit showNowPlayingRequested(); }
    void toggleFullscreen() { emit toggleFullscreenRequested(); }

signals:
    // To the page.
    void stateChanged(const QString &json);
    void libraryChanged();

    // To the main window.
    void playRequested();
    void pauseRequested();
    void togglePlayRequested();
    void nextRequested();
    void previousRequested();
    void seekRequested(qint64 ms);
    void volumeRequested(int percent);
    void playTrackRequested(const QString &path);
    void stopRequested();
    void openFolderRequested();
    void rescanRequested();
    void showVideoRequested();
    void showLibraryRequested();
    void openArtistRequested(const QString &artist);
    void openAlbumRequested(const QString &artist, const QString &album);
    void showNowPlayingRequested();
    void toggleFullscreenRequested();

private:
    QString m_stateJson = QStringLiteral("{}");
    QString m_libraryJson = QStringLiteral("{\"root\":\"\",\"movies\":[],\"shows\":[],\"artists\":[]}");
};
