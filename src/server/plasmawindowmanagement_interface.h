/********************************************************************
Copyright 2015  Martin Gräßlin <mgraesslin@kde.org>

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
#ifndef WAYLAND_SERVER_PLASMA_WINDOW_MANAGEMENT_INTERFACE_H
#define WAYLAND_SERVER_PLASMA_WINDOW_MANAGEMENT_INTERFACE_H

#include <QObject>

#include <Wrapland/Server/wraplandserver_export.h>

#include "global.h"
#include "resource.h"

class QSize;

namespace Wrapland
{
namespace Server
{

class Display;
class PlasmaWindowInterface;
class Surface;
class PlasmaVirtualDesktopManagementInterface;

/**
 * @todo Add documentation
 */
class WRAPLANDSERVER_EXPORT PlasmaWindowManagementInterface : public Global
{
    Q_OBJECT
public:
    virtual ~PlasmaWindowManagementInterface();
    enum class ShowingDesktopState {
        Disabled,
        Enabled
    };
    void setShowingDesktopState(ShowingDesktopState state);

    PlasmaWindowInterface *createWindow(QObject *parent);
    QList<PlasmaWindowInterface*> windows() const;

    /**
     * Unmaps the @p window previously created with {@link createWindow}.
     * The window will be unmapped and removed from the list of {@link windows}.
     *
     * Unmapping a @p window indicates to the client that it should destroy the
     * resource created for the window. Once all resources for the @p window are
     * destroyed, the @p window will get deleted automatically. There is no need
     * to manually delete the @p window. A manual delete will trigger the unmap
     * and resource destroy at the same time and can result in protocol errors on
     * client side if it still accesses the resource before receiving the unmap event.
     *
     * @see createWindow
     * @see windows
     * @since 0.0.523
     **/
    void unmapWindow(PlasmaWindowInterface *window);

    /**
     * Associate a PlasmaVirtualDesktopManagementInterface to this window management.
     * It's necessary to associate one in orderto use the plasma virtual desktop features
     * of PlasmaWindowInterface, as a window must know what are the deasktops available
     * @since 0.0.548
     */
    void setPlasmaVirtualDesktopManagementInterface(PlasmaVirtualDesktopManagementInterface *manager);

    /**
     * @returns the PlasmaVirtualDesktopManagementInterface associated to this PlasmaWindowManagementInterface
     * @since 0.0.548
     */
    PlasmaVirtualDesktopManagementInterface *plasmaVirtualDesktopManagementInterface() const;

Q_SIGNALS:
    void requestChangeShowingDesktop(ShowingDesktopState requestedState);

private:
    friend class Display;
    explicit PlasmaWindowManagementInterface(Display *display, QObject *parent);
    class Private;
    Private *d_func() const;
};

/**
 * @todo Add documentation
 */
class WRAPLANDSERVER_EXPORT PlasmaWindowInterface : public QObject
{
    Q_OBJECT
public:
    virtual ~PlasmaWindowInterface();

    void setTitle(const QString &title);
    void setAppId(const QString &appId);
    void setPid(quint32 pid);
    void setActive(bool set);
    void setMinimized(bool set);
    void setMaximized(bool set);
    void setFullscreen(bool set);
    void setKeepAbove(bool set);
    void setKeepBelow(bool set);
    void setOnAllDesktops(bool set);
    void setDemandsAttention(bool set);
    void setCloseable(bool set);
    void setMinimizeable(bool set);
    void setMaximizeable(bool set);
    void setFullscreenable(bool set);
    void setSkipTaskbar(bool skip);
    void setSkipSwitcher(bool skip);
    /**
     * @since 0.0.522
     */
    void setShadeable(bool set);
    /**
     * @since 0.0.522
     */
    void setShaded(bool set);
    /**
     * @since 0.0.522
     */
    void setMovable(bool set);
    /**
     * @since 0.0.522
     */
    void setResizable(bool set);
    /**
     * FIXME: still relevant with new desktops?
     * @since 0.0.522
     */
    void setVirtualDesktopChangeable(bool set);

    /**
     * This method removes the Window and the Client is supposed to release the resource
     * bound for this Window. Once all resources are released the Window gets deleted.
     *
     * Prefer using {@link PlasmaWindowManagementInterface::unmapWindow}.
     * @see PlasmaWindowManagementInterface::unmapWindow
     **/
    void unmap();

