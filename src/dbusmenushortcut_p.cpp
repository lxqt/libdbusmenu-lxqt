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
#include "dbusmenushortcut_p.h"

// Qt
#include <QLatin1StringView>
#include <QtGui/QKeySequence>
#include <QtDBus/QDBusArgument>

// Local
#include "debug_p.h"

using namespace Qt::Literals::StringLiterals;

static const int QT_COLUMN = 0;
static const int DM_COLUMN = 1;

static void processKeyTokens(QStringList* tokens, int srcCol, int dstCol)
{
    struct Row {
        const char16_t* zero;
        const char16_t* one;
        const char16_t* operator[](int col) const { return col == 0 ? zero : one; }
    };
    static const Row table[] =
    { {u"Meta", u"Super"},
      {u"Ctrl", u"Control"},
      // Special cases for compatibility with libdbusmenu-glib which uses
      // "plus" for "+" and "minus" for "-".
      // cf https://bugs.launchpad.net/libdbusmenu-qt/+bug/712565
      {u"+", u"plus"},
      {u"-", u"minus"},
      {nullptr, nullptr}
    };

    const Row* ptr = table;
    for (; ptr->zero != 0; ++ptr) {
        const QStringView from = (*ptr)[srcCol];
        const QStringView to = (*ptr)[dstCol];
        tokens->replaceInStrings(from, to);
    }
}

DBusMenuShortcut DBusMenuShortcut::fromKeySequence(const QKeySequence& sequence)
{
    QString string = sequence.toString();
    DBusMenuShortcut shortcut;
    const QStringList tokens = string.split(", "_L1);
    for (QString token : tokens) {
        // Hack: Qt::CTRL | Qt::Key_Plus is turned into the string "Ctrl++",
        // but we don't want the call to token.split() to consider the
        // second '+' as a separator so we replace it with its final value.
        token.replace("++"_L1, "+plus"_L1);
        QStringList keyTokens = token.split(u'+');
        processKeyTokens(&keyTokens, QT_COLUMN, DM_COLUMN);
        shortcut << keyTokens;
    }
    return shortcut;
}

QKeySequence DBusMenuShortcut::toKeySequence() const
{
    QStringList tmp;
    for (const QStringList& keyTokens_ : std::as_const(*this)) {
        QStringList keyTokens = keyTokens_;
        processKeyTokens(&keyTokens, DM_COLUMN, QT_COLUMN);
        tmp << keyTokens.join("+"_L1);
    }
    QString string = tmp.join(", "_L1);
    return QKeySequence::fromString(string);
}

QDBusArgument &operator<<(QDBusArgument &argument, const DBusMenuShortcut &obj)
{
    int id = qMetaTypeId<QStringList>();
    argument.beginArray(id);
    typename DBusMenuShortcut::ConstIterator it = obj.constBegin();
    typename DBusMenuShortcut::ConstIterator end = obj.constEnd();
    for ( ; it != end; ++it)
        argument << *it;
    argument.endArray();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, DBusMenuShortcut &obj)
{
    argument.beginArray();
    obj.clear();
    while (!argument.atEnd()) {
        QStringList item;
        argument >> item;
        obj.push_back(item);
    }
    argument.endArray();

    return argument;
}
