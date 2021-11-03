/****************************************************************************
Copyright © 2020 Adrien Faveraux <ad1rie3@hotmail.fr>

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
#pragma once

#include <QObject>

#include <Wrapland/Server/wraplandserver_export.h>

#include <memory>

namespace Wrapland::Server
{
class Display;
class Client;
class Seat;
class Surface;

enum class text_input_v2_content_hint : uint32_t {
    none = 0,
    completion = 1 << 0,
    correction = 1 << 1,
    capitalization = 1 << 2,
    lowercase = 1 << 3,
    uppercase = 1 << 4,
    titlecase = 1 << 5,
    hidden_text = 1 << 6,
    sensitive_data = 1 << 7,
    latin = 1 << 8,
    multiline = 1 << 9,
};
Q_DECLARE_FLAGS(text_input_v2_content_hints, text_input_v2_content_hint)

enum class text_input_v2_content_purpose : uint32_t {
    normal,
    alpha,
    digits,
    number,
    phone,
    url,
    email,
    name,
    password,
    date,
    time,
    datetime,
    terminal,
};

class WRAPLANDSERVER_EXPORT text_input_manager_v2 : public QObject
{
    Q_OBJECT
public:
    explicit text_input_manager_v2(Display* display);
    ~text_input_manager_v2() override;

private:
    class Private;
    std::unique_ptr<Private> d_ptr;
};

class WRAPLANDSERVER_EXPORT text_input_v2 : public QObject
{
    Q_OBJECT
public:
    Client* client() const;
    QByteArray preferredLanguage() const;
    QRect cursorRectangle() const;
    text_input_v2_content_purpose contentPurpose() const;
    text_input_v2_content_hints contentHints() const;
    QByteArray surroundingText() const;
    qint32 surroundingTextCursorPosition() const;
    qint32 surroundingTextSelectionAnchor() const;
    QPointer<Surface> surface() const;
    bool isEnabled() const;

    void preEdit(const QByteArray& text, const QByteArray& commitText);
    void commit(const QByteArray& text);
    void setPreEditCursor(qint32 index);
    void deleteSurroundingText(quint32 beforeLength, quint32 afterLength);
    void setCursorPosition(qint32 index, qint32 anchor);
    void setTextDirection(Qt::LayoutDirection direction);

    void keysymPressed(quint32 keysym, Qt::KeyboardModifiers modifiers = Qt::NoModifier);
    void keysymReleased(quint32 keysym, Qt::KeyboardModifiers modifiers = Qt::NoModifier);
    void setInputPanelState(bool visible, const QRect& overlappedSurfaceArea);
    void setLanguage(const QByteArray& languageTag);

Q_SIGNALS:
    void requestShowInputPanel();
    void requestHideInputPanel();
    void requestReset();
    void preferredLanguageChanged(const QByteArray& language);
    void cursorRectangleChanged(const QRect& rect);
    void contentTypeChanged();
    void surroundingTextChanged();
    void enabledChanged();
    void resourceDestroyed();

private:
    explicit text_input_v2(Client* client, uint32_t version, uint32_t id);
    friend class text_input_manager_v2;
    friend class Seat;
    friend class text_input_pool;

    class Private;
    Private* d_ptr;
};

}

Q_DECLARE_METATYPE(Wrapland::Server::text_input_v2*)
Q_DECLARE_METATYPE(Wrapland::Server::text_input_v2_content_hint)
Q_DECLARE_METATYPE(Wrapland::Server::text_input_v2_content_hints)
Q_DECLARE_OPERATORS_FOR_FLAGS(Wrapland::Server::text_input_v2_content_hints)
Q_DECLARE_METATYPE(Wrapland::Server::text_input_v2_content_purpose)
