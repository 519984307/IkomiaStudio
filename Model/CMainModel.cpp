// Copyright (C) 2021 Ikomia SAS
// Contact: https://www.ikomia.com
//
// This file is part of the IkomiaStudio software.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Affero General Public License for more details.
//
// You should have received a copy of the GNU Affero General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

#include "CMainModel.h"
#include "PythonThread.hpp"
#include "Main/AppTools.hpp"
#include "Model/Matomo/piwiktracker.h"
#include "CLogManager.h"

#define TYTI_PYLOGHOOK_USE_BOOST
#include "Main/PyLogHook.hpp"

void logMessageOutput(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    CLogManager::instance().handleMessage(type, context, msg);
}

CMainModel::CMainModel()
{
    //Initialisation is made in the init() method
}

CMainModel::~CMainModel()
{
    m_mlflowProcess->kill();
    m_userMgr.beforeAppClose();
}

CProjectManager* CMainModel::getProjectManager()
{
    return &m_projectMgr;
}

CProcessManager* CMainModel::getProcessManager()
{
    return &m_processMgr;
}

CProtocolManager* CMainModel::getProtocolManager()
{
    return &m_protocolMgr;
}

CRenderManager* CMainModel::getRenderManager()
{
    return &m_renderMgr;
}

CProgressBarManager* CMainModel::getProgressManager()
{
    return &m_progressMgr;
}

CGraphicsManager* CMainModel::getGraphicsManager()
{
    return &m_graphicsMgr;
}

CResultManager* CMainModel::getResultManager()
{
    return &m_resultsMgr;
}

CUserManager *CMainModel::getUserManager()
{
    return &m_userMgr;
}

CMainDataManager* CMainModel::getDataManager()
{
    return &m_dataMgr;
}

CStoreManager *CMainModel::getStoreManager()
{
    return &m_storeMgr;
}

CSettingsManager*CMainModel::getSettingsManager()
{
    return &m_settingsMgr;
}

CPluginManager *CMainModel::getPluginManager()
{
    return &m_pluginMgr;
}

void CMainModel::init()
{
    //Take care of initialization order
    initLogFile();
    initDb();
    initSettingsManager();
    initPython();
    initProjectManager();
    initPluginManager();
    initProcessManager();
    initProtocolManager();
    initGraphicsManager();
    initResultsManager();
    initRenderManager();
    initDataManager();
    initUserManager();
    initStoreManager();
    initMatomo();
    initConnections();
}

void CMainModel::notifyViewShow()
{
    m_processMgr.notifyViewShow();
    m_protocolMgr.notifyViewShow();
    m_graphicsMgr.notifyViewShow();
    m_userMgr.notifyViewShow();
    m_pluginMgr.notifyViewShow();
    m_settingsMgr.notifyViewShow();
}

void CMainModel::initConnections()
{
    connect(&m_userMgr, &CUserManager::doSetCurrentUser, this, &CMainModel::onSetCurrentUser);
}

void CMainModel::onOpenImage(const QModelIndex& index)
{
    Q_UNUSED(index)
}

void CMainModel::onSetCurrentUser(const CUser &user)
{
    //Take care of initialization order
    m_pluginMgr.setCurrentUser(user);
    m_processMgr.setCurrentUser(user);
    m_storeMgr.setCurrentUser(user);
    m_protocolMgr.setCurrentUser(user);
}

