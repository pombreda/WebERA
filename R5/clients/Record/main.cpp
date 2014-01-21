/*
 * Copyright (C) 2009 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2009 Girish Ramakrishnan <girish@forwardbias.in>
 * Copyright (C) 2006 George Staikos <staikos@kde.org>
 * Copyright (C) 2006 Dirk Mueller <mueller@kde.org>
 * Copyright (C) 2006 Zack Rusin <zack@kde.org>
 * Copyright (C) 2006 Simon Hausmann <hausmann@kde.org>
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <fstream>
#include <iostream>

#include <QString>
#include <QTimer>

#include <WebCore/platform/ThreadTimers.h>
#include <WebCore/platform/ThreadGlobalData.h>
#include <JavaScriptCore/runtime/JSExportMacros.h>
#include <WebCore/platform/network/qt/QNetworkReplyHandler.h>
#include <WebCore/platform/schedule/DefaultScheduler.h>

#include "utils.h"
#include "clientapplication.h"
#include "network.h"
#include "specificationscheduler.h"
#include "datalog.h"
#include "autoexplorer.h"

#include "wtf/ActionLogReport.h"

class RecordClientApplication : public ClientApplication {
    Q_OBJECT

public:
    RecordClientApplication(int& argc, char** argv);

private:
    void handleUserOptions();

private:
    bool m_running;

    QString m_schedulePath;
    QString m_logTimePath;
    QString m_logRandomPath;
    QString m_url;

    unsigned int m_autoExplorePreTimout;
    unsigned int m_autoExploreTimout;
    bool m_autoExplore;

    bool m_showWindow;

    QNetworkReplyControllableFactoryRecord* m_controllableFactory;
    TimeProviderRecord* m_timeProvider;
    RandomProviderRecord* m_randomProvider;
    //SpecificationScheduler* m_scheduler;
    WebCore::DefaultScheduler* m_scheduler;
    AutoExplorer* m_autoExplorer;

public slots:
    void slOnCloseEvent();
};

RecordClientApplication::RecordClientApplication(int& argc, char** argv)
    : ClientApplication(argc, argv)
    , m_running(true)
    , m_schedulePath("/tmp/schedule.data")
    , m_logTimePath("/tmp/log.time.data")
    , m_logRandomPath("/tmp/log.random.data")
    , m_autoExplorePreTimout(30)
    , m_autoExploreTimout(30)
    , m_autoExplore(false)
    , m_showWindow(true)
    , m_controllableFactory(new QNetworkReplyControllableFactoryRecord())
    , m_timeProvider(new TimeProviderRecord())
    , m_randomProvider(new RandomProviderRecord())
    //, m_scheduler(new SpecificationScheduler(m_controllableFactory))
    , m_scheduler(new WebCore::DefaultScheduler())
    , m_autoExplorer(new AutoExplorer(m_window, m_window->page()->mainFrame()))
{
    QObject::connect(m_window, SIGNAL(sigOnCloseEvent()), this, SLOT(slOnCloseEvent()));
    handleUserOptions();

    // Recording specific setup

    m_timeProvider->attach();
    m_randomProvider->attach();

    WebCore::ThreadTimers::setScheduler(m_scheduler);
    WebCore::QNetworkReplyControllableFactory::setFactory(m_controllableFactory);

    m_window->page()->networkAccessManager()->setCookieJar(new WebCore::QNetworkSnapshotCookieJar(this));

    // Load and explore the website

    if (m_autoExplore) {
        m_autoExplorer->explore(m_url, m_autoExplorePreTimout, m_autoExploreTimout);
        QObject::connect(m_autoExplorer, SIGNAL(done()), this, SLOT(slOnCloseEvent()));
    } else {
        loadWebsite(m_url);
    }

    // Show window while running

    if (m_showWindow) {
        m_window->show();
    }
}

void RecordClientApplication::handleUserOptions()
{
    QStringList args = arguments();

    if (args.contains("-help")) {
        qDebug() << "Usage:" << m_programName.toLatin1().data()
                 << "[-schedule-path]"
                 << "[-autoexplore]"
                 << "[-autoexplore-timeout]"
                 << "[-pre-autoexplore-timeout]"
                 << "[-hidewindow]"
                 << "URL";
        exit(0);
    }

    int schedulePathIndex = args.indexOf("-schedule-path");
    if (schedulePathIndex != -1) {
        this->m_schedulePath = takeOptionValue(&args, schedulePathIndex);
    }

    int timeoutIndex = args.indexOf("-autoexplore-timeout");
    if (timeoutIndex != -1) {
        m_autoExploreTimout = takeOptionValue(&args, timeoutIndex).toInt();
    }

    int preTimeoutIndex = args.indexOf("-pre-autoexplore-timeout");
    if (preTimeoutIndex != -1) {
        m_autoExplorePreTimout = takeOptionValue(&args, preTimeoutIndex).toInt();
    }

    int autoexploreIndex = args.indexOf("-autoexplore");
    if (autoexploreIndex != -1) {
        m_autoExplore = true;
    }

    int windowIndex = args.indexOf("-hidewindow");
    if (windowIndex != -1) {
        m_showWindow = false;
    }

    int lastArg = args.lastIndexOf(QRegExp("^-.*"));
    QStringList urls = (lastArg != -1) ? args.mid(++lastArg) : args.mid(1);

    if (urls.length() == 0) {
        qDebug() << "URL required";
        exit(1);
        return;
    }

    m_url = urls.at(0);
}

void RecordClientApplication::slOnCloseEvent()
{
    if (!m_running) {
        return; // dont close twice
    }

    m_running = false;

    // happens before

    ActionLogSave();
    ActionLogStrictMode(false);

    // schedule

    std::ofstream schedulefile;
    schedulefile.open(m_schedulePath.toStdString().c_str());

    WebCore::threadGlobalData().threadTimers().eventActionRegister()->dispatchHistory()->serialize(schedulefile);

    schedulefile.close();

    // network

    m_controllableFactory->writeNetworkFile();

    // log

    m_timeProvider->writeLogFile(m_logTimePath);
    m_randomProvider->writeLogFile(m_logRandomPath);

    // Write human readable HB relation dump (DEBUG)
    std::vector<ActionLog::Arc> arcs = ActionLogReportArcs();
    std::ofstream arcslog;
    arcslog.open("/tmp/arcs.log");

    for (std::vector<ActionLog::Arc>::iterator it = arcs.begin(); it != arcs.end(); ++it) {
        arcslog << (*it).m_tail << " -> " << (*it).m_head << std::endl;
    }

    arcslog.close();

    m_scheduler->stop();
    m_window->close();

    std::cout << "Recording finished" << std::endl;
}


int main(int argc, char **argv)
{
    RecordClientApplication app(argc, argv);

#ifndef NDEBUG
    int retVal = app.exec();
    QWebSettings::clearMemoryCaches();
    return retVal;
#else
    return app.exec();
#endif
}

#include "main.moc"
