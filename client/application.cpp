#include <QIcon>
#include <QKeySequence>
#include <QFile>
#include <QTemporaryFile>
#include <QPixmap>
#include <QDesktopWidget>
#include <QDebug>
#include <QClipboard>
#include <QMessageBox>
#include <QBuffer>
#include <QLocalSocket>
#include <QTimer>
#include <QDir>
#include <QScreen>
#include <QThread>
#include <QPixmap>
#include <QDesktopServices>
#include "application.h"
#include "imageselectwidget.h"
#include "ui_config.h"
#include "defines.h"
#include "languageselectdialog.h"
#include "utils.h"
#include "application.h"

#ifdef Q_OS_LINUX
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <string>
#include <climits>

QString getClipboardText() {
    std::string result;
    Display* d = XOpenDisplay(nullptr);
    if(d) {
        Window wParent = DefaultRootWindow(d);
        Window w = XCreateSimpleWindow(
            d, wParent, 0, 0, 1, 1, 0, CopyFromParent, CopyFromParent);
        Atom target = XInternAtom(d, "UTF8_STRING", False);
        XConvertSelection(d, XA_PRIMARY, target, None, w, CurrentTime);
        XFlush(d);

        for(unsigned long offset = 0;;) {
            XEvent ev;
            XNextEvent(d, &ev);
            if(SelectionNotify == ev.type
               && None != ev.xselection.property
            ) {
                Atom actual_type_return = None;
                int actual_format_return = 0;
                unsigned long nitems_return = 0;
                unsigned long bytes_after_return = 0;
                unsigned char* prop_return = nullptr;
                XGetWindowProperty(
                    d, w, ev.xselection.property, offset, INT_MAX
                    , False, AnyPropertyType, &actual_type_return
                    , &actual_format_return, &nitems_return
                    , &bytes_after_return, &prop_return);
                if(nitems_return) {
                    result.append(reinterpret_cast<const char*>(prop_return)
                                  , nitems_return);
                    offset += nitems_return;
                }
                XDeleteProperty(d, w, ev.xselection.property);
                if(!bytes_after_return) {
                    break;
                }
            }
        }
        XDestroyWindow(d, w);
        XCloseDisplay(d);
    }
    return QString(result.c_str());
}
#endif


#ifdef Q_OS_WIN
bool IsTopMost( HWND hwnd )
{
  WINDOWINFO info;
  GetWindowInfo( hwnd, &info );
  return ( info.dwExStyle & WS_EX_TOPMOST ) ? true : false;
}

bool IsFullScreenSize( HWND hwnd, const int cx, const int cy )
{
  RECT r;
  ::GetWindowRect( hwnd, &r );
  return r.right - r.left == cx && r.bottom - r.top == cy;
}

bool IsFullscreenAndMaximized( HWND hwnd )
{
  if( IsTopMost( hwnd ) )
  {
    const int cx = GetSystemMetrics( SM_CXSCREEN );
    const int cy = GetSystemMetrics( SM_CYSCREEN );
    if( IsFullScreenSize( hwnd, cx, cy ) )
      return true;
  }
  return false;
}

#endif



Application::Application(int argc, char *argv[]) :
    QApplication(argc, argv)
    , _configWidget(0)
    , _trayIcon(0)
    , _trayIconMenu(0)
    , _network(0)
    , _settings(0)
{
}

Application::~Application()
{
    _trayIcon->hide();
    _settings->Save();
    delete _trayIconMenu;
}

//---- Get parameters from app instance and upload ---//
void Application::localRequestReceived()
{
    QString path = _localConnection->readAll();
    uploadFile(path); // upload given file
}

