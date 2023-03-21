/*
    SPDX-FileCopyrightText: 2023 Roman Glig <subdiff@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only
*/
#include "wlr_output_head_v1_p.h"

#include "display.h"
#include "output_p.h"
#include "utils.h"
#include "wayland/global.h"
#include "wlr_output_mode_v1_p.h"

namespace Wrapland::Server
{

double estimate_scale(output_state const& data)
{
    auto const& mode_size = data.mode.size;
    auto const& logical_size = data.geometry.size();

    auto scale_x = mode_size.width() / logical_size.width();
    auto scale_y = mode_size.height() / logical_size.height();

    // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)
    return (scale_x + scale_y) / 2.;
}

wlr_output_head_v1::wlr_output_head_v1(Server::output& output, wlr_output_manager_v1& manager)
    : output{output}
    , manager{manager}
{
    manager.d_ptr->add_head(*this);
}

wlr_output_head_v1::~wlr_output_head_v1()
{
    for (auto&& res : resources) {
        res->d_ptr->send<zwlr_output_head_v1_send_finished>();
        res->d_ptr->head = nullptr;
    }

    manager.d_ptr->changed = true;
    remove_all(manager.d_ptr->heads, this);
}

wlr_output_head_v1_res* wlr_output_head_v1::add_bind(wlr_output_manager_v1_bind& bind)
{
    auto res = new wlr_output_head_v1_res(bind.client->handle, bind.version, *this);
    bind.send<zwlr_output_manager_v1_send_head>(res->d_ptr->resource);
    resources.push_back(res);

    res->send_static_data(output.d_ptr->pending.meta);

    for (auto&& mode : output.d_ptr->modes) {
        res->add_mode(*new wlr_output_mode_v1(bind.client->handle, bind.version, mode));
    }

    res->send_mutable_data(output.d_ptr->pending);

    return res;
}

void wlr_output_head_v1::broadcast()
{
    auto const published = output.d_ptr->published;
    auto const pending = output.d_ptr->pending;

    if (published.enabled != pending.enabled) {
        for (auto res : resources) {
            res->send_enabled(pending.enabled);
        }
        manager.d_ptr->changed = true;
    }

    if (!pending.enabled) {
        return;
    }

    // Force resending data when switching from disabled to enabled.
    if (published.mode != pending.mode || !published.enabled) {
        for (auto res : resources) {
            res->send_current_mode(pending.mode);
        }
        manager.d_ptr->changed = true;
    }

    if (published.geometry.topLeft() != pending.geometry.topLeft() || !published.enabled) {
        for (auto res : resources) {
            res->send_position(pending.geometry.topLeft().toPoint());
        }
        manager.d_ptr->changed = true;
    }

    if (published.transform != pending.transform || !published.enabled) {
        for (auto res : resources) {
            res->send_transform(pending.transform);
        }
        manager.d_ptr->changed = true;
    }

    if (auto scale = estimate_scale(pending); scale != current_scale || !published.enabled) {
        for (auto res : resources) {
            res->send_scale(scale);
        }
        current_scale = scale;
        manager.d_ptr->changed = true;
    }
}

wlr_output_head_v1_res::Private::Private(Client* client,
                                         uint32_t version,
                                         wlr_output_head_v1& head,
                                         wlr_output_head_v1_res& q_ptr)
    : Wayland::Resource<wlr_output_head_v1_res>(client,
                                                version,
                                                0,
                                                &zwlr_output_head_v1_interface,
                                                nullptr,
                                                &q_ptr)
    , head{&head}
{
}
wlr_output_head_v1_res::Private::~Private()
{
    if (head) {
        remove_all(head->resources, handle);
    }
}

wlr_output_head_v1_res::wlr_output_head_v1_res(Client* client,
                                               uint32_t version,
                                               wlr_output_head_v1& head)
    : d_ptr{new Private(client, version, head, *this)}
{
}

void wlr_output_head_v1_res::add_mode(wlr_output_mode_v1& mode) const
{
    d_ptr->modes.push_back(&mode);
    d_ptr->send<zwlr_output_head_v1_send_mode>(mode.d_ptr->resource);
    mode.d_ptr->send_data();
}

void wlr_output_head_v1_res::send_static_data(output_metadata const& data) const
{
    d_ptr->send<zwlr_output_head_v1_send_name>(data.name.c_str());
    d_ptr->send<zwlr_output_head_v1_send_description>(data.description.c_str());
    d_ptr->send<zwlr_output_head_v1_send_make>(data.make.c_str());
    d_ptr->send<zwlr_output_head_v1_send_model>(data.model.c_str());
    d_ptr->send<zwlr_output_head_v1_send_serial_number>(data.serial_number.c_str());
    if (auto const& size = data.physical_size; !size.isEmpty()) {
        d_ptr->send<zwlr_output_head_v1_send_physical_size>(size.width(), size.height());
    }
}

void wlr_output_head_v1_res::send_mutable_data(output_state const& data) const
{
    send_enabled(data.enabled);

    if (!data.enabled) {
        return;
    }

    send_current_mode(data.mode);
    send_position(data.geometry.topLeft().toPoint());
    send_transform(data.transform);
    send_scale(estimate_scale(data));
}

void wlr_output_head_v1_res::send_enabled(bool enabled) const
{
    d_ptr->send<zwlr_output_head_v1_send_enabled>(enabled);
}

void wlr_output_head_v1_res::send_current_mode(output_mode const& mode) const
{
    auto wlr_mode_it = std::find_if(d_ptr->modes.begin(), d_ptr->modes.end(), [&](auto wlr_mode) {
        return wlr_mode->d_ptr->mode == mode;
    });
    assert(wlr_mode_it != d_ptr->modes.end());
    d_ptr->send<zwlr_output_head_v1_send_current_mode>((*wlr_mode_it)->d_ptr->resource);
}

void wlr_output_head_v1_res::send_position(QPoint const& pos) const
{
    d_ptr->send<zwlr_output_head_v1_send_position>(pos.x(), pos.y());
}

void wlr_output_head_v1_res::send_transform(output_transform transform) const
{
    d_ptr->send<zwlr_output_head_v1_send_transform>(output_to_transform(transform));
}

void wlr_output_head_v1_res::send_scale(double scale) const
{
    d_ptr->send<zwlr_output_head_v1_send_scale>(wl_fixed_from_double(scale));
}

}
