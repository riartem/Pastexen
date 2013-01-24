#include "nativeeventfilter.h"
#include "uglobalhotkeys.h"

NativeEventFilter::NativeEventFilter() :
    hk(0)
{
}

bool NativeEventFilter::nativeEventFilter(const QByteArray &eventType, void *message, long *result)
{
    if (hk)
        hk->nativeEvent(eventType, message, result);

    return nativeEventHandler(eventType, message, result);
}

bool NativeEventFilter::nativeEventHandler(const QByteArray &eventType, void *message, long *result)
{
    return false;
}

void NativeEventFilter::setGlobalHotkeyObj(UGlobalHotkeys *obj)
{
    hk = obj;
}