void CMainModel::initLogFile()
{
    m_logFilePath = Utils::IkomiaApp::getAppFolder() + "/log.txt";

    QFile file(QString::fromStdString(m_logFilePath));
    if(!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
        return;

    // Write all Qt Message in log file
    qInstallMessageHandler(logMessageOutput);
    CLogManager::instance().addOutputManager(std::bind(&CMainModel::writeLogMsg, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    Utils::File::createDirectory(Utils::IkomiaApp::getAppFolder() + "/Resources/Tmp");
    CLogManager::instance().setStdRedirection(Utils::IkomiaApp::getQAppFolder() + "/Resources/Tmp/stdout_err.txt");
}

void CMainModel::initDb()
{
    m_dbMgr.init();
}

void CMainModel::initProjectManager()
{
    m_projectMgr.setManagers(&m_graphicsMgr, &m_renderMgr, &m_resultsMgr, &m_protocolMgr, &m_progressMgr, &m_dataMgr);
}

void CMainModel::initProcessManager()
{
    emit doSetSplashMessage(tr("Load process library and plugins..."), Qt::AlignCenter, qApp->palette().highlight().color());
    QCoreApplication::processEvents();
    m_processMgr.setManagers(&m_pluginMgr, &m_protocolMgr);
    m_processMgr.init();
}

void CMainModel::initProtocolManager()
{
    m_protocolMgr.setManagers(&m_processMgr, &m_projectMgr, &m_graphicsMgr,
                              &m_resultsMgr, &m_dataMgr, &m_progressMgr, &m_settingsMgr);
    startLocalMLflowServer();
}

void CMainModel::initGraphicsManager()
{
    m_graphicsMgr.setManagers(&m_projectMgr, &m_protocolMgr);
}

void CMainModel::initResultsManager()
{
    m_resultsMgr.init();
    m_resultsMgr.setManagers(&m_projectMgr, &m_protocolMgr, &m_graphicsMgr, &m_renderMgr, &m_dataMgr, &m_progressMgr);
}

void CMainModel::initRenderManager()
{
    m_renderMgr.setManagers(&m_progressMgr);
}

void CMainModel::initDataManager()
{
    m_dataMgr.setManagers(&m_projectMgr, &m_protocolMgr, &m_graphicsMgr, &m_resultsMgr, &m_renderMgr, &m_progressMgr);
}

void CMainModel::initUserManager()
{
    m_userMgr.init();
    m_userMgr.setManagers(&m_networkMgr);
}

void CMainModel::initStoreManager()
{
    m_storeMgr.setManagers(&m_networkMgr, &m_processMgr, &m_pluginMgr, &m_progressMgr);
}

void CMainModel::initSettingsManager()
{
    // init settings manager
    m_settingsMgr.init();
}

void CMainModel::initPluginManager()
{
    m_pluginMgr.setRegistrator(&m_processMgr.m_processRegistrator);
}

void CMainModel::initPython()
{
    checkUserInstall();

    emit doSetSplashMessage(tr("Configure Python environment..."), Qt::AlignCenter, qApp->palette().highlight().color());
    QCoreApplication::processEvents();
    QString pythonPath = Utils::IkomiaApp::getQAppFolder() + "/Python";

    // Set program if existing
    QDir pythonDir(pythonPath);
    if(pythonDir.exists())
    {
        //Define embedded Python paths
#if defined(Q_OS_MACOS)
        QString delimiter = ":";
        std::string pythonExe = "/bin/python" + Utils::Python::_python_bin_prod_version;
        std::string pythonLib = "/lib/python" + Utils::Python::_python_lib_prod_version + ":";
        std::string pythonDynload = "/lib/python" + Utils::Python::_python_lib_prod_version + "/lib-dynload:";
        std::string pythonSitePackages = "/lib/python" + Utils::Python::_python_lib_prod_version + "/site-packages:";
#elif defined(Q_OS_LINUX)
        QString delimiter = ":";
        std::string pythonExe = "/bin/python" + Utils::Python::_python_bin_prod_version;
        std::string pythonLib = "/lib/python" + Utils::Python::_python_lib_prod_version + ":";
        std::string pythonDynload = "/lib/python" + Utils::Python::_python_lib_prod_version + "/lib-dynload:";
        std::string pythonSitePackages = "/lib/python" + Utils::Python::_python_lib_prod_version + "/site-packages:";
#elif defined(Q_OS_WIN64)
        QString delimiter = ";";
        std::string pythonExe = "/python.exe";
        std::string pythonLib = "/lib;";
        std::string pythonDynload = "/DLLs;";
        std::string pythonSitePackages = "/lib/site-packages;";
#endif
        //Embedded Python executable
        auto filepath = pythonPath.toStdString() + pythonExe;
        auto s = filepath.size();
        Py_SetProgramName(Py_DecodeLocale(filepath.c_str(), &s));

        //Set Python path
        std::string modulePath = pythonPath.toStdString() + pythonLib;
        modulePath += pythonPath.toStdString() + pythonDynload;
        modulePath += pythonPath.toStdString() + pythonSitePackages;

#ifndef QT_DEBUG
        modulePath += Utils::IkomiaApp::getAppFolder() + "/Api";
#else
    #ifdef Q_OS_WIN64
        modulePath += "C:/Developpement/Ikomia/Build/Lib/Python;";
    #else
        modulePath += QDir::homePath().toStdString() + "/Developpement/Ikomia/Build/Lib/Python:";
    #endif
#endif

        auto sp = modulePath.size();
        Py_SetPath(Py_DecodeLocale(modulePath.c_str(), &sp));

        //Add $HOME/python/bin to $PATH
        QString pathenv = qEnvironmentVariable("PATH");
        pathenv = pythonPath + "/bin" + delimiter + pathenv;
        qputenv("PATH", pathenv.toUtf8());
    }
    else
    {
        qWarning().noquote() << "Embedded Python not found:" << pythonPath;

#if defined(Q_OS_MACOS)
        std::string pythonExe = "/bin/python" + Utils::Python::_python_bin_prod_version;
        std::string pythonLib = "/lib/python" + Utils::Python::_python_lib_prod_version + ":";
        std::string pythonDynload = "/lib/python" + Utils::Python::_python_lib_prod_version + "/lib-dynload:";
        std::string pythonSitePackages = "/lib/python" + Utils::Python::_python_lib_prod_version + "/site-packages:";
        std::string ikomiaApi = QDir::homePath().toStdString() + "/Developpement/IkomiaApi/Build/Lib/Python:";
#elif defined(Q_OS_LINUX)
        std::string pythonExe = "/usr/bin/python" + Utils::Python::getDevBinVersion();
        std::string pythonLib = ":/usr/lib/python" + Utils::Python::getDevLibVersion() + ":";
        std::string pythonDynload = "/usr/lib/python" + Utils::Python::getDevLibVersion() + "/lib-dynload:";
        std::string pythonSitePackages = "/usr/lib/python" + Utils::Python::getDevLibVersion() + "/site-packages:";
        std::string ikomiaApi = QDir::homePath().toStdString() + "/Developpement/IkomiaApi/Build/Lib/Python:";
#elif defined(Q_OS_WIN64)
        std::string programFilesPath = getenv("PROGRAMFILES");
        std::string pythonFolder = programFilesPath + "/Python" + Utils::Python::getDevBinVersion();
        std::string pythonExe = pythonFolder + "/python.exe";
        std::string pythonLib = ";" + pythonFolder + "/Lib;";
        std::string pythonDynload = pythonFolder + "/DLLs;";
        std::string pythonSitePackages = pythonFolder + "/Lib/site-packages;";
        std::string ikomiaApi = "C:/Developpement/IkomiaApi/Build/Lib/Python;";
#endif
        auto s = pythonExe.size();
        Py_SetProgramName(Py_DecodeLocale(pythonExe.c_str(), &s));

        //Set Python path
        std::string modulePath = pythonExe;
        modulePath += pythonLib;
        modulePath += pythonDynload;
        modulePath += pythonSitePackages;
        modulePath += ikomiaApi;
        auto sp = modulePath.size();
        Py_SetPath(Py_DecodeLocale(modulePath.c_str(), &sp));
    }

    //Initialize Python interpreter
    Py_Initialize();

    // Set multiprocessing executable to launch python interpreter
    // for subprocess instead of Ikomia App...
    std::string pythonExe = pythonPath.toStdString() + "/python.exe";
    boost::python::object main_module = boost::python::import("__main__");
    boost::python::object main_namespace = main_module.attr("__dict__");
    boost::python::object multiprocessing = boost::python::import("multiprocessing");
    multiprocessing.attr("set_executable")(pythonExe);
    main_namespace["multiprocessing"] = multiprocessing;

    //Threading
    if(PyEval_ThreadsInitialized() == 0)
        PyEval_InitThreads();

    //Release GIL
    PyThreadState* st = PyEval_SaveThread();
    Q_UNUSED(st);

    //Set sys.stdout and sys.stderr redirection
    CPyEnsureGIL gil;
    tyti::pylog::redirect_stdout([&](const char* w){ qInfo().noquote() << QString(w); });
    tyti::pylog::redirect_stderr([&](const char* w){ qCritical().noquote() << QString(w); });
}

void CMainModel::initMatomo()
{
    PiwikTracker* pPiwikTracker = new PiwikTracker(qApp, QUrl(Utils::Network::getMatomoUrl()), MATOMO_APP_ID);
    pPiwikTracker->sendVisit("main", "Application_Started");
}

void CMainModel::writeLogMsg(int type, const QString& msg, const QString& categoryName)
{
    Q_UNUSED(categoryName);

    QFile file(QString::fromStdString(m_logFilePath));
    if(!file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
        return;

    QString msgType;
    switch(type)
    {
        case QtDebugMsg: msgType = "DEBUG:"; break;
        case QtInfoMsg: msgType = "INFO:"; break;
        case QtWarningMsg: msgType = "WARNING:"; break;
        case QtCriticalMsg: msgType = "CRITICAL:"; break;
        case QtFatalMsg: msgType = "FATAL:"; break;
        default: msgType = "INFO:"; break;
    }

    auto date = QDate::currentDate();
    auto time = QTime::currentTime();
    QString str = msgType + date.toString() + " " + time.toString() + ": " + msg;

    QTextStream out(&file);
    out << str << "\n";
    file.close();
}

void CMainModel::checkUserInstall()
{
#if defined(Q_OS_WIN64)
    QString srcPythonFolder = QCoreApplication::applicationDirPath() + "/Python";
    QString srcApiFolder = QCoreApplication::applicationDirPath() + "/Api";
    QString srcResourcesFolder = QCoreApplication::applicationDirPath() + "/Resources";
#elif defined(Q_OS_LINUX)
    QString srcPythonFolder = "/opt/Ikomia/Python";
    QString srcApiFolder = "/opt/Ikomia/Api";
    QString srcResourcesFolder = "/opt/Resources";
#elif defined(Q_OS_MACOS)
    QString srcPythonFolder = "/usr/local/Ikomia/Python";
    QString srcApiFolder = "/usr/local/Ikomia/Api";
    QString srcResourcesFolder = "/usr/local/Resources";
#endif

    //Python
    QString appFolder = Utils::IkomiaApp::getQAppFolder();
    QString userPythonFolder = appFolder + "/Python";

    if(QDir(userPythonFolder).exists() == false)
    {
        emit doSetSplashMessage(tr("Install Python environment...\n(Please be patient, this may take a while)"), Qt::AlignCenter, qApp->palette().highlight().color());
        QCoreApplication::processEvents();

        //Copy Python directory
        Utils::File::copyDirectory(srcPythonFolder, userPythonFolder, true);
        //Install Python required packages
        installPythonRequirements();
    }

    //Copy Ikomia API
    QString userApiFolder = appFolder + "/Api";
    if(QDir(userApiFolder).exists() == false)
    {
        emit doSetSplashMessage(tr("Install Ikomia API..."), Qt::AlignCenter, qApp->palette().highlight().color());
        QCoreApplication::processEvents();
        Utils::File::copyDirectory(srcApiFolder, userApiFolder, true);
    }

    // Copy Resources
    QString userResourcesFolder = appFolder + "/Resources";
    if(QDir(userResourcesFolder).exists() == false)
    {
        emit doSetSplashMessage(tr("Install Ikomia Resources..."), Qt::AlignCenter, qApp->palette().highlight().color());
        QCoreApplication::processEvents();
        Utils::File::copyDirectory(srcResourcesFolder, userResourcesFolder, true);
    }

    QString userGmicFolder = Utils::IkomiaApp::getGmicFolder();
    if(QDir(userGmicFolder).exists() == false)
    {
        emit doSetSplashMessage(tr("Install Gmic resources..."), Qt::AlignCenter, qApp->palette().highlight().color());
        QCoreApplication::processEvents();
        Utils::File::copyDirectory(srcResourcesFolder + "/gmic", userGmicFolder, true);
    }
}

void CMainModel::installPythonRequirements()
{
    emit doSetSplashMessage(tr("Install Python packages...\n(Please be patient, this may take a while)"), Qt::AlignCenter, qApp->palette().highlight().color());
    QCoreApplication::processEvents();

    QString userPythonFolder = Utils::IkomiaApp::getQAppFolder() + "/Python";
    QString requirementsPath = userPythonFolder + "/requirements.txt";
    Utils::Python::installRequirements(requirementsPath);
}

void CMainModel::startLocalMLflowServer()
{
    QStringList args;
    QString cmd = "mlflow";
    QString backendStoreUri = QString::fromStdString(Utils::MLflow::getBackendStoreURI());
    QString artifactRootUri = QString::fromStdString(Utils::MLflow::getArtifactURI());

    // Create local directory to store MLflow experiments and runs
    Utils::File::createDirectory(Utils::MLflow::getBackendStoreURI());

    // Launch server
#ifdef Q_OS_WIN
    backendStoreUri.remove(":");
    artifactRootUri.remove(":");
    backendStoreUri = "file://" + backendStoreUri;
    artifactRootUri = "file://" + artifactRootUri;
#endif

    args << "server";
    args << "--backend-store-uri" << backendStoreUri;
    args << "--default-artifact-root" << artifactRootUri;
    args << "--host" << "0.0.0.0";

    m_mlflowProcess = new QProcess(this);
    connect(m_mlflowProcess, &QProcess::errorOccurred, [&](QProcess::ProcessError error)
    {
        QString msg;
        switch(error)
        {
            case QProcess::FailedToStart:
                msg = tr("Failed to launch MLflow server. Check if the process is already running or if mlflow Python package is correctly installed.");
                break;
            case QProcess::Crashed:
                msg = tr("MLflow server crashed.");
                break;
            case QProcess::Timedout:
                msg = tr("MLflow server do not respond. Process is waiting...");
                break;
            case QProcess::UnknownError:
                msg = tr("MLflow server encountered an unknown error...");
                break;
        }

        qInfo().noquote() << msg;
    });
    connect(m_mlflowProcess, &QProcess::started, [&]
    {
        qInfo().noquote() << tr("MLflow server launched successfully.");
    });
    m_mlflowProcess->start(cmd, args);
}
