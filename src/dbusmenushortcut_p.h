/* This file is part of the dbusmenu-qt library
   Copyright 2009 Canonical
   Author: Aurelien Gateau <aurelien.gateau@canonical.com>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License (LGPL) as published by the Free Software Foundation;
   either version 2 of the License, or (at your option) any later
   version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/
#ifndef DBUSMENUSHORTCUT_H
#define DBUSMENUSHORTCUT_H

// Qt
#include <QtCore/QMetaType>
#include <QtCore/QStringList>

// Local
#include <dbusmenu_export.h>


class QKeySequence;
class QDBusArgument;

class DBUSMENU_EXPORT DBusMenuShortcut : public QList<QStringList>
{
public:
    QKeySequence toKeySequence() const;
    static DBusMenuShortcut fromKeySequence(const QKeySequence&);
};

Q_DECLARE_METATYPE(DBusMenuShortcut)

DBUSMENU_EXPORT QDBusArgument &operator<<(QDBusArgument &argument, const DBusMenuShortcut &);
DBUSMENU_EXPORT const QDBusArgument &operator>>(const QDBusArgument &argument, DBusMenuShortcut &);

#endif /* DBUSMENUSHORTCUT_H */
