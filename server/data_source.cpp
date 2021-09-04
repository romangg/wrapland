/********************************************************************
Copyright © 2020, 2021 Roman Gilg <subdiff@gmail.com>

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
#include "data_source.h"
#include "data_source_p.h"

#include "data_control_v1_p.h"
#include "data_device_manager.h"
#include "selection_p.h"

#include "wayland/resource.h"

#include <unistd.h>

namespace Wrapland::Server
{

data_source::Private::Private(data_source* q_ptr)
    : q_ptr{q_ptr}
{
}

data_source::data_source()
    : d_ptr(new data_source::Private(this))
{
}

data_source_res_impl::data_source_res_impl(Client* client,
                                           uint32_t version,
                                           uint32_t id,
                                           data_source_res* q)
    : Wayland::Resource<data_source_res>(client,
                                         version,
                                         id,
                                         &wl_data_source_interface,
                                         &s_interface,
                                         q)
    , q_ptr{q}
{
}

const struct wl_data_source_interface data_source_res_impl::s_interface = {
    offer_callback,
    destroyCallback,
    setActionsCallback,
};

void data_source_res_impl::offer_callback(wl_client* /*wlClient*/,
                                          wl_resource* wlResource,
                                          char const* mimeType)
{
    auto handle = Resource::handle(wlResource);
    offer_mime_type(handle->src_priv(), mimeType);
}

void data_source_res_impl::setActionsCallback([[maybe_unused]] wl_client* wlClient,
                                              wl_resource* wlResource,
                                              uint32_t dnd_actions)
{
    Server::dnd_actions supportedActions;
    if (dnd_actions & WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY) {
        supportedActions |= dnd_action::copy;
    }
    if (dnd_actions & WL_DATA_DEVICE_MANAGER_DND_ACTION_MOVE) {
        supportedActions |= dnd_action::move;
    }
    if (dnd_actions & WL_DATA_DEVICE_MANAGER_DND_ACTION_ASK) {
        supportedActions |= dnd_action::ask;
    }
    // verify that the no other actions are sent
    if (dnd_actions
        & ~(WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY | WL_DATA_DEVICE_MANAGER_DND_ACTION_MOVE
            | WL_DATA_DEVICE_MANAGER_DND_ACTION_ASK)) {
        wl_resource_post_error(
            wlResource, WL_DATA_SOURCE_ERROR_INVALID_ACTION_MASK, "Invalid action mask");
        return;
    }

    auto src_priv = handle(wlResource)->src_priv();
    if (src_priv->supportedDnDActions != supportedActions) {
        src_priv->supportedDnDActions = supportedActions;
        Q_EMIT src_priv->q_ptr->supported_dnd_actions_changed();
    }
}

data_source_res::data_source_res(Client* client, uint32_t version, uint32_t id)
    : pub_src{new data_source}
    , impl{new data_source_res_impl(client, version, id, this)}
{
    QObject::connect(
        this, &data_source_res::resourceDestroyed, src(), &data_source::resourceDestroyed);
    src_priv()->res = this;

    if (version < WL_DATA_SOURCE_ACTION_SINCE_VERSION) {
        src_priv()->supportedDnDActions = dnd_action::copy;
    }
}

void data_source_res::accept(std::string const& mimeType) const
{
    // TODO(unknown author): does this require a sanity check on the possible mimeType?
    impl->send<wl_data_source_send_target>(mimeType.empty() ? nullptr : mimeType.c_str());
}

void data_source_res::request_data(std::string const& mimeType, int32_t fd) const
{
    // TODO(unknown author): does this require a sanity check on the possible mimeType?
    impl->send<wl_data_source_send_send>(mimeType.c_str(), fd);
    close(fd);
}

void data_source_res::cancel() const
{
    impl->send<wl_data_source_send_cancelled>();
    impl->client()->flush();
}

void data_source_res::send_dnd_drop_performed() const
{
    impl->send<wl_data_source_send_dnd_drop_performed,
               WL_DATA_SOURCE_DND_DROP_PERFORMED_SINCE_VERSION>();
}

void data_source_res::send_dnd_finished() const
{
    impl->send<wl_data_source_send_dnd_finished, WL_DATA_SOURCE_DND_FINISHED_SINCE_VERSION>();
}

void data_source_res::send_action(dnd_action action) const
{
    uint32_t wlAction = WL_DATA_DEVICE_MANAGER_DND_ACTION_NONE;

    if (action == dnd_action::copy) {
        wlAction = WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY;
    } else if (action == dnd_action::move) {
        wlAction = WL_DATA_DEVICE_MANAGER_DND_ACTION_MOVE;
    } else if (action == dnd_action::ask) {
        wlAction = WL_DATA_DEVICE_MANAGER_DND_ACTION_ASK;
    }

    impl->send<wl_data_source_send_action, WL_DATA_SOURCE_ACTION_SINCE_VERSION>(wlAction);
}

data_source* data_source_res::src() const
{
    return src_priv()->q_ptr;
}

data_source::Private* data_source_res::src_priv() const
{
    return pub_src->d_ptr.get();
}

void data_source::accept(std::string const& mimeType) const
{
    std::get<data_source_res*>(d_ptr->res)->accept(mimeType);
}

void data_source::request_data(std::string const& mimeType, int32_t fd) const
{
    std::visit([&](auto&& res) { res->request_data(mimeType, fd); }, d_ptr->res);
}

void data_source::cancel() const
{
    std::visit([](auto&& res) { res->cancel(); }, d_ptr->res);
}

dnd_actions data_source::supported_dnd_actions() const
{
    return d_ptr->supportedDnDActions;
}

void data_source::send_dnd_drop_performed() const
{
    std::get<data_source_res*>(d_ptr->res)->send_dnd_drop_performed();
}

void data_source::send_dnd_finished() const
{
    std::get<data_source_res*>(d_ptr->res)->send_dnd_finished();
}

void data_source::send_action(dnd_action action) const
{
    std::get<data_source_res*>(d_ptr->res)->send_action(action);
}

std::vector<std::string> data_source::mime_types() const
{
    return d_ptr->mimeTypes;
}

Client* data_source::client() const
{
    Client* cl{nullptr};
    std::visit([&](auto&& res) { cl = res->impl->client()->handle(); }, d_ptr->res);
    return cl;
}

}
