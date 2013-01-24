#include <QtCore>
#if defined(Q_OS_WIN)
#include <windows.h>
#include "winhotkeymap.h"
#else defined(Q_OS_LINUX)
#include "xcb/xcb_keysyms.h"
#include "X11/keysym.h"
#endif
#include "uglobalhotkeys.h"
#include <QGuiApplication>
#include <QWindow>
#include <qpa/qplatformnativeinterface.h>

UGlobalHotkeys::UGlobalHotkeys(NativeEventFilter *eventFilter, QWidget *parent)
    : QWidget(parent), evFilter(eventFilter)
{
#ifdef Q_OS_LINUX
    void* v = qGuiApp->platformNativeInterface()->nativeResourceForWindow("connection", new QWindow);
    c = (xcb_connection_t*)v;
    wId = xcb_setup_roots_iterator(xcb_get_setup(c)).data->root;
    keySyms = xcb_key_symbols_alloc(c);
#endif
    qApp->installNativeEventFilter(eventFilter);
    eventFilter->setGlobalHotkeyObj(this);
}

void UGlobalHotkeys::RegisterHotkey(const QString& keySeq, size_t id) {
    RegisterHotkey(UKeySequence(keySeq), id);
}

void UGlobalHotkeys::RegisterHotkey(const UKeySequence& keySeq, size_t id) {
    if (keySeq.Size() == 0) {
        throw UException("Empty hotkeys");
    }
    if (Registered.find(id) != Registered.end()) {
        UnregisterHotkey(id);
    }
#if defined(Q_OS_WIN)
    size_t winMod = 0;
    size_t key = VK_F2;

    for (size_t i = 0; i != keySeq.Size(); i++) {
        if (keySeq[i] == Qt::Key_Control) {
            winMod |= MOD_CONTROL;
        } else if (keySeq[i] == Qt::Key_Alt) {
            winMod |= MOD_ALT;
        } else if (keySeq[i] == Qt::Key_Shift) {6
            winMod |= MOD_SHIFT;
        } else {
            key = QtKeyToWin(keySeq[i]);
        }
    }

    if (!RegisterHotKey((HWND)winId(), id, winMod, key)) {
        qDebug() << "Error activating hotkey!";
    } else {
        Registered.insert(id);
    }
#elif defined(Q_OS_LINUX)
    UHotkeyData data;

    xcb_keycode_t *keyC = xcb_key_symbols_get_keycode(keySyms, XK_F10);
    int mods = XCB_MOD_MASK_CONTROL | XCB_MOD_MASK_SHIFT;

    data.keyCode = *keyC;
    data.mods = mods;

    xcb_grab_key(c, 1, wId, data.mods, data.keyCode, XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC);

    Registered.insert(id, data);
#endif
}

void UGlobalHotkeys::UnregisterHotkey(size_t id) {
    Q_ASSERT(Registered.find(id) != Registered.end() && "Unregistered hotkey");
#if defined(Q_OS_WIN)
    UnregisterHotKey((HWND)winId(), id);
#elif defined(Q_OS_LINUX)
    UHotkeyData data = Registered.take(id);

    xcb_ungrab_key(c, data.keyCode, wId, data.mods);
#endif
    Registered.remove(id);
}

UGlobalHotkeys::~UGlobalHotkeys() {
    for (QHash<size_t, UHotkeyData>::iterator i = Registered.begin(); i != Registered.end(); i++) {
#if defined(Q_OS_WIN)
        UnregisterHotKey((HWND)winId(), i.key());
#endif
    }
}


#if defined(Q_OS_WIN)
bool UGlobalHotkeys::winEvent(MSG * message, long * result) {
    Q_UNUSED(result);
    if (message->message == WM_HOTKEY) {
        size_t id = message->wParam;
        Q_ASSERT(Registered.find(id) != Registered.end() && "Unregistered hotkey");
        emit Activated(id);
    }
    return false;
}

#elif defined(Q_OS_LINUX)
bool UGlobalHotkeys::linuxEvent(void *message)
{
    xcb_generic_event_t* ev = (xcb_generic_event_t*)message;

    switch (ev->response_type & ~0x80) {
    case XCB_KEY_PRESS:
        fprintf(stderr, "KeyEvent\n");
    }

    return false;
}
#endif
bool UGlobalHotkeys::nativeEvent(const QByteArray &eventType,
                                       void *message, long *result)
{
    qDebug() << Q_FUNC_INFO;
    Q_UNUSED(eventType);
#if defined(Q_OS_WIN)
    return winEvent((MSG*)message, result);
#elif defined(Q_OS_LINUX)
    return linuxEvent(message);
#endif
}
