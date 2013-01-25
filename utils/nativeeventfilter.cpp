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
    UKeyData    keyData = qtKeyToLinux(keySeq);

    xcb_keycode_t *keyC = xcb_key_symbols_get_keycode(keySyms, keyData.key);

    data.keyCode = *keyC;
    data.mods = keyData.mods;

    xcb_grab_key(c, 1, wId, data.mods, data.keyCode, XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC);

    Registered.insert(id, data);
}

void NativeEventFilter::unregLinuxHotkey(size_t id)
{
    UHotkeyData data = Registered.take(id);
    xcb_ungrab_key(c, data.keyCode, wId, data.mods);
}

UKeyData NativeEventFilter::qtKeyToLinux(const UKeySequence &keySeq)
{
    UKeyData data = {0, 0};

    auto key = keySeq.GetSimpleKeys();
    if (key.size() > 0)
        data.key = key[0];
    else
        throw UException("Invalid hotkey");

    // Key conversion
    // Qt's F keys need conversion
    if (data.key >= Qt::Key_F1 && data.key <= Qt::Key_F35) {
        const size_t DIFF = Qt::Key_F1 - XK_F1;
        data.key -= DIFF;
    } else if (data.key >= Qt::Key_Space && data.key <= Qt::Key_QuoteLeft) {
        // conversion is not necessary, if the value in the range Qt::Key_Space - Qt::Key_QuoteLeft
    } else {
        throw UException("Invalid hotkey: key conversion is not defined");
    }

    // Modifiers conversion
    auto mods = keySeq.GetModifiers();

    if (mods.size()) {
        const int i = mods[0];

        if (i & Qt::SHIFT || i == Qt::Key_Shift)
            data.mods |= XCB_MOD_MASK_SHIFT;
        if (i & Qt::CTRL || i == Qt::Key_Control)
            data.mods |= XCB_MOD_MASK_CONTROL;
        if (i & Qt::ALT || i == Qt::Key_Alt)
            data.mods |= 0; // !
        if (i & Qt::META || i == Qt::Key_Meta)
            data.mods |= 0; // !
    }

    return data;
}

#endif
