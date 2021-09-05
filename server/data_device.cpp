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
#include "data_device.h"

#include "data_device_manager.h"
#include "data_offer_p.h"
#include "data_source.h"
#include "data_source_p.h"
#include "seat.h"
#include "selection_p.h"
#include "surface.h"
#include "surface_p.h"

#include "wayland/resource.h"

#include <wayland-server.h>

namespace Wrapland::Server
{

class data_device::Private : public Wayland::Resource<data_device>
{
public:
    Private(Client* client, uint32_t version, uint32_t id, Seat* seat, data_device* q);
    ~Private() override;

    data_offer* createDataOffer(data_source* source);
    void cancel_drag_target();
    void update_drag_motion();
    void update_drag_pointer_motion();
    void update_drag_touch_motion();
    void update_drag_target_offer(Surface* surface, uint32_t serial);

    Seat* seat;
    data_source* source = nullptr;
    Surface* surface = nullptr;
    Surface* icon = nullptr;

    data_source* selection = nullptr;
    QMetaObject::Connection selectionDestroyedConnection;

    struct Drag {
        Surface* surface = nullptr;
        QMetaObject::Connection destroyConnection;
        QMetaObject::Connection posConnection;
        QMetaObject::Connection sourceActionConnection;
        QMetaObject::Connection targetActionConnection;
        quint32 serial = 0;
    };
    Drag drag;

    QPointer<Surface> proxyRemoteSurface;

private:
    static void startDragCallback(wl_client* wlClient,
                                  wl_resource* wlResource,
                                  wl_resource* wlSource,
                                  wl_resource* wlOrigin,
                                  wl_resource* wlIcon,
                                  uint32_t serial);
    static void set_selection_callback(wl_client* wlClient,
                                       wl_resource* wlResource,
                                       wl_resource* wlSource,
                                       uint32_t id);

    void startDrag(data_source* dataSource, Surface* origin, Surface* icon, quint32 serial);

    static const struct wl_data_device_interface s_interface;

