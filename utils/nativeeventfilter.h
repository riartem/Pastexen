#ifndef NATIVEEVENTFILTER_H
#define NATIVEEVENTFILTER_H

#include <QAbstractNativeEventFilter>
#include "ukeysequence.h"
#include <QHash>

#if defined(Q_OS_LINUX)
#include "xcb/xcb.h"
#endif

struct UHotkeyData {
#if defined(Q_OS_WIN)

#elif defined(Q_OS_LINUX)
    xcb_keycode_t       keyCode;
    int                 mods;
#endif
};

struct UKeyData {
    int key;
    int mods;
};

class UGlobalHotkeys;

class NativeEventFilter : public QAbstractNativeEventFilter
{
    friend class UGlobalHotkeys;
public:
    NativeEventFilter(UGlobalHotkeys *globalHotkeys);
    ~NativeEventFilter();
    bool nativeEventFilter(const QByteArray &eventType, void *message, long *result) override;

    void registerHotkey(const UKeySequence& keySeq, size_t id);
    void unregisterHotkey(size_t id);

private:
#if defined(Q_OS_WIN)
    void winEvent (MSG* message, long* result);
    void regWinHotkey(const UKeySequence& keySeq, size_t id);
    void unregWinHotkey(size_t id);
#elif defined(Q_OS_LINUX)
    void linuxEvent(xcb_generic_event_t *message);
    void regLinuxHotkey(const UKeySequence& keySeq, size_t id);
    void unregLinuxHotkey(size_t id);
    UKeyData qtKeyToLinux(const UKeySequence& keySeq);
#endif

    UGlobalHotkeys *hk;

#ifdef Q_OS_LINUX
    xcb_connection_t    *c;
    xcb_window_t        wId;
#endif

    QHash<size_t, UHotkeyData> Registered;
};

#endif // NATIVEEVENTFILTER_H