//---- Upload file to server ---//
void Application::uploadFile(QString request)
{
    // For get extension of file
    QFileInfo ImgExt;
    ImgExt.setFile(request);

    //----------------------Loading Images-------------------------//
    if ((ImgExt.completeSuffix()=="png")||(ImgExt.completeSuffix()=="jpg")||(ImgExt.completeSuffix()=="gif")){

        if (Sharing) {
            return;
        }

        if (!checkEllapsed()) {
            return;
        }

        Sharing = true;

        QPixmap pixmap;

        // try load image
        if(! pixmap.load(request)){            
            return;
        }

        QString imagetype = settings().GetParameter("general/imagetype", DEFAULT_IMAGE_TYPE);

        QByteArray imageBytes;
        QBuffer buffer(&imageBytes);
        buffer.open(QFile::WriteOnly);
        pixmap.save(&buffer, imagetype.toLocal8Bit().constData());
        buffer.close();
        _network->upload(imageBytes, ImgExt.completeSuffix());
        Sharing = false;
    }

    //----------------------Loading Text-------------------------//
    else if(_languages.contains(ImgExt.completeSuffix())){

        if (Sharing) {
            return;
        }
        if (!checkEllapsed()) {
            return;
        }

        Sharing = true;

        QFile source(request);
        if (!source.open(QIODevice::ReadOnly)){
            return;
        }

        QTextStream textStream(&source);
        QString text = textStream.readAll();
        _network->upload(text.toUtf8(), ImgExt.completeSuffix());

        Sharing = false;
    }
}

bool Application::pxAppInit()
{

    QLocalSocket socket;
    socket.connectToServer(APP_NAME);

    // Check if app instance already run
    if (socket.waitForConnected(500)) {
        // Send args from new instance, and check if args given
        if (QApplication::arguments().count()>1){
            socket.write(QApplication::arguments().at(1).toUtf8());
        }
        qDebug() << "Application already launched!";
        return false;
    }

    _lastSended.start();

    _localServer = new QLocalServer(this);

    if (!_localServer->listen(APP_NAME)) {
        QLocalServer::removeServer(APP_NAME);
        _localServer->listen(APP_NAME);
    }

    connect(_localServer, SIGNAL(newConnection()), SLOT(newLocalSocketConnection()));

    const QString& settingsFile = QDir::homePath() + "/" + SETTINGS_FILE;
    _settings = new USettings(settingsFile);

    QString uuid = _settings->GetParameter("general/uuid", "");
    if (uuid.length() != 24 * 2) {
        uuid = GenerateUUID();
        qDebug() << Q_FUNC_INFO << " uuid = " << uuid;

        Q_ASSERT(uuid.length() == 24 * 2);
        _settings->SetParameter("general/uuid", uuid);
        _settings->Save();
    }

    initLanguages();

    _configWidget = new ConfigWidget(_languages);
    connect(_configWidget, SIGNAL(settingsChanged()), SLOT(setupHotkeys()));
    connect(_configWidget, SIGNAL(hotkeyActivated(size_t)), SLOT(hotkeyPressed(size_t)));

    _configWidget->init();

    _trayIconMenu = new QMenu;
    _trayIconMenu->addAction(tr("About"), this, SLOT(aboutDialog()));
    _trayIconMenu->addAction("", this, SLOT(processCodeShare()));
    _trayIconMenu->addAction("", this, SLOT(processScreenshotFull()));
    _trayIconMenu->addAction("", this, SLOT(processScreenshotPart()));
    _trayIconMenu->addAction(tr("Configure"), _configWidget, SLOT(show()));
    _trayIconMenu->addAction(tr("My files"), this, SLOT(accountLink()));
    _trayIconMenu->addSeparator();
    _trayIconMenu->addAction(tr("Exit"), this, SLOT(quit()));                   // Tray menu

    setupHotkeys();

    _trayIcon = new QSystemTrayIcon(QIcon(":/icons/icon.png"), this);
    connect(_trayIcon, SIGNAL(activated(QSystemTrayIcon::ActivationReason)),
            SLOT(trayIconClicked(QSystemTrayIcon::ActivationReason)));
    _trayIcon->setContextMenu(_trayIconMenu);                                   // Tray icon

    _network = new Network(this);
    connect(_network, SIGNAL(linkReceived(QString)), SLOT(linkAvaliable(QString)));     // Network
    connect(_network, SIGNAL(trayMessage(QString,QString)), SLOT(trayMessage(QString,QString)));

    this->setQuitOnLastWindowClosed(false);

    _trayIcon->show();
    QFile file(settingsFile);

    _lastSended = _lastSended.addSecs(5); // for check on first run
    connect(_network, SIGNAL(ready()), SLOT(resolved()));

    if (!file.exists())
        _configWidget->show();
    return true;
}

void Application::resolved()
{
    // If program have parameter upload file
    if (QApplication::arguments().count()>1){
        qDebug() << Application::arguments().at(1).toUtf8();
        uploadFile(QApplication::arguments().at(1).toUtf8());
    }
}

