#pragma once

#include <QWidget>
#include <QAbstractNativeEventFilter>
#include <QSet>
#include <QHash>

#include "ukeysequence.h"
#include "uexception.h"
#include "nativeeventfilter.h"

#if defined(Q_OS_LINUX)
#include "xcb/xcb.h"
#include "xcb/xcb_keysyms.h"
#endif

struct UHotkeyData {
#if defined(Q_OS_WIN)

#else defined(Q_OS_LINUX)
    xcb_keycode_t       keyCode;
    int                 mods;
#endif
};

class UGlobalHotkeys : public QWidget
{
    Q_OBJECT

    friend class NativeEventFilter;
public:
    explicit UGlobalHotkeys(NativeEventFilter* eventFilter, QWidget *parent = 0);
    void RegisterHotkey(const QString& keySeq, size_t id = 1);
    void RegisterHotkey(const UKeySequence& keySeq, size_t id = 1);
    void UnregisterHotkey(size_t id = 1);
    ~UGlobalHotkeys();
protected:
    #if defined(Q_OS_WIN)
    bool winEvent (MSG * message, long * result);
    #elif defined(Q_OS_LINUX)
    bool linuxEvent(void *message);
    #endif
    bool nativeEvent(const QByteArray &eventType, void *message, long *result);
signals:
    void Activated(size_t id);
private:
    QHash<size_t, UHotkeyData> Registered;

#ifdef Q_OS_LINUX
    xcb_connection_t    *c;
    xcb_window_t        wId;
    xcb_key_symbols_t   *keySyms;
#endif
    NativeEventFilter *evFilter;
};

