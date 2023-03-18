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
#include "wl_output_p.h"

#include "display.h"

#include "wayland/client.h"
#include "wayland/display.h"

#include <functional>
#include <wayland-server.h>

namespace Wrapland::Server
{

WlOutput::Private::Private(Output* output, Display* display, WlOutput* q_ptr)
    : WlOutputGlobal(q_ptr, display, &wl_output_interface, &s_interface)
    , displayHandle(display)
    , output(output)
{
    create();
}

const struct wl_output_interface WlOutput::Private::s_interface = {resourceDestroyCallback};

int32_t to_subpixel(output_subpixel subpixel)
{
    switch (subpixel) {
    case output_subpixel::unknown:
        return WL_OUTPUT_SUBPIXEL_UNKNOWN;
    case output_subpixel::none:
        return WL_OUTPUT_SUBPIXEL_NONE;
    case output_subpixel::horizontal_rgb:
        return WL_OUTPUT_SUBPIXEL_HORIZONTAL_RGB;
    case output_subpixel::horizontal_bgr:
        return WL_OUTPUT_SUBPIXEL_HORIZONTAL_BGR;
    case output_subpixel::vertical_rgb:
        return WL_OUTPUT_SUBPIXEL_VERTICAL_RGB;
    case output_subpixel::vertical_bgr:
        return WL_OUTPUT_SUBPIXEL_VERTICAL_BGR;
    }
    abort();
}

std::tuple<int32_t, int32_t, int32_t, int32_t, int32_t, char const*, char const*, int32_t>
WlOutput::Private::geometry_args(output_state const& state)
{
    auto const position = state.geometry.topLeft();

    return std::make_tuple(position.x(),
                           position.y(),
                           state.meta.physical_size.width(),
                           state.meta.physical_size.height(),
                           to_subpixel(state.subpixel),
                           state.meta.make.c_str(),
                           state.meta.model.c_str(),
                           Output::Private::to_transform(state.transform));
}

void WlOutput::Private::bindInit(WlOutputBind* bind)
{
    auto const state = output->d_ptr->published;

    send<wl_output_send_geometry>(bind, geometry_args(state));

    for (auto const& mode : output->d_ptr->modes) {
        if (mode != output->d_ptr->published.mode) {
            sendMode(bind, mode);
        }
    }
    sendMode(bind, output->d_ptr->published.mode);

    send<wl_output_send_scale, WL_OUTPUT_SCALE_SINCE_VERSION>(bind, state.client_scale);
    done(bind);
    bind->client->flush();
}

void WlOutput::Private::sendMode(WlOutputBind* bind, output_mode const& mode)
{
    // Only called on bind. In this case we want to send the currently published mode.
    auto flags = Output::Private::get_mode_flags(mode, output->d_ptr->published);

    send<wl_output_send_mode>(
        bind, flags, mode.size.width(), mode.size.height(), mode.refresh_rate);
}

void WlOutput::Private::sendMode(output_mode const& mode)
{
    // Only called on update. In this case we want to send the pending mode.
    auto flags = Output::Private::get_mode_flags(mode, output->d_ptr->pending);

    send<wl_output_send_mode>(flags, mode.size.width(), mode.size.height(), mode.refresh_rate);
}

bool WlOutput::Private::broadcast()
{
    auto const published = output->d_ptr->published;
    auto const pending = output->d_ptr->pending;

    bool changed = false;

    if (published.geometry.topLeft() != pending.geometry.topLeft()
        || published.meta.physical_size != pending.meta.physical_size
        || published.subpixel != pending.subpixel || published.meta.make != pending.meta.make
        || published.meta.model != pending.meta.model || published.transform != pending.transform) {
        send<wl_output_send_geometry>(geometry_args(pending));
        changed = true;
    }

    if (published.client_scale != pending.client_scale) {
        send<wl_output_send_scale, WL_OUTPUT_SCALE_SINCE_VERSION>(pending.client_scale);
        changed = true;
    }

    if (published.mode != pending.mode) {
        sendMode(pending.mode);
        changed = true;
    }

    return changed;
}

void WlOutput::Private::done()
{
    send<wl_output_send_done, WL_OUTPUT_DONE_SINCE_VERSION>();
}

void WlOutput::Private::done(WlOutputBind* bind)
{
    send<wl_output_send_done, WL_OUTPUT_DONE_SINCE_VERSION>(bind);
}

WlOutput::WlOutput(Output* output, Display* display)
    : QObject(nullptr)
    , d_ptr(new Private(output, display, this))
{
    display->add_wl_output(this);
}

WlOutput::~WlOutput()
{
    Q_EMIT removed();

    if (d_ptr->displayHandle) {
        d_ptr->displayHandle->removeOutput(this);
    }
}

Output* WlOutput::output() const
{
    return d_ptr->output;
}

}
