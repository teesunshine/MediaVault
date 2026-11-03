#include "ThemeBridge.h"

ThemeBridge::ThemeBridge(QObject *parent)
    : QObject(parent)
{
}

void ThemeBridge::setState(const QString &json)
{
    if (json == m_stateJson)
        return;
    m_stateJson = json;
    emit stateChanged(json);
}

void ThemeBridge::setLibrary(const QString &json)
{
    m_libraryJson = json;
    emit libraryChanged();
}