void Application::newLocalSocketConnection()
{
    _localConnection = _localServer->nextPendingConnection();
    connect(_localConnection, SIGNAL(readyRead()), SLOT(localRequestReceived()));
}

void Application::trayMessage(const QString &caption, const QString &text)
{
    _trayIcon->showMessage(caption, text, QSystemTrayIcon::Information, 6500);
}

void Application::processScreenshot(bool isFullScreen)
{
    if (Sharing) {
        return;
    }
    if (!checkEllapsed()) {
        return;
    }
    Sharing = true;
    QPixmap pixmap;
    #if defined(Q_OS_LINUX)
    pixmap = QGuiApplication::primaryScreen()->grabWindow(0);

    #elif defined(Q_OS_WIN)


    if(IsFullscreenAndMaximized(GetForegroundWindow()))
    {

        //pixmap.load("screenshot.bmp"); //Still not have working function for take snapshot from full screen applications
    }

    else
    {
        pixmap = QGuiApplication::primaryScreen()->grabWindow(0);
    }

    //pixmap = QGuiApplication::primaryScreen()->grabWindow(0);
    #endif

    if (!isFullScreen) {
        ImageSelectWidget imageSelectDialog(&pixmap);
        imageSelectDialog.setWindowState(Qt::WindowFullScreen);
        if (!imageSelectDialog.exec()) {
            Sharing = false;
            return;
        }
    }

//    QString imagetype = _settings->value("general/imagetype", DEFAULT_IMAGE_TYPE).toString();
    QString imagetype = settings().GetParameter("general/imagetype", DEFAULT_IMAGE_TYPE);

    QByteArray imageBytes;
    QBuffer buffer(&imageBytes);
    buffer.open(QFile::WriteOnly);
    pixmap.save(&buffer, imagetype.toLocal8Bit().constData());
    buffer.close();
    _network->upload(imageBytes, imagetype);
    Sharing = false;
}

void Application::processCodeShare()
{
    if (!checkEllapsed()) {
        return;
    }

//    bool showsourcedialog = _settings->value("general/showsourcedialog", DEFAULT_SHOW_SOURCES_CONF_DIALOG).toBool();
    bool showsourcedialog = settings().GetParameter("general/showsourcedialog", ToString(DEFAULT_SHOW_SOURCES_CONF_DIALOG));
    if (showsourcedialog) {
        LanguageSelectDialog dialog(_languages);
        if (!dialog.exec()) {
            return;
        }
    }

#ifdef Q_OS_WIN
    QApplication::processEvents();

    INPUT input[4];

    input[0].type = INPUT_KEYBOARD;
    input[0].ki.wVk = VK_CONTROL;
    input[0].ki.wScan = 0;
    input[0].ki.dwFlags = 0;
    input[0].ki.time = 0;
    input[0].ki.dwExtraInfo = 0;

    input[1].type = INPUT_KEYBOARD;
    input[1].ki.wVk = 'C';
    input[1].ki.wScan = 0;
    input[1].ki.dwFlags = 0;
    input[1].ki.time = 10;
    input[1].ki.dwExtraInfo = 0;

    input[2].type = INPUT_KEYBOARD;
    input[2].ki.wVk = 'C';
    input[2].ki.wScan = 0;
    input[2].ki.dwFlags = KEYEVENTF_KEYUP;
    input[2].ki.time = 20;
    input[2].ki.dwExtraInfo = 0;

    input[3].type = INPUT_KEYBOARD;
    input[3].ki.wVk = VK_CONTROL;
    input[3].ki.wScan = 0;
    input[3].ki.dwFlags = KEYEVENTF_KEYUP;
    input[3].ki.time = 30;
    input[3].ki.dwExtraInfo = 0;

    SendInput(4, input, sizeof(INPUT));

    #endif
    _timerId = this->startTimer(200);
}

void Application::accountLink()
{
    QDesktopServices::openUrl(QUrl(Application::GetAccountUrl()));
}

void Application::trayIconClicked(const QSystemTrayIcon::ActivationReason &button)
{
    switch (button)
    {
    case QSystemTrayIcon::Trigger:
        _configWidget->show();
        break;
    case QSystemTrayIcon::MiddleClick:
        this->quit();
        break;
    default:
        break;
    }
}

void Application::linkAvaliable(const QString &link)
{
    QApplication::clipboard()->setText(link);
    _trayIcon->showMessage(tr("Done!"), tr("File shared!"), QSystemTrayIcon::Information, 6500);
}

