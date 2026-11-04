#include "ThemePanel.h"

#include "ThemeBridge.h"

#include <QLabel>
#include <QVBoxLayout>

#ifdef MV_HAVE_WEBENGINE
#include <QFile>
#include <QUrl>
#include <QWebChannel>
#include <QWebEnginePage>
#include <QWebEngineScript>
#include <QWebEngineScriptCollection>
#include <QWebEngineSettings>
#include <QWebEngineView>

namespace {

// Runs in the page before the theme's own scripts. Turns the QWebChannel bridge
// into a friendly window.MediaVault object. Documented in docs/THEMES.md.
const char kBootstrap[] = R"JS(
;(function () {
  if (window.MediaVault) return;
  var handlers = {};
  var mv = window.MediaVault = {
    ready: false,
    state: null,
    on: function (name, fn) {
      (handlers[name] = handlers[name] || []).push(fn);
      if (name === 'state' && mv.state) { try { fn(mv.state); } catch (e) { console.error(e); } }
      if (name === 'ready' && mv.ready) { try { fn(mv.state); } catch (e) { console.error(e); } }
      return mv;
    },
    _emit: function (name, data) {
      (handlers[name] || []).slice().forEach(function (f) { try { f(data); } catch (e) { console.error(e); } });
      window.dispatchEvent(new CustomEvent('mediavault:' + name, { detail: data }));
    }
  };

  function connect() {
    new QWebChannel(qt.webChannelTransport, function (channel) {
      var b = channel.objects.bridge;
      ['play', 'pause', 'togglePlay', 'stop', 'next', 'previous', 'showNowPlaying', 'toggleFullscreen',
       'openFolder', 'rescan', 'showVideo', 'showLibrary'].forEach(function (n) {
        mv[n] = function () { b[n](); };
      });
      mv.seek = function (ms) { b.seek(Math.round(ms)); };
      mv.setVolume = function (p) { b.setVolume(Math.round(Math.max(0, Math.min(100, p)))); };
      mv.playTrack = function (path) { b.playMedia(path); };
      mv.playMedia = function (path) { b.playMedia(path); };
      mv.openArtist = function (a) { b.openArtist(a); };
      mv.openAlbum = function (a, al) { b.openAlbum(a, al); };
      mv.getLibrary = function () {
        return new Promise(function (resolve) {
          b.getLibrary(function (s) { resolve(JSON.parse(s)); });
        });
      };
      b.stateChanged.connect(function (s) { mv.state = JSON.parse(s); mv._emit('state', mv.state); });
      b.libraryChanged.connect(function () { mv._emit('library'); });
      b.getState(function (s) {
        mv.state = JSON.parse(s);
        mv.ready = true;
        mv._emit('ready', mv.state);
        mv._emit('state', mv.state);
      });
    });
  }

  var tries = 0;
  (function wait() {
    if (window.qt && qt.webChannelTransport && typeof QWebChannel !== 'undefined') connect();
    else if (tries++ < 200) setTimeout(wait, 25);
  })();
})();
)JS";

} // namespace
#endif

bool ThemePanel::isAvailable()
{
#ifdef MV_HAVE_WEBENGINE
    return true;
#else
    return false;
#endif
}

ThemePanel::ThemePanel(QWidget *parent)
    : QWidget(parent)
{
    m_bridge = new ThemeBridge(this);
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

#ifdef MV_HAVE_WEBENGINE
    m_view = new QWebEngineView(this);
    m_view->setContextMenuPolicy(Qt::NoContextMenu);
    m_view->setStyleSheet(QStringLiteral("background:black;"));
    m_view->page()->setBackgroundColor(Qt::black);
    m_view->settings()->setAttribute(QWebEngineSettings::LocalContentCanAccessFileUrls, true);
    m_view->settings()->setAttribute(QWebEngineSettings::LocalContentCanAccessRemoteUrls, false);
    m_view->settings()->setAttribute(QWebEngineSettings::PlaybackRequiresUserGesture, true);

    m_channel = new QWebChannel(this);
    m_channel->registerObject(QStringLiteral("bridge"), m_bridge);
    m_view->page()->setWebChannel(m_channel);

    QString source;
    QFile js(QStringLiteral(":/qtwebchannel/qwebchannel.js"));
    if (js.open(QIODevice::ReadOnly))
        source = QString::fromUtf8(js.readAll());
    source += QString::fromUtf8(kBootstrap);

    QWebEngineScript script;
    script.setName(QStringLiteral("mediavault-bootstrap"));
    script.setSourceCode(source);
    script.setInjectionPoint(QWebEngineScript::DocumentCreation);
    script.setWorldId(QWebEngineScript::MainWorld);
    script.setRunsOnSubFrames(false);
    m_view->page()->scripts().insert(script);

    layout->addWidget(m_view);
#else
    m_note = new QLabel(tr("This build of MediaVault doesn't include web themes (Qt WebEngine)."));
    m_note->setAlignment(Qt::AlignCenter);
    m_note->setWordWrap(true);
    layout->addWidget(m_note);
#endif
}

bool ThemePanel::loadTheme(const mv::ThemeInfo &theme)
{
#ifdef MV_HAVE_WEBENGINE
    if (!theme.hasEntry())
        return false;
    m_view->load(QUrl::fromLocalFile(theme.entryPath));
    return true;
#else
    Q_UNUSED(theme);
    return false;
#endif
}
