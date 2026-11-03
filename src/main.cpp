#include <QApplication>
#include <QCoreApplication>

#include "MainWindow.h"

int main(int argc, char *argv[])
{
#ifdef MV_HAVE_WEBENGINE
    // Recommended by Qt when WebEngine and other OpenGL users share a process.
    QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
#endif
    QApplication app(argc, argv);
    QApplication::setApplicationName("MediaVault");
    QApplication::setOrganizationName("MediaVault");
    QApplication::setApplicationVersion("0.1.0");

    MainWindow w;
    w.show();

    // Optional: MediaVault /path/to/media
    const QStringList args = QApplication::arguments();
    if (args.size() > 1)
        w.openLibrary(args.at(1));
    else
        w.openSavedLibrary();

    return app.exec();
}
