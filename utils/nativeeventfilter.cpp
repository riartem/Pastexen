#include "nativeeventfilter.h"
#include "uglobalhotkeys.h"
#include <QApplication>
#include <QWindow>
#include <qpa/qplatformnativeinterface.h>
#include <QDebug>
#if defined(Q_OS_WIN)
#include <windows.h>
#include "winhotkeymap.h"
#elif defined(Q_OS_LINUX)
#include "X11/keysym.h"
#include "xcb/xcb_keysyms.h"
#endif

NativeEventFilter::NativeEventFilter(UGlobalHotkeys *globalHotkeys) :
    hk(globalHotkeys)
{
#ifdef Q_OS_LINUX
//    QWindow wndw;
    void* v = qGuiApp->platformNativeInterface()->nativeResourceForWindow("connection", new QWindow);
    c = (xcb_connection_t*)v;
    wId = xcb_setup_roots_iterator(xcb_get_setup(c)).data->root;
#endif
}

NativeEventFilter::~NativeEventFilter()
{
}

bool NativeEventFilter::nativeEventFilter(const QByteArray &eventType, void *message, long *result)
{
#if defined(Q_OS_WIN)
    winEvent(static_cast<MSG*>(message), result);
#elif defined(Q_OS_LINUX)
    linuxEvent(static_cast<xcb_generic_event_t*>(message));
#endif

    return false;
}

void NativeEventFilter::registerHotkey(const UKeySequence &keySeq, size_t id)
{
#if defined(Q_OS_WIN)
    regWinHotkey(jeySeq, id);
#elif defined(Q_OS_LINUX)
    regLinuxHotkey(keySeq, id);
#endif
}

void NativeEventFilter::unregisterHotkey(size_t id)
{
#if defined(Q_OS_WIN)
    unregWinHotkey(id);
#elif defined(Q_OS_LINUX)
    unregLinuxHotkey(id);
#endif
}

#if defined(Q_OS_WIN)

void NativeEventFilter::winEvent(MSG *message, long *result)
{
    Q_UNUSED(result);
    if (message->message == WM_HOTKEY) {
        size_t id = message->wParam;
        Q_ASSERT(Registered.find(id) != Registered.end() && "Unregistered hotkey");
        emit Activated(id);
    }
}

void NativeEventFilter::regWinHotkey()
{
}

void NativeEventFilter::unregWinHotkey()
{
}

#elif defined(Q_OS_LINUX)

void NativeEventFilter::linuxEvent(xcb_generic_event_t *message)
{
    qDebug() << Q_FUNC_INFO;

    if ( (message->response_type & ~0x80) == XCB_KEY_PRESS ) {
        fprintf(stderr, "KeyEvent\n");
    }
}

void NativeEventFilter::regLinuxHotkey(const UKeySequence &keySeq, size_t id)
{
    static xcb_key_symbols_t* keySyms = xcb_key_symbols_alloc(c);

    UHotkeyData data;

    xcb_keycode_t *keyC = xcb_key_symbols_get_keycode(keySyms, XK_F10);
    int mods = XCB_MOD_MASK_CONTROL | XCB_MOD_MASK_SHIFT;

    data.keyCode = *keyC;
    data.mods = mods;

    xcb_grab_key(c, 1, wId, data.mods, data.keyCode, XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC);

    Registered.insert(id, data);
}

void NativeEventFilter::unregLinuxHotkey(size_t id)
{
    UHotkeyData data = Registered.take(id);
    xcb_ungrab_key(c, data.keyCode, wId, data.mods);
}

#endif
