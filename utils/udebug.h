#pragma once

#include <QDebug>
#include <memory>

class UDebugOut
{
public:
#ifndef QT_NO_DEBUG
    UDebugOut() : Debug(new QDebug(QtDebugMsg)) {}
#endif

    template<typename T>
    UDebugOut& operator << (const T& obj) {
#ifndef QT_NO_DEBUG
        *Debug << obj;
#else
        Q_UNUSED(obj);
#endif
        return *this;
    }

private:
#ifndef QT_NO_DEBUG
    std::unique_ptr<QDebug> Debug;
#endif
};

#define UDebug (UDebugOut())
