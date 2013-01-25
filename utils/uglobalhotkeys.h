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

class UGlobalHotkeys : public QObject
{
    Q_OBJECT

    friend class NativeEventFilter;
public:
    explicit UGlobalHotkeys(QObject *parent = 0);
    void RegisterHotkey(const QString& keySeq, size_t id = 1);
    void RegisterHotkey(const UKeySequence& keySeq, size_t id = 1);
    void UnregisterHotkey(size_t id = 1);
    ~UGlobalHotkeys();

signals:
    void Activated(size_t id);
private:

    NativeEventFilter *evFilter;
};

