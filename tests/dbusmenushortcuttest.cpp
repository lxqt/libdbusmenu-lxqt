/* This file is part of the dbusmenu-qt library
   Copyright 2010 Canonical
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
// Self
#include "dbusmenushortcuttest.h"

// Qt
#include <QtTest>
#include <QByteArray>
#include <QLatin1StringView>

// DBusMenuLXQt
#include <dbusmenushortcut_p.h>
#include <debug_p.h>

using namespace Qt::Literals::StringLiterals;

QTEST_MAIN(DBusMenuShortcutTest)

namespace QTest
{
template<>
char *toString(const DBusMenuShortcut &dmShortcut)
{
    QByteArray ba = "DBusMenuShortcut("_ba;
    for(const QStringList& tokens : dmShortcut) {
        ba += "(_"_ba;
        ba += tokens.join("+"_L1).toUtf8();
        ba += ")"_ba;
    }
    ba += ")"_ba;
    return qstrdup(ba.data());
}
}

DBusMenuShortcut createKeyList(const QString& txt)
{
    DBusMenuShortcut lst;
    const QStringList tokens = txt.split(u',');
    for (const QString& token : tokens) {
        lst << token.split(u'+');
    }
    return lst;
}

#define ADD_ROW(ksArgs, klArgs) QTest::newRow(#ksArgs) << QKeySequence ksArgs << createKeyList(klArgs)

void DBusMenuShortcutTest::testConverter_data()
{
    QTest::addColumn<QKeySequence>("keySequence");
    QTest::addColumn<DBusMenuShortcut>("keyList");

    ADD_ROW((Qt::ALT | Qt::Key_F4), "Alt+F4"_L1);
    ADD_ROW((Qt::CTRL | Qt::Key_S), "Control+S"_L1);
    ADD_ROW((Qt::CTRL | Qt::Key_X, Qt::ALT | Qt::SHIFT | Qt::Key_Q), "Control+X,Alt+Shift+Q"_L1);
    ADD_ROW((Qt::META | Qt::Key_E), "Super+E"_L1);
    ADD_ROW((Qt::CTRL | Qt::Key_Plus), "Control+plus"_L1);
    ADD_ROW((Qt::CTRL | Qt::Key_Minus), "Control+minus"_L1);
}

void DBusMenuShortcutTest::testConverter()
{
    QFETCH(QKeySequence, keySequence);
    QFETCH(DBusMenuShortcut, keyList);

    DBusMenuShortcut list = DBusMenuShortcut::fromKeySequence(keySequence);
    QCOMPARE(list, keyList);
    QKeySequence sequence = keyList.toKeySequence();
    QCOMPARE(sequence.toString(), keySequence.toString());
}