void Application::aboutDialog()
{
    QMessageBox::information(NULL, APP_NAME,
                             tr("Utility for easy screenshoting and code sharing<br> We live at <a href=\"http://pastexen.com/\">pastexen.com</a><br> Hosting provided by <a href=\"http://scalaxy.ru/\">scalaxy.ru</a>"));
}

void Application::setupHotkeys()
{
//    QString fullHotkey = _settings->value("general/fullhotkey", DEFAULT_HOTKEY_FULL).toString();
//    QString partHotkey = _settings->value("general/parthotkey", DEFAULT_HOTKEY_PART).toString();
//    QString codeHotkey = _settings->value("general/texthotkey", DEFAULT_HOTKEY_CODE).toString();
    QString fullHotkey = settings().GetParameter("general/fullhotkey", DEFAULT_HOTKEY_FULL);
    QString partHotkey = settings().GetParameter("general/parthotkey", DEFAULT_HOTKEY_PART);
    QString codeHotkey = settings().GetParameter("general/texthotkey", DEFAULT_HOTKEY_CODE);

    QList<QAction*> actsList = _trayIconMenu->actions();
    actsList[1]->setText(tr("Text share (%1)").arg(codeHotkey));
    actsList[2]->setText(tr("Full s-shot (%1)").arg(fullHotkey));
    actsList[3]->setText(tr("Half s-shot (%1)").arg(partHotkey));
}

void Application::initLanguages()
{
    _languages.insert("auto", tr("Auto detection"));
    _languages.insert("txt", tr("Plain text"));
    _languages.insert("c", "C");
    _languages.insert("cpp", "C++ ");
    _languages.insert("h", "C header");
    _languages.insert("hpp", "C header");
    _languages.insert("cs", "C#");
    _languages.insert("java", "Java");
    _languages.insert("php", "PHP");
    _languages.insert("py", "Python");
    _languages.insert("pl", "Perl");
    _languages.insert("pas", "Pascal");
    _languages.insert("m", "ObjectiveC");
    _languages.insert("xml", "XML");
    _languages.insert("html", "HTML");
    _languages.insert("js", "Javascript");
    _languages.insert("css", "CSS");
    _languages.insert("json", "Json");
    _languages.insert("as", "ActionScript");
    _languages.insert("vb", "VBscript");
    _languages.insert("d", "D");
    _languages.insert("sql", "SQL");
    _languages.insert("st", "Smalltalk");
    _languages.insert("lisp", "LISP");
    _languages.insert("ini", "ini");
    _languages.insert("conf", "Apache");
    _languages.insert("sh", "BASH");
    _languages.insert("bat", "Dos (bat)");
    _languages.insert("cmake", "CMake");
    _languages.insert("hs", "Haskell");
}

bool Application::checkEllapsed()
{
    if (_lastSended.elapsed() < 3000) {
        return false;
    }
    _lastSended.restart();
    return true;
}

void Application::timerEvent(QTimerEvent *) {
    if (_timerId == -1) { // Fix for unknown timerEvent-s, #108
        return;
    }
    this->killTimer(_timerId);
    _timerId = -1;

    QString sourcestype = settings().GetParameter("general/sourcetype", DEFAULT_SOURCES_TYPE);

    QString text;

#ifdef Q_OS_LINUX
    Display *dpy = XOpenDisplay(NULL);
    if (!dpy) qDebug() << "DPU ERROR!";
    XTestFakeKeyEvent(dpy, KEYCODE_LCONTROL, KEY_DOWN, CurrentTime);
    XTestFakeKeyEvent(dpy, KEYCODE_C, KEY_DOWN, 0);
    QThread::msleep(200);
    XTestFakeKeyEvent(dpy, KEYCODE_LCONTROL, KEY_UP, CurrentTime);
    XTestFakeKeyEvent(dpy, KEYCODE_C, KEY_UP, 0);
    XCloseDisplay(dpy);
    text = getClipboardText();
#elif defined(Q_OS_WIN)
    text = QApplication::clipboard()->text();
#endif

    if (text.count() == 0) {
        _trayIcon->showMessage(tr("Error!"), tr("No text found in clipboard"), QSystemTrayIcon::Information, 6500);
        return;
    }
    _network->upload(text.toUtf8(), sourcestype);
}