    /**
     * @returns Geometries of the taskbar entries, indicized by the
     *          surface of the panels
     * @since 5.5
     */
    QHash<Surface*, QRect> minimizedGeometries() const;

    /**
     * Sets this PlasmaWindowInterface as a transient window to @p parentWindow.
     * If @p parentWindow is @c nullptr, the PlasmaWindowInterface is a toplevel
     * window and does not have a parent window.
     * @since 0.0.524
     **/
    void setParentWindow(PlasmaWindowInterface *parentWindow);

    /**
     * Sets the window @p geometry of this PlasmaWindow.
     *
     * @param geometry The geometry in absolute coordinates
     * @since 0.0.525
     **/
    void setGeometry(const QRect &geometry);

    /**
     * Set the icon of the PlasmaWindowInterface.
     *
     * In case the icon has a themed name, only the name is sent to the client.
     * Otherwise the client is only informed that there is an icon and the client
     * can request the icon in an asynchronous way by passing a file descriptor
     * into which the icon will be serialized.
     *
     * @param icon The new icon
     * @since 0.0.528
     **/
    void setIcon(const QIcon &icon);

    /**
     * Adds a new desktop to this window: a window can be on
     * an arbitrary subset of virtual desktops.
     * If it's on none it will be considered on all desktops.
     *
     * @since 0.0.548
     */
    void addPlasmaVirtualDesktop(const QString &id);

    /**
     * Removes a visrtual desktop from a window
     *
     * @since 0.0.548
     */
    void removePlasmaVirtualDesktop(const QString &id);

    /**
     * The ids of all the desktops currently associated with this window.
     * When a desktop is deleted it will be automatically removed from this list
     *
     * @since 0.0.548
     */
    QStringList plasmaVirtualDesktops() const;

Q_SIGNALS:
    void closeRequested();
    /**
     * @since 0.0.522
     */
    void moveRequested();
    /**
     * @since 0.0.522
     */
    void resizeRequested();
    void activeRequested(bool set);
    void minimizedRequested(bool set);
    void maximizedRequested(bool set);
    void fullscreenRequested(bool set);
    void keepAboveRequested(bool set);
    void keepBelowRequested(bool set);
    void demandsAttentionRequested(bool set);
    void closeableRequested(bool set);
    void minimizeableRequested(bool set);
    void maximizeableRequested(bool set);
    void fullscreenableRequested(bool set);
    void skipTaskbarRequested(bool set);
    void skipSwitcherRequested(bool set);
    QRect minimizedGeometriesChanged();
    /**
     * @since 0.0.522
     */
    void shadeableRequested(bool set);
    /**
     * @since 0.0.522
     */
    void shadedRequested(bool set);
    /**
     * @since 0.0.522
     */
    void movableRequested(bool set);
    /**
     * @since 0.0.522
     */
    void resizableRequested(bool set);
    /**
     * FIXME: still relevant with new virtual desktops?
     * @since 0.0.522
     */
    void virtualDesktopChangeableRequested(bool set);

    /**
     * Emitted when the client wishes this window to enter in a new virtual desktop.
     * The server will decide whether to consent this request
     * @since 0.0.552
     */
    void enterPlasmaVirtualDesktopRequested(const QString &desktop);

    /**
     * Emitted when the client wishes this window to enter in
     * a new virtual desktop to be created for it.
     * The server will decide whether to consent this request
     * @since 0.0.552
     */
    void enterNewPlasmaVirtualDesktopRequested();

    /**
     * Emitted when the client wishes to remove this window from a virtual desktop.
     * The server will decide whether to consent this request
     * @since 0.0.552
     */
    void leavePlasmaVirtualDesktopRequested(const QString &desktop);

private:
    friend class PlasmaWindowManagementInterface;
    explicit PlasmaWindowInterface(PlasmaWindowManagementInterface *wm, QObject *parent);

    class Private;
    const std::unique_ptr<Private> d;
};

}
}

Q_DECLARE_METATYPE(Wrapland::Server::PlasmaWindowManagementInterface::ShowingDesktopState)

#endif
