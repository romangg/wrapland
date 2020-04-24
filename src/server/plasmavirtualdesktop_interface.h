/****************************************************************************
Copyright 2018  Marco Martin <notmart@gmail.com>

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
****************************************************************************/
#ifndef WRAPLAND_SERVER_PLASMAVIRTUALDESKTOP_H
#define WRAPLAND_SERVER_PLASMAVIRTUALDESKTOP_H

#include "global.h"
#include "resource.h"

#include <Wrapland/Server/wraplandserver_export.h>

#include <memory>

namespace Wrapland
{
namespace Server
{

class Display;
class PlasmaVirtualDesktopInterface;

/**
 * @short Wrapper for the org_kde_plasma_virtual_desktop_management interface.
 *
 * This class provides a convenient wrapper for the org_kde_plasma_virtual_desktop_management interface.
 * @since 0.0.552
 */
class WRAPLANDSERVER_EXPORT PlasmaVirtualDesktopManagementInterface : public Global
{
    Q_OBJECT
public:
    virtual ~PlasmaVirtualDesktopManagementInterface();

    /**
     * Sets how many rows the virtual desktops should be laid into
     * @since 0.0.555
     */
    void setRows(quint32 rows);

    /**
     * @returns A desktop identified uniquely by this id.
     * If not found, nullptr will be returned.
     * @see createDesktop
     */
    PlasmaVirtualDesktopInterface *desktop(const QString &id);

    /**
     * @returns A desktop identified uniquely by this id, if not found
     * a new desktop will be created for this id at a given position.
     * @param id the id for the desktop
     * @param position the position the desktop will be in, if not provided,
     *                 it will be appended at the end. If the desktop was already
     *                 existing, position is ignored.
     */
    PlasmaVirtualDesktopInterface *createDesktop(const QString &id, quint32 position = std::numeric_limits<uint32_t>::max());

    /**
     * Removed and destroys the desktop identified by id, if present
     */
    void removeDesktop(const QString &id);

    /**
     * @returns All tghe desktops present.
     */
    QList <PlasmaVirtualDesktopInterface *> desktops() const;

    /**
     * Inform the clients that all the properties have been sent, and
     * their client-side representation is complete.
     */
    void sendDone();

Q_SIGNALS:
    /**
     * A desktop has been activated
     */
    void desktopActivated(const QString &id);

    /**
     * The client asked to remove a desktop, It's responsibility of the server
     * deciding whether to remove it or not.
     */
    void desktopRemoveRequested(const QString &id);

    /**
     * The client asked to create a desktop, It's responsibility of the server
     * deciding whether to create it or not.
     * @param name The desired user readable name
     * @param position The desired position. Normalized, guaranteed to be in the range 0-count
     */
    void desktopCreateRequested(const QString &name, quint32 position);

private:
    explicit PlasmaVirtualDesktopManagementInterface(Display *display, QObject *parent = nullptr);
    friend class Display;
    class Private;
    Private *d_func() const;
};

class WRAPLANDSERVER_EXPORT PlasmaVirtualDesktopInterface : public QObject
{
    Q_OBJECT
public:
    virtual ~PlasmaVirtualDesktopInterface();

    /**
     * @returns the unique id for this desktop.
     * ids are set at creation time by PlasmaVirtualDesktopManagementInterface::createDesktop
     * and can't be changed at runtime.
     */
    QString id() const;

    /**
     * Sets a new name for this desktop
     */
    void setName(const QString &name);

    /**
     * @returns the name for this desktop
     */
    QString name() const;

    /**
     * Set this desktop as active or not.
     * It's the compositor responsibility to manage the mutual exclusivity of active desktops.
     */
    void setActive(bool active);

    /**
     * @returns true if this desktop is active. Only one at a time will be.
     */
    bool isActive() const;

    /**
     * Inform the clients that all the properties have been sent, and
     * their client-side representation is complete.
     */
    void sendDone();

Q_SIGNALS:
    /**
     * Emitted when the client asked to activate this desktop:
     * it's the decision of the server whether to perform the activation or not.
     */
    void activateRequested();

private:
    explicit PlasmaVirtualDesktopInterface(PlasmaVirtualDesktopManagementInterface *parent);
    friend class PlasmaVirtualDesktopManagementInterface;

    class Private;
    const std::unique_ptr<Private> d;
};

}
}

#endif