    data_device* q_ptr;
};

const struct wl_data_device_interface data_device::Private::s_interface = {
    startDragCallback,
    set_selection_callback,
    destroyCallback,
};

data_device::Private::Private(Client* client,
                              uint32_t version,
                              uint32_t id,
                              Seat* seat,
                              data_device* q)
    : Wayland::Resource<data_device>(client,
                                     version,
                                     id,
                                     &wl_data_device_interface,
                                     &s_interface,
                                     q)
    , seat(seat)
    , q_ptr{q}
{
}

data_device::Private::~Private() = default;

void data_device::Private::startDragCallback([[maybe_unused]] wl_client* wlClient,
                                             wl_resource* wlResource,
                                             wl_resource* wlSource,
                                             wl_resource* wlOrigin,
                                             wl_resource* wlIcon,
                                             uint32_t serial)
{
    auto priv = handle(wlResource)->d_ptr;
    auto source = wlSource ? Resource<data_source>::handle(wlSource) : nullptr;
    auto origin = Resource<Surface>::handle(wlOrigin);
    auto icon = wlIcon ? Resource<Surface>::handle(wlIcon) : nullptr;

    priv->startDrag(source, origin, icon, serial);
}

void data_device::Private::startDrag(data_source* dataSource,
                                     Surface* origin,
                                     Surface* _icon,
                                     quint32 serial)
{
    // TODO(unknown author): verify serial

    auto focusSurface = origin;

    if (proxyRemoteSurface) {
        // origin is a proxy surface
        focusSurface = proxyRemoteSurface.data();
    }

    auto pointerGrab = false;
    if (seat->hasPointer()) {
        auto& pointers = seat->pointers();
        pointerGrab
            = pointers.has_implicit_grab(serial) && pointers.get_focus().surface == focusSurface;
    }

    if (!pointerGrab) {
        // Client doesn't have pointer grab.
        if (!seat->hasTouch()) {
            // Client has no pointer grab and no touch capability.
            return;
        }
        auto& touches = seat->touches();
        if (!touches.has_implicit_grab(serial) || touches.get_focus().surface != focusSurface) {
            // Client neither has pointer nor touch grab. No drag start allowed.
            return;
        }
    }

    // Source is allowed to be null, handled client internally.
    source = dataSource;
    if (dataSource) {
        QObject::connect(
            dataSource, &data_source::resourceDestroyed, q_ptr, [this] { source = nullptr; });
    }

    surface = origin;
    icon = _icon;
    drag.serial = serial;
    Q_EMIT q_ptr->drag_started();
}

void data_device::Private::set_selection_callback(wl_client* /*wlClient*/,
                                                  wl_resource* wlResource,
                                                  wl_resource* wlSource,
                                                  uint32_t /*id*/)
{
    // TODO(unknown author): verify serial
    auto handle = Resource::handle(wlResource);
    auto source = wlSource ? Wayland::Resource<data_source>::handle(wlSource) : nullptr;

    // TODO(romangg): move errors into Wayland namespace.
    if (source && source->supported_dnd_actions()
        && wl_resource_get_version(wlSource) >= WL_DATA_SOURCE_ACTION_SINCE_VERSION) {
        wl_resource_post_error(
            wlSource, WL_DATA_SOURCE_ERROR_INVALID_SOURCE, "Data source is for drag and drop");
        return;
    }

    set_selection(handle, handle->d_ptr, wlSource);
}

data_offer* data_device::Private::createDataOffer(data_source* source)
{
    if (!source) {
        // A data offer can only exist together with a source.
        return nullptr;
    }

    auto offer = new data_offer(client()->handle(), version(), source);

    if (!offer->d_ptr->resource()) {
        // TODO(unknown author): send error?
        delete offer;
        return nullptr;
    }

    send<wl_data_device_send_data_offer>(offer->d_ptr->resource());
    offer->send_all_offers();
    return offer;
}

void data_device::Private::cancel_drag_target()
{
    if (!drag.surface) {
        return;
    }
    if (resource() && drag.surface->resource()) {
        send<wl_data_device_send_leave>();
    }
    if (drag.posConnection) {
        disconnect(drag.posConnection);
        drag.posConnection = QMetaObject::Connection();
    }
    disconnect(drag.destroyConnection);
    drag.destroyConnection = QMetaObject::Connection();
    drag.surface = nullptr;
    if (drag.sourceActionConnection) {
        disconnect(drag.sourceActionConnection);
        drag.sourceActionConnection = QMetaObject::Connection();
    }
    if (drag.targetActionConnection) {
        disconnect(drag.targetActionConnection);
        drag.targetActionConnection = QMetaObject::Connection();
    }
    // don't update serial, we need it
}

void data_device::Private::update_drag_motion()
{
    if (seat->drags().is_pointer_drag()) {
        update_drag_pointer_motion();
    } else if (seat->drags().is_touch_drag()) {
        update_drag_touch_motion();
    }
}

void data_device::Private::update_drag_pointer_motion()
{
    assert(seat->drags().is_pointer_drag());
    drag.posConnection = connect(seat, &Seat::pointerPosChanged, handle(), [this] {
        auto const pos
            = seat->drags().get_target().transformation.map(seat->pointers().get_position());
        send<wl_data_device_send_motion>(
            seat->timestamp(), wl_fixed_from_double(pos.x()), wl_fixed_from_double(pos.y()));
        client()->flush();
    });
}

void data_device::Private::update_drag_touch_motion()
{
    assert(seat->drags().is_touch_drag());

    drag.posConnection = connect(
        seat, &Seat::touchMoved, handle(), [this](auto id, auto serial, auto globalPosition) {
            Q_UNUSED(id);
            if (serial != drag.serial) {
                // different touch down has been moved
                return;
            }
            auto const pos = seat->drags().get_target().transformation.map(globalPosition);
            send<wl_data_device_send_motion>(
                seat->timestamp(), wl_fixed_from_double(pos.x()), wl_fixed_from_double(pos.y()));
            client()->flush();
        });
}

void data_device::Private::update_drag_target_offer(Surface* surface, uint32_t serial)
{
    auto source = seat->drags().get_source().dev->drag_source();
    auto offer = createDataOffer(source);

    // TODO(unknown author): handle touch position
    auto const pos = seat->drags().get_target().transformation.map(seat->pointers().get_position());
    send<wl_data_device_send_enter>(serial,
                                    surface->d_ptr->resource(),
                                    wl_fixed_from_double(pos.x()),
                                    wl_fixed_from_double(pos.y()),
                                    offer ? offer->d_ptr->resource() : nullptr);

    if (!offer) {
        // No new offer.
        return;
    }

    offer->d_ptr->sendSourceActions();

    auto matchOffers = [source, offer] {
        dnd_action action{dnd_action::none};

        if (source->supported_dnd_actions().testFlag(offer->preferred_dnd_action())) {
            action = offer->preferred_dnd_action();

        } else {
            auto const source_actions = source->supported_dnd_actions();
            auto const offer_actions = offer->supported_dnd_actions();

            if (source_actions.testFlag(dnd_action::copy)
                && offer_actions.testFlag(dnd_action::copy)) {
                action = dnd_action::copy;
            } else if (source_actions.testFlag(dnd_action::move)
                       && offer_actions.testFlag(dnd_action::move)) {
                action = dnd_action::move;
            } else if (source_actions.testFlag(dnd_action::ask)
                       && offer_actions.testFlag(dnd_action::ask)) {
                action = dnd_action::ask;
            }
        }

        offer->send_action(action);
        source->send_action(action);
    };
    drag.targetActionConnection
        = connect(offer, &data_offer::dnd_actions_changed, offer, matchOffers);
    drag.sourceActionConnection
        = connect(source, &data_source::supported_dnd_actions_changed, source, matchOffers);
}

data_device::data_device(Client* client, uint32_t version, uint32_t id, Seat* seat)
    : d_ptr(new Private(client, version, id, seat, this))
{
}

Seat* data_device::seat() const
{
    return d_ptr->seat;
}

data_source* data_device::drag_source() const
{

    return d_ptr->source;
}

Surface* data_device::icon() const
{
    return d_ptr->icon;
}

Surface* data_device::origin() const
{
    return d_ptr->proxyRemoteSurface ? d_ptr->proxyRemoteSurface.data() : d_ptr->surface;
}

data_source* data_device::selection() const
{
    return d_ptr->selection;
}

void data_device::send_selection(data_source* source)
{
    if (!source) {
        send_clear_selection();
        return;
    }

    auto offer = d_ptr->createDataOffer(source);
    if (!offer) {
        return;
    }

    d_ptr->send<wl_data_device_send_selection>(offer->d_ptr->resource());
}

void data_device::send_clear_selection()
{
    d_ptr->send<wl_data_device_send_selection>(nullptr);
}

void data_device::drop()
{
    d_ptr->send<wl_data_device_send_drop>();

    if (d_ptr->drag.posConnection) {
        disconnect(d_ptr->drag.posConnection);
        d_ptr->drag.posConnection = QMetaObject::Connection();
    }

    disconnect(d_ptr->drag.destroyConnection);
    d_ptr->drag.destroyConnection = QMetaObject::Connection();
    d_ptr->drag.surface = nullptr;

    // TODO(romangg): do we need to flush the client here?
}

void data_device::update_drag_target(Surface* surface, quint32 serial)
{
    d_ptr->cancel_drag_target();

    if (!surface) {
        if (auto s = d_ptr->seat->drags().get_source().dev->drag_source()) {
            s->send_action(dnd_action::none);
        }
        return;
    }
    if (d_ptr->proxyRemoteSurface && d_ptr->proxyRemoteSurface == surface) {
        // A proxy can not have the remote surface as target. All other surfaces even of itself
        // are fine. Such surfaces get data offers from themselves while a drag is ongoing.
        return;
    }

    d_ptr->update_drag_motion();

    d_ptr->drag.surface = surface;
    d_ptr->drag.destroyConnection = connect(surface, &Surface::resourceDestroyed, this, [this] {
        if (d_ptr->resource()) {
            d_ptr->send<wl_data_device_send_leave>();
        }
        if (d_ptr->drag.posConnection) {
            disconnect(d_ptr->drag.posConnection);
        }
        d_ptr->drag = Private::Drag();
    });

    d_ptr->update_drag_target_offer(surface, serial);
    d_ptr->client()->flush();
}

quint32 data_device::drag_implicit_grab_serial() const
{
    return d_ptr->drag.serial;
}

void data_device::update_proxy(Surface* remote)
{
    // TODO(romangg): connect destroy signal?
    d_ptr->proxyRemoteSurface = remote;
}

Client* data_device::client() const
{
    return d_ptr->client()->handle();
}

}
