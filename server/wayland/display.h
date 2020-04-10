/********************************************************************
Copyright © 2020 Roman Gilg <subdiff@gmail.com>

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
#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct wl_client;
struct wl_display;
struct wl_event_loop;
struct wl_global;

class QObject;

namespace Wrapland
{
namespace Server
{
class Client;
class D_isplay;
class G_lobal;

namespace Wayland
{
class Client;

class Display
{
public:
    explicit Display(Wrapland::Server::D_isplay* parent);
    virtual ~Display();

    void setSocketName(const std::string& name);

    void addGlobal(Server::G_lobal* global);
    void removeGlobal(Server::G_lobal* global);

    wl_display* display() const;
    std::string socketName() const;

    void start(bool createSocket);
    void terminate();

    void startLoop();

    void flush();

    void dispatchEvents(int msecTimeout = -1);
    void dispatch();

    bool running() const;
    void setRunning(bool running);
    void installSocketNotifier(QObject* parent);

    wl_client* createClient(int fd);
    Client* getClient(wl_client* wlClient);

    void setupClient(Client* client);

    std::vector<Client*> const& clients() const;

    Server::D_isplay* handle() const;

    static Display* backendCast(Server::D_isplay* display);

private:
    static std::vector<Display*> s_displays;

    void addSocket();
    wl_display* m_display = nullptr;
    wl_event_loop* m_loop = nullptr;
    std::string m_socketName{"wayland-0"};

    bool m_running = false;

    std::vector<Server::G_lobal*> m_globals;
    std::vector<Client*> m_clients;

    Server::D_isplay* m_handle;
};

}
}
}
