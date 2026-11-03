#pragma once

#include <QWidget>

#include "ThemeData.h"

class ThemeBridge;
class QLabel;
#ifdef MV_HAVE_WEBENGINE
class QWebChannel;
class QWebEngineView;
#endif

// Shows a theme's web page in place of the video while music is open. Without
// Qt WebEngine (built with -DMEDIAVAULT_WEBENGINE=OFF) it only shows a note.
class ThemePanel : public QWidget {
    Q_OBJECT
public:
    explicit ThemePanel(QWidget *parent = nullptr);

    ThemeBridge *bridge() const { return m_bridge; }
    static bool isAvailable();

    // Loads the theme's entry page. Returns false if it has none or web
    // themes aren't available in this build.
    bool loadTheme(const mv::ThemeInfo &theme);

private:
    ThemeBridge *m_bridge = nullptr;
    QLabel *m_note = nullptr;
#ifdef MV_HAVE_WEBENGINE
    QWebEngineView *m_view = nullptr;
    QWebChannel *m_channel = nullptr;
#endif
};
