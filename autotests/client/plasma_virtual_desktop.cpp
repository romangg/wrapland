/********************************************************************
Copyright 2018  Marco Martin <mart@kde.org>

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) version 3, or any
later version accepted by the membership of KDE e.V. (or its
successor approved by the membership of KDE e.V.), which shall
act as a proxy defined in Section 6 of version 3 of the license.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
#include <QtTest>

#include "../../src/client/compositor.h"
#include "../../src/client/connection_thread.h"
#include "../../src/client/event_queue.h"
#include "../../src/client/plasmavirtualdesktop.h"
#include "../../src/client/plasmawindowmanagement.h"
#include "../../src/client/region.h"
#include "../../src/client/registry.h"
#include "../../src/client/surface.h"

#include "../../server/compositor.h"
#include "../../server/display.h"
#include "../../server/region.h"

#include "../../server/plasma_virtual_desktop.h"
#include "../../server/plasma_window.h"

class TestVirtualDesktop : public QObject
{
    Q_OBJECT
public:
    explicit TestVirtualDesktop(QObject* parent = nullptr);
private Q_SLOTS:
    void init();
    void cleanup();

    void testCreate();
    void testSetRows();
    void testConnectNewClient();
    void testDestroy();
    void testActivate();

    void testEnterLeaveDesktop();
    void testAllDesktops();
    void testCreateRequested();
    void testRemoveRequested();

private:
    Wrapland::Server::Display* m_display;
    Wrapland::Server::Compositor* m_serverCompositor;
    Wrapland::Server::PlasmaVirtualDesktopManager* m_serverPlasmaVirtualDesktopManager;
    Wrapland::Server::PlasmaWindowManager* m_serverWindowManager;
    Wrapland::Server::PlasmaWindow* m_windowInterface;

    Wrapland::Client::ConnectionThread* m_connection;
    Wrapland::Client::Compositor* m_compositor;
    Wrapland::Client::PlasmaVirtualDesktopManagement* m_plasmaVirtualDesktopManagement;
    Wrapland::Client::EventQueue* m_queue;
    Wrapland::Client::PlasmaWindowManagement* m_windowManagement;
    Wrapland::Client::PlasmaWindow* m_window;

    QThread* m_thread;
};

constexpr auto socket_name{"wrapland-test-wayland-virtual-desktop-0"};

TestVirtualDesktop::TestVirtualDesktop(QObject* parent)
    : QObject(parent)
    , m_display(nullptr)
    , m_serverCompositor(nullptr)
    , m_connection(nullptr)
    , m_compositor(nullptr)
    , m_queue(nullptr)
    , m_thread(nullptr)
{
    qRegisterMetaType<uint32_t>();
}

void TestVirtualDesktop::init()
{
    m_display = new Wrapland::Server::Display(this);
    m_display->set_socket_name(socket_name);
    m_display->start();

    // setup connection
    m_connection = new Wrapland::Client::ConnectionThread;
    QSignalSpy connectedSpy(m_connection, &Wrapland::Client::ConnectionThread::establishedChanged);
    QVERIFY(connectedSpy.isValid());
    m_connection->setSocketName(socket_name);

    m_thread = new QThread(this);
    m_connection->moveToThread(m_thread);
    m_thread->start();

    m_connection->establishConnection();
    QVERIFY(connectedSpy.count() || connectedSpy.wait());
    QCOMPARE(connectedSpy.count(), 1);

    m_queue = new Wrapland::Client::EventQueue(this);
    QVERIFY(!m_queue->isValid());
    m_queue->setup(m_connection);
    QVERIFY(m_queue->isValid());

    Wrapland::Client::Registry registry;
    QSignalSpy compositorSpy(&registry, &Wrapland::Client::Registry::compositorAnnounced);
    QVERIFY(compositorSpy.isValid());

    QSignalSpy plasmaVirtualDesktopManagementSpy(
        &registry, &Wrapland::Client::Registry::plasmaVirtualDesktopManagementAnnounced);
    QVERIFY(plasmaVirtualDesktopManagementSpy.isValid());

    QSignalSpy windowManagementSpy(&registry,
                                   SIGNAL(plasmaWindowManagementAnnounced(quint32, quint32)));
    QVERIFY(windowManagementSpy.isValid());

    QVERIFY(!registry.eventQueue());
    registry.setEventQueue(m_queue);
    QCOMPARE(registry.eventQueue(), m_queue);
    registry.create(m_connection->display());
    QVERIFY(registry.isValid());
    registry.setup();

    m_serverCompositor = m_display->createCompositor(m_display);

    QVERIFY(compositorSpy.wait());
    m_compositor = registry.createCompositor(compositorSpy.first().first().value<quint32>(),
                                             compositorSpy.first().last().value<quint32>(),
                                             this);

    m_serverPlasmaVirtualDesktopManager = m_display->createPlasmaVirtualDesktopManager(m_display);

    QVERIFY(plasmaVirtualDesktopManagementSpy.wait());
    m_plasmaVirtualDesktopManagement = registry.createPlasmaVirtualDesktopManagement(
        plasmaVirtualDesktopManagementSpy.first().first().value<quint32>(),
        plasmaVirtualDesktopManagementSpy.first().last().value<quint32>(),
        this);

    m_serverWindowManager = m_display->createPlasmaWindowManager(m_display);
    m_serverWindowManager->setVirtualDesktopManager(m_serverPlasmaVirtualDesktopManager);

    QVERIFY(windowManagementSpy.wait());
    m_windowManagement = registry.createPlasmaWindowManagement(
        windowManagementSpy.first().first().value<quint32>(),
        windowManagementSpy.first().last().value<quint32>(),
        this);

    QSignalSpy windowSpy(m_windowManagement,
                         SIGNAL(windowCreated(Wrapland::Client::PlasmaWindow*)));
    QVERIFY(windowSpy.isValid());
    m_windowInterface = m_serverWindowManager->createWindow(this);
    m_windowInterface->setPid(1337);

    QVERIFY(windowSpy.wait());
    m_window = windowSpy.first().first().value<Wrapland::Client::PlasmaWindow*>();
}

void TestVirtualDesktop::cleanup()
{
#define CLEANUP(variable)                                                                          \
    if (variable) {                                                                                \
        delete variable;                                                                           \
        variable = nullptr;                                                                        \
    }
    CLEANUP(m_compositor)
    CLEANUP(m_plasmaVirtualDesktopManagement)
    CLEANUP(m_windowInterface)
    CLEANUP(m_windowManagement)
    CLEANUP(m_queue)
    if (m_connection) {
        m_connection->deleteLater();
        m_connection = nullptr;
    }
    if (m_thread) {
        m_thread->quit();
        m_thread->wait();
        delete m_thread;
        m_thread = nullptr;
    }
    CLEANUP(m_serverCompositor)
    CLEANUP(m_serverPlasmaVirtualDesktopManager)
    CLEANUP(m_serverWindowManager)
    CLEANUP(m_display)
#undef CLEANUP
}

void TestVirtualDesktop::testCreate()
{
    QSignalSpy desktopCreatedSpy(m_plasmaVirtualDesktopManagement,
                                 &Wrapland::Client::PlasmaVirtualDesktopManagement::desktopCreated);
    QSignalSpy managementDoneSpy(m_plasmaVirtualDesktopManagement,
                                 &Wrapland::Client::PlasmaVirtualDesktopManagement::done);

    // on this createDesktop bind() isn't called already, the desktopadded signals will be sent
    // after bind happened
    auto serverDesktop1 = m_serverPlasmaVirtualDesktopManager->createDesktop(QStringLiteral("0-1"));
    serverDesktop1->setName("Desktop 1");

    desktopCreatedSpy.wait();
    QList<QVariant> arguments = desktopCreatedSpy.takeFirst();
    QCOMPARE(arguments.at(0).toString(), QStringLiteral("0-1"));
    QCOMPARE(arguments.at(1).toUInt(), (quint32)0);
    m_serverPlasmaVirtualDesktopManager->sendDone();
    managementDoneSpy.wait();

    auto desktop1 = m_plasmaVirtualDesktopManagement->desktops().first();
    QSignalSpy desktop1DoneSpy(desktop1, &Wrapland::Client::PlasmaVirtualDesktop::done);
    QVERIFY(desktop1DoneSpy.isValid());

    serverDesktop1->sendDone();
    desktop1DoneSpy.wait();

    QCOMPARE(desktop1->id(), QStringLiteral("0-1"));
    QCOMPARE(desktop1->name(), QStringLiteral("Desktop 1"));

    // on those createDesktop the bind will already be done
    auto serverDesktop2 = m_serverPlasmaVirtualDesktopManager->createDesktop(QStringLiteral("0-2"));
    serverDesktop2->setName("Desktop 2");
    desktopCreatedSpy.wait();
    arguments = desktopCreatedSpy.takeFirst();
    QCOMPARE(arguments.at(0).toString(), QStringLiteral("0-2"));
    QCOMPARE(arguments.at(1).toUInt(), (quint32)1);
    QCOMPARE(m_plasmaVirtualDesktopManagement->desktops().length(), 2);

    Wrapland::Server::PlasmaVirtualDesktop* desktop3Int
        = m_serverPlasmaVirtualDesktopManager->createDesktop(QStringLiteral("0-3"));
    desktop3Int->setName("Desktop 3");
    desktopCreatedSpy.wait();
    arguments = desktopCreatedSpy.takeFirst();
    QCOMPARE(arguments.at(0).toString(), QStringLiteral("0-3"));
    QCOMPARE(m_plasmaVirtualDesktopManagement->desktops().length(), 3);

    m_serverPlasmaVirtualDesktopManager->sendDone();
    managementDoneSpy.wait();

    // get the clients
    Wrapland::Client::PlasmaVirtualDesktop* desktop2
        = m_plasmaVirtualDesktopManagement->desktops()[1];
    QSignalSpy desktop2DoneSpy(desktop2, &Wrapland::Client::PlasmaVirtualDesktop::done);
    serverDesktop2->sendDone();
    desktop2DoneSpy.wait();

    Wrapland::Client::PlasmaVirtualDesktop* desktop3
        = m_plasmaVirtualDesktopManagement->desktops()[2];
    QSignalSpy desktop3DoneSpy(desktop3, &Wrapland::Client::PlasmaVirtualDesktop::done);
    desktop3Int->sendDone();
    desktop3DoneSpy.wait();

    QCOMPARE(desktop1->id(), QStringLiteral("0-1"));
    QCOMPARE(desktop1->name(), QStringLiteral("Desktop 1"));

    QCOMPARE(desktop2->id(), QStringLiteral("0-2"));
    QCOMPARE(desktop2->name(), QStringLiteral("Desktop 2"));

    QCOMPARE(desktop3->id(), QStringLiteral("0-3"));
    QCOMPARE(desktop3->name(), QStringLiteral("Desktop 3"));

    // coherence of order between client and server
    QCOMPARE(m_serverPlasmaVirtualDesktopManager->desktops().length(), 3);
    QCOMPARE(m_plasmaVirtualDesktopManagement->desktops().length(), 3);

    for (int i = 0; i < m_plasmaVirtualDesktopManagement->desktops().length(); ++i) {
        QCOMPARE(m_serverPlasmaVirtualDesktopManager->desktops().at(i)->id(),
                 m_plasmaVirtualDesktopManagement->desktops().at(i)->id());
    }
}

void TestVirtualDesktop::testSetRows()
{
    // rebuild some desktops
    testCreate();

    QSignalSpy rowsChangedSpy(m_plasmaVirtualDesktopManagement,
                              &Wrapland::Client::PlasmaVirtualDesktopManagement::rowsChanged);

    m_serverPlasmaVirtualDesktopManager->setRows(3);
    QVERIFY(rowsChangedSpy.wait());
    QCOMPARE(m_plasmaVirtualDesktopManagement->rows(), 3);
}

void TestVirtualDesktop::testConnectNewClient()
{
    // rebuild some desktops
    testCreate();

    Wrapland::Client::Registry registry;
    QVERIFY(!registry.eventQueue());
    registry.setEventQueue(m_queue);
    QCOMPARE(registry.eventQueue(), m_queue);
    registry.create(m_connection->display());
    QVERIFY(registry.isValid());
    registry.setup();

    QSignalSpy plasmaVirtualDesktopManagementSpy(
        &registry, &Wrapland::Client::Registry::plasmaVirtualDesktopManagementAnnounced);
    QVERIFY(plasmaVirtualDesktopManagementSpy.isValid());

    QVERIFY(plasmaVirtualDesktopManagementSpy.wait());

    Wrapland::Client::PlasmaVirtualDesktopManagement* otherPlasmaVirtualDesktopManagement
        = registry.createPlasmaVirtualDesktopManagement(
            plasmaVirtualDesktopManagementSpy.first().first().value<quint32>(),
            plasmaVirtualDesktopManagementSpy.first().last().value<quint32>(),
            this);

    QSignalSpy managementDoneSpy(otherPlasmaVirtualDesktopManagement,
                                 &Wrapland::Client::PlasmaVirtualDesktopManagement::done);

    managementDoneSpy.wait();
    QCOMPARE(otherPlasmaVirtualDesktopManagement->desktops().length(), 3);

    delete otherPlasmaVirtualDesktopManagement;
}

void TestVirtualDesktop::testDestroy()
{
    // rebuild some desktops
    testCreate();

    Wrapland::Server::PlasmaVirtualDesktop* serverDesktop1
        = m_serverPlasmaVirtualDesktopManager->desktops().first();
    Wrapland::Client::PlasmaVirtualDesktop* desktop1
        = m_plasmaVirtualDesktopManagement->desktops().first();

    QSignalSpy serverDesktop1DestroyedSpy(serverDesktop1, &QObject::destroyed);
    QSignalSpy desktop1DestroyedSpy(desktop1, &QObject::destroyed);
    QSignalSpy desktop1RemovedSpy(desktop1, &Wrapland::Client::PlasmaVirtualDesktop::removed);
    m_serverPlasmaVirtualDesktopManager->removeDesktop(QStringLiteral("0-1"));

    // test that both server and client desktop interfaces go away
    desktop1DestroyedSpy.wait();
    QTRY_COMPARE(serverDesktop1DestroyedSpy.count(), 1);
    QTRY_COMPARE(desktop1RemovedSpy.count(), 1);
    QTRY_COMPARE(desktop1DestroyedSpy.count(), 1);

    // coherence of order between client and server
    QCOMPARE(m_serverPlasmaVirtualDesktopManager->desktops().length(), 2);
    QCOMPARE(m_plasmaVirtualDesktopManagement->desktops().length(), 2);

    for (int i = 0; i < m_plasmaVirtualDesktopManagement->desktops().length(); ++i) {
        QCOMPARE(m_serverPlasmaVirtualDesktopManager->desktops().at(i)->id(),
                 m_plasmaVirtualDesktopManagement->desktops().at(i)->id());
    }

    // Test the desktopRemoved signal of the manager, remove another desktop as the signals can't be
    // tested at the same time
    QSignalSpy desktopManagerRemovedSpy(
        m_plasmaVirtualDesktopManagement,
        &Wrapland::Client::PlasmaVirtualDesktopManagement::desktopRemoved);
    m_serverPlasmaVirtualDesktopManager->removeDesktop(QStringLiteral("0-2"));
    desktopManagerRemovedSpy.wait();
    QCOMPARE(desktopManagerRemovedSpy.takeFirst().at(0).toString(), QStringLiteral("0-2"));

    QCOMPARE(m_serverPlasmaVirtualDesktopManager->desktops().length(), 1);
    QCOMPARE(m_plasmaVirtualDesktopManagement->desktops().length(), 1);
}

void TestVirtualDesktop::testActivate()
{
    // rebuild some desktops
    testCreate();

    Wrapland::Server::PlasmaVirtualDesktop* serverDesktop1
        = m_serverPlasmaVirtualDesktopManager->desktops().first();
    Wrapland::Client::PlasmaVirtualDesktop* desktop1
        = m_plasmaVirtualDesktopManagement->desktops().first();
    QVERIFY(desktop1->isActive());
    QVERIFY(serverDesktop1->active());

    Wrapland::Server::PlasmaVirtualDesktop* serverDesktop2
        = m_serverPlasmaVirtualDesktopManager->desktops()[1];
    Wrapland::Client::PlasmaVirtualDesktop* desktop2
        = m_plasmaVirtualDesktopManagement->desktops()[1];
    QVERIFY(!serverDesktop2->active());

    QSignalSpy requestActivateSpy(serverDesktop2,
                                  &Wrapland::Server::PlasmaVirtualDesktop::activateRequested);
    QSignalSpy activatedSpy(desktop2, &Wrapland::Client::PlasmaVirtualDesktop::activated);

    desktop2->requestActivate();
    requestActivateSpy.wait();

    // This simulates a compositor which supports only one active desktop at a time
    for (auto* deskInt : m_serverPlasmaVirtualDesktopManager->desktops()) {
        if (deskInt->id() == desktop2->id()) {
            deskInt->setActive(true);
        } else {
            deskInt->setActive(false);
        }
    }
    activatedSpy.wait();

    // correct state in the server
    QVERIFY(serverDesktop2->active());
    QVERIFY(!serverDesktop1->active());
    // correct state in the client
    QVERIFY(serverDesktop2->active());
    QVERIFY(!serverDesktop1->active());

    // Test the deactivated signal
    QSignalSpy deactivatedSpy(desktop2, &Wrapland::Client::PlasmaVirtualDesktop::deactivated);

    for (auto* deskInt : m_serverPlasmaVirtualDesktopManager->desktops()) {
        if (deskInt->id() == desktop1->id()) {
            deskInt->setActive(true);
        } else {
            deskInt->setActive(false);
        }
    }
    deactivatedSpy.wait();
}

void TestVirtualDesktop::testEnterLeaveDesktop()
{
    testCreate();

    QSignalSpy enterRequestedSpy(
        m_windowInterface, &Wrapland::Server::PlasmaWindow::enterPlasmaVirtualDesktopRequested);
    m_window->requestEnterVirtualDesktop(QStringLiteral("0-1"));
    enterRequestedSpy.wait();

    QCOMPARE(enterRequestedSpy.takeFirst().at(0).toString(), QStringLiteral("0-1"));

    QSignalSpy virtualDesktopEnteredSpy(
        m_window, &Wrapland::Client::PlasmaWindow::plasmaVirtualDesktopEntered);

    // agree to the request
    m_windowInterface->addPlasmaVirtualDesktop(QStringLiteral("0-1"));
    QCOMPARE(m_windowInterface->plasmaVirtualDesktops().length(), 1);
    QCOMPARE(m_windowInterface->plasmaVirtualDesktops().first(), QStringLiteral("0-1"));

    // check if the client received the enter
    virtualDesktopEnteredSpy.wait();
    QCOMPARE(virtualDesktopEnteredSpy.takeFirst().at(0).toString(), QStringLiteral("0-1"));
    QCOMPARE(m_window->plasmaVirtualDesktops().length(), 1);
    QCOMPARE(m_window->plasmaVirtualDesktops().first(), QStringLiteral("0-1"));

    // add another desktop, server side
    m_windowInterface->addPlasmaVirtualDesktop(QStringLiteral("0-3"));
    virtualDesktopEnteredSpy.wait();
    QCOMPARE(virtualDesktopEnteredSpy.takeFirst().at(0).toString(), QStringLiteral("0-3"));
    QCOMPARE(m_windowInterface->plasmaVirtualDesktops().length(), 2);
    QCOMPARE(m_window->plasmaVirtualDesktops().length(), 2);
    QCOMPARE(m_window->plasmaVirtualDesktops()[1], QStringLiteral("0-3"));

    // try to add an invalid desktop
    m_windowInterface->addPlasmaVirtualDesktop(QStringLiteral("invalid"));
    QCOMPARE(m_window->plasmaVirtualDesktops().length(), 2);

    // remove a desktop
    QSignalSpy leaveRequestedSpy(
        m_windowInterface, &Wrapland::Server::PlasmaWindow::leavePlasmaVirtualDesktopRequested);
    m_window->requestLeaveVirtualDesktop(QStringLiteral("0-1"));
    leaveRequestedSpy.wait();

    QCOMPARE(leaveRequestedSpy.takeFirst().at(0).toString(), QStringLiteral("0-1"));

    QSignalSpy virtualDesktopLeftSpy(m_window,
                                     &Wrapland::Client::PlasmaWindow::plasmaVirtualDesktopLeft);

    // agree to the request
    m_windowInterface->removePlasmaVirtualDesktop(QStringLiteral("0-1"));
    QCOMPARE(m_windowInterface->plasmaVirtualDesktops().length(), 1);
    QCOMPARE(m_windowInterface->plasmaVirtualDesktops().first(), QStringLiteral("0-3"));

    // check if the client received the leave
    virtualDesktopLeftSpy.wait();
    QCOMPARE(virtualDesktopLeftSpy.takeFirst().at(0).toString(), QStringLiteral("0-1"));
    QCOMPARE(m_window->plasmaVirtualDesktops().length(), 1);
    QCOMPARE(m_window->plasmaVirtualDesktops().first(), QStringLiteral("0-3"));

    // Destroy desktop 2
    m_serverPlasmaVirtualDesktopManager->removeDesktop(QStringLiteral("0-3"));
    // the window should receive a left signal from the destroyed desktop
    virtualDesktopLeftSpy.wait();

    QCOMPARE(m_window->plasmaVirtualDesktops().length(), 0);
}

void TestVirtualDesktop::testAllDesktops()
{
    testCreate();
    QSignalSpy virtualDesktopEnteredSpy(
        m_window, &Wrapland::Client::PlasmaWindow::plasmaVirtualDesktopEntered);
    QSignalSpy virtualDesktopLeftSpy(m_window,
                                     &Wrapland::Client::PlasmaWindow::plasmaVirtualDesktopLeft);

    // in the beginning the window is on desktop 1 and desktop 3
    m_windowInterface->addPlasmaVirtualDesktop(QStringLiteral("0-1"));
    m_windowInterface->addPlasmaVirtualDesktop(QStringLiteral("0-3"));
    virtualDesktopEnteredSpy.wait();

    // setting on all desktops
    QCOMPARE(m_window->plasmaVirtualDesktops().length(), 2);
    m_windowInterface->setOnAllDesktops(true);
    // setting on all desktops, the window will leave every desktop

    virtualDesktopLeftSpy.wait();
    QCOMPARE(virtualDesktopLeftSpy.count(), 2);
    QCOMPARE(m_window->plasmaVirtualDesktops().length(), 0);
    QVERIFY(m_window->isOnAllDesktops());

    QCOMPARE(m_window->plasmaVirtualDesktops().length(), 0);
    QVERIFY(m_window->isOnAllDesktops());

    // return to the active desktop (0-1)
    m_windowInterface->setOnAllDesktops(false);
    virtualDesktopEnteredSpy.wait();
    QCOMPARE(m_window->plasmaVirtualDesktops().length(), 1);
    QCOMPARE(m_windowInterface->plasmaVirtualDesktops().first(), QStringLiteral("0-1"));
    QVERIFY(!m_window->isOnAllDesktops());
}

void TestVirtualDesktop::testCreateRequested()
{
    // rebuild some desktops
    testCreate();

    QSignalSpy desktopCreateRequestedSpy(
        m_serverPlasmaVirtualDesktopManager,
        &Wrapland::Server::PlasmaVirtualDesktopManager::desktopCreateRequested);
    QSignalSpy desktopCreatedSpy(m_plasmaVirtualDesktopManagement,
                                 &Wrapland::Client::PlasmaVirtualDesktopManagement::desktopCreated);

    // listen for createdRequested
    m_plasmaVirtualDesktopManagement->requestCreateVirtualDesktop(QStringLiteral("Desktop"), 1);
    desktopCreateRequestedSpy.wait();
    QCOMPARE(desktopCreateRequestedSpy.first().first().toString(), QStringLiteral("Desktop"));
    QCOMPARE(desktopCreateRequestedSpy.first().at(1).value<uint32_t>(), (uint32_t)1);

    // actually create
    m_serverPlasmaVirtualDesktopManager->createDesktop(QStringLiteral("0-4"), 1);
    Wrapland::Server::PlasmaVirtualDesktop* desktopInt
        = m_serverPlasmaVirtualDesktopManager->desktops().at(1);

    QCOMPARE(desktopInt->id(), QStringLiteral("0-4"));
    desktopInt->setName(QStringLiteral("Desktop"));

    desktopCreatedSpy.wait();

    QCOMPARE(desktopCreatedSpy.first().first().toString(), QStringLiteral("0-4"));
    QCOMPARE(m_plasmaVirtualDesktopManagement->desktops().count(), 4);

    auto desktop = m_plasmaVirtualDesktopManagement->desktops().at(1);
    QSignalSpy desktopDoneSpy(desktop, &Wrapland::Client::PlasmaVirtualDesktop::done);
    desktopInt->sendDone();
    // desktopDoneSpy.wait();
    // check the order is correct
    QCOMPARE(m_plasmaVirtualDesktopManagement->desktops().at(0)->id(), QStringLiteral("0-1"));
    QCOMPARE(desktop->id(), QStringLiteral("0-4"));
    QCOMPARE(m_plasmaVirtualDesktopManagement->desktops().at(2)->id(), QStringLiteral("0-2"));
    QCOMPARE(m_plasmaVirtualDesktopManagement->desktops().at(3)->id(), QStringLiteral("0-3"));
}

void TestVirtualDesktop::testRemoveRequested()
{
    // rebuild some desktops
    testCreate();

    QSignalSpy desktopRemoveRequestedSpy(
        m_serverPlasmaVirtualDesktopManager,
        &Wrapland::Server::PlasmaVirtualDesktopManager::desktopRemoveRequested);

    // request a remove, just check the request arrived, ignore the request.
    m_plasmaVirtualDesktopManagement->requestRemoveVirtualDesktop(QStringLiteral("0-1"));
    desktopRemoveRequestedSpy.wait();
    QCOMPARE(desktopRemoveRequestedSpy.first().first().toString(), QStringLiteral("0-1"));
}

QTEST_GUILESS_MAIN(TestVirtualDesktop)
#include "plasma_virtual_desktop.moc"
