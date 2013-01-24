#ifndef NATIVEEVENTFILTER_H
#define NATIVEEVENTFILTER_H

#include <QAbstractNativeEventFilter>

/**********************************************************
If you need new functionality, override nativeEventHandler().

FUCKING QT!!

***********************************************************/
class UGlobalHotkeys;

class NativeEventFilter : public QAbstractNativeEventFilter
{
    friend class UGlobalHotkeys;
public:
    NativeEventFilter();
    bool nativeEventFilter(const QByteArray &eventType, void *message, long *result) override final;
    virtual bool nativeEventHandler(const QByteArray &eventType, void *message, long *result);
private:
    void setGlobalHotkeyObj(UGlobalHotkeys *obj);
    UGlobalHotkeys *hk;
};

#endif // NATIVEEVENTFILTER_H
