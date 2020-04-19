/********************************************************************
Copyright 2016  Martin Gräßlin <mgraesslin@kde.org>

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
#include "testserver.h"

#include "../../../server/display.h"
#include "../../../server/compositor.h"
#include "../../../server/subcompositor.h"
#include "../../../server/output.h"
#include "../../../server/seat.h"
#include "../../../server/surface.h"

#include "../../server/idle_interface.h"
#include "../../server/fakeinput_interface.h"
#include "../../server/shell_interface.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QProcess>
#include <QTimer>
// system
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>

using namespace Wrapland::Server;

TestServer::TestServer(QObject *parent)
    : QObject(parent)
    , m_repaintTimer(new QTimer(this))
    , m_timeSinceStart(new QElapsedTimer)
    , m_cursorPos(QPointF(0, 0))
{
}

TestServer::~TestServer() = default;

void TestServer::init()
{
    Q_ASSERT(!m_display);
    m_display = new D_isplay(this);
    m_display->start(D_isplay::StartMode::ConnectClientsOnly);
    m_display->createShm();
    m_display->createCompositor();
    m_shell = m_display->createShell(m_display);
    connect(m_shell, &ShellInterface::surfaceCreated, this,
        [this] (ShellSurfaceInterface *surface) {
            m_shellSurfaces << surface;
            // TODO: pass keyboard/pointer/touch focus on mapped
            connect(surface, &QObject::destroyed, this,
                [this, surface] {
                    m_shellSurfaces.removeOne(surface);
                }
            );
        }
    );

    m_shell->create();
    m_seat = m_display->createSeat(m_display);
    m_seat->setHasKeyboard(true);
    m_seat->setHasPointer(true);
    m_seat->setHasTouch(true);

    m_display->createDataDeviceManager(m_display);
    m_display->createIdle(m_display)->create();
    m_display->createSubCompositor(m_display);

    auto output = m_display->createOutput(m_display);
    const QSize size(1280, 1024);
    output->setGlobalPosition(QPoint(0, 0));
    output->setPhysicalSize(size / 3.8);
    output->addMode(size);

    auto fakeInput = m_display->createFakeInput(m_display);
    fakeInput->create();
    connect(fakeInput, &FakeInputInterface::deviceCreated, this,
        [this] (FakeInputDevice *device) {
            device->setAuthentication(true);
            connect(device, &FakeInputDevice::pointerMotionRequested, this,
                [this] (const QSizeF &delta) {
                    m_seat->setTimestamp(m_timeSinceStart->elapsed());
                    m_cursorPos = m_cursorPos + QPointF(delta.width(), delta.height());
                    m_seat->setPointerPos(m_cursorPos);
                }
            );
            connect(device, &FakeInputDevice::pointerButtonPressRequested, this,
                [this] (quint32 button) {
                    m_seat->setTimestamp(m_timeSinceStart->elapsed());
                    m_seat->pointerButtonPressed(button);
                }
            );
            connect(device, &FakeInputDevice::pointerButtonReleaseRequested, this,
                [this] (quint32 button) {
                    m_seat->setTimestamp(m_timeSinceStart->elapsed());
                    m_seat->pointerButtonReleased(button);
                }
            );
            connect(device, &FakeInputDevice::pointerAxisRequested, this,
                [this] (Qt::Orientation orientation, qreal delta) {
                    m_seat->setTimestamp(m_timeSinceStart->elapsed());
                    m_seat->pointerAxis(orientation, delta);
                }
            );
            connect(device, &FakeInputDevice::touchDownRequested, this,
                [this] (quint32 id, const QPointF &pos) {
                    m_seat->setTimestamp(m_timeSinceStart->elapsed());
                    m_touchIdMapper.insert(id, m_seat->touchDown(pos));
                }
            );
            connect(device, &FakeInputDevice::touchMotionRequested, this,
                [this] (quint32 id, const QPointF &pos) {
                    m_seat->setTimestamp(m_timeSinceStart->elapsed());
                    const auto it = m_touchIdMapper.constFind(id);
                    if (it != m_touchIdMapper.constEnd()) {
                        m_seat->touchMove(it.value(), pos);
                    }
                }
            );
            connect(device, &FakeInputDevice::touchUpRequested, this,
                [this] (quint32 id) {
                    m_seat->setTimestamp(m_timeSinceStart->elapsed());
                    const auto it = m_touchIdMapper.find(id);
                    if (it != m_touchIdMapper.end()) {
                        m_seat->touchUp(it.value());
                        m_touchIdMapper.erase(it);
                    }
                }
            );
            connect(device, &FakeInputDevice::touchCancelRequested, this,
                [this] {
                    m_seat->setTimestamp(m_timeSinceStart->elapsed());
                    m_seat->cancelTouchSequence();
                }
            );
            connect(device, &FakeInputDevice::touchFrameRequested, this,
                [this] {
                    m_seat->setTimestamp(m_timeSinceStart->elapsed());
                    m_seat->touchFrame();
                }
            );
        }
    );

    m_repaintTimer->setInterval(1000 / 60);
    connect(m_repaintTimer, &QTimer::timeout, this, &TestServer::repaint);
    m_repaintTimer->start();
    m_timeSinceStart->start();
}

void TestServer::startTestApp(const QString &app, const QStringList &arguments)
{
    int sx[2];
    if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sx) < 0) {
        QCoreApplication::instance()->exit(1);
        return;
    }
    m_display->createClient(sx[0]);
    int socket = dup(sx[1]);
    if (socket == -1) {
        QCoreApplication::instance()->exit(1);
        return;
    }
    QProcess *p = new QProcess(this);
    p->setProcessChannelMode(QProcess::ForwardedChannels);
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    environment.insert(QStringLiteral("QT_QPA_PLATFORM"), QStringLiteral("wayland"));
    environment.insert(QStringLiteral("WAYLAND_SOCKET"), QString::fromUtf8(QByteArray::number(socket)));
    p->setProcessEnvironment(environment);
    auto finishedSignal = static_cast<void (QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished);
    connect(p, finishedSignal, QCoreApplication::instance(), &QCoreApplication::exit);
    connect(p, &QProcess::errorOccurred, this,
        [] {
            QCoreApplication::instance()->exit(1);
        }
    );
    p->start(app, arguments);
}

void TestServer::repaint()
{
    for (auto it = m_shellSurfaces.constBegin(), end = m_shellSurfaces.constEnd(); it != end; ++it) {
        (*it)->surface()->frameRendered(m_timeSinceStart->elapsed());
    }
}
