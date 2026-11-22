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
#include "dbusmenuimporter.h"

// Qt
#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDBusVariant>
#include <QFont>
#include <QLatin1StringView>
#include <QMenu>
#include <QPointer>
#include <QSignalMapper>
#include <QTime>
#include <QTimer>
#include <QToolButton>
#include <QWidgetAction>
#include <QActionGroup>

// Local
#include "dbusmenutypes_p.h"
#include "dbusmenushortcut_p.h"
#include "debug_p.h"
#include "utils_p.h"

//#define BENCHMARK
#ifdef BENCHMARK
#include <QTime>
static QTime sChrono;
#endif

using namespace Qt::Literals::StringLiterals;

static constexpr QLatin1StringView DBUSMENU_INTERFACE = "com.canonical.dbusmenu"_L1;

static const int ABOUT_TO_SHOW_TIMEOUT = 3000;
static const int REFRESH_TIMEOUT = 4000;
static const int LAYOUT_UPDATE_TIMEOUT = 2000;

static const char *DBUSMENU_PROPERTY_ID = "_dbusmenu_id";
static const char *DBUSMENU_PROPERTY_ICON_NAME = "_dbusmenu_icon_name";
static const char *DBUSMENU_PROPERTY_ICON_DATA_HASH = "_dbusmenu_icon_data_hash";
static const char *DBUSMENU_PROPERTY_RESEND_CLICK = "_dbusmenu_resend_click";
static const char *DBUSMENU_PROPERTY_STALE = "_dbusmenu_stale";

static QAction *createKdeTitle(QAction *action, QWidget *parent)
{
    QToolButton *titleWidget = new QToolButton(0);
    QFont font = titleWidget->font();
    font.setBold(true);
    titleWidget->setFont(font);
    titleWidget->setIcon(action->icon());
    titleWidget->setText(action->text());
    titleWidget->setDown(true);
    titleWidget->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);

    QWidgetAction *titleAction = new QWidgetAction(parent);
    titleAction->setDefaultWidget(titleWidget);
    return titleAction;
}

Menu::Menu(QWidget *parent) : QMenu(parent), blockEvents(false) {}

bool Menu::event(QEvent *e) {
    if (blockEvents
        // These events are dangerous while actions are being replaced:
        && (e->type() == QEvent::Timer ||
            e->type() == QEvent::MouseMove ||
            e->type() == QEvent::MouseButtonPress ||
            e->type() == QEvent::MouseButtonRelease ||
            e->type() == QEvent::KeyPress ||
            e->type() == QEvent::KeyRelease ||
            e->type() == QEvent::Leave ||
            e->type() == QEvent::Enter)) {
        e->accept();
        return true;
    }
    return QMenu::event(e);
}

class DBusMenuImporterPrivate
{
public:
    DBusMenuImporter *q;

    QDBusAbstractInterface *m_interface;
    QMenu *m_menu;
    typedef QMap<int, QPointer<QAction> > ActionForId;
    ActionForId m_actionForId;
    QSignalMapper m_mapper;
    QTimer *m_pendingLayoutUpdateTimer;
    bool m_refreshing;

    QSet<int> m_idsRefreshedByAboutToShow;
    QSet<int> m_pendingLayoutUpdates;

    bool m_mustEmitMenuUpdated;

    DBusMenuImporterType m_type;

    // One level of the path of a clicked item: its index and label, and which
    // of the items with the same label it is, out of how many.
    struct ClickLevel {
        int index;
        QString text;
        int occurrence;
        int count;
    };

    static QList<QAction*> actionsWithText(const QList<QAction*> &acts, const QString &text)
    {
        QList<QAction*> result;
        for (QAction *act : acts) {
            if (act->text() == text) {
                result << act;
            }
        }
        return result;
    }

    static ClickLevel clickLevel(QMenu *menu, QAction *action)
    {
        const QList<QAction*> acts = menu->actions();
        const QList<QAction*> sameText = actionsWithText(acts, action->text());
        return {static_cast<int>(acts.indexOf(action)), action->text(),
                static_cast<int>(sameText.indexOf(action)), static_cast<int>(sameText.size())};
    }

    // The parent menus of the menus whose "aboutToShow()" is waiting for the
    // app (see "slotMenuAboutToShow()"). They must not be reloaded meanwhile.
    QList<QMenu*> m_lockedMenus;

    // The ids of the menus whose next fetch must resolve the pending click.
    QSet<int> m_resendClickIds;

    // The menus above "menu", up to the root menu. A submenu is always created
    // as a child of its parent menu (see "createAction()"), and its item may
    // have already been replaced by a layout update.
    QList<QMenu*> parentMenus(QMenu *menu) const
    {
        QList<QMenu*> result;
        while (menu && menu != m_menu) {
            menu = qobject_cast<QMenu*>(menu->parent());
            if (menu) {
                result << menu;
            }
        }
        return result;
    }

    // The item of "parent" that opens "subMenu", if any.
    static QAction *actionForMenu(QMenu *parent, QMenu *subMenu)
    {
        const auto acts = parent->actions();
        for (QAction *act : acts) {
            if (act->menu() == subMenu) {
                return act;
            }
        }
        return nullptr;
    }

    // The path (from the root menu) of a clicked item whose id may be stale.
    // It is resolved again, level by level, after fresh layouts are fetched,
    // and the click is sent to the item found.
    QList<ClickLevel> m_pendingClickPath;

    // The paths of the clicks made while another one is being resolved.
    QList<QList<ClickLevel>> m_queuedClickPaths;

    // Resolve "path" from a fresh root layout, or queue it after the click
    // that is being resolved.
    void resolveClick(const QList<ClickLevel> &path)
    {
        if (!m_pendingClickPath.isEmpty()) {
            m_queuedClickPaths << path;
            return;
        }
        m_pendingLayoutUpdates.remove(0);
        m_pendingClickPath = path;
        refresh(0)->setProperty(DBUSMENU_PROPERTY_RESEND_CLICK, true);
    }

    void resolveNextQueuedClick()
    {
        if (!m_queuedClickPaths.isEmpty()) {
            resolveClick(m_queuedClickPaths.takeFirst());
        }
    }

    // Called with the freshly loaded menu of the next level of the path.
    void resolvePendingClick(QMenu *menu)
    {
        if (m_pendingClickPath.isEmpty()) {
            return;
        }
        const ClickLevel level = m_pendingClickPath.takeFirst();
        const QList<QAction*> acts = menu ? menu->actions() : QList<QAction*>();
        QAction *target = nullptr;
        if (level.index >= 0 && level.index < acts.size()
            && acts.at(level.index)->text() == level.text) {
            target = acts.at(level.index);
        } else { // the item has moved: find it among the items with its label
            const QList<QAction*> sameText = actionsWithText(acts, level.text);
            // if an item with this label was added or removed, it is unknown
            // which one is the clicked item
            if (sameText.size() == level.count) {
                target = sameText.at(level.occurrence);
            }
        }
        if (!target || !target->isEnabled()
            || (!m_pendingClickPath.isEmpty() && !target->menu())) {
            DMWARNING << "Clicked item not found after a layout update:" << level.text;
            m_pendingClickPath.clear();
            resolveNextQueuedClick();
            return;
        }
        const int id = target->property(DBUSMENU_PROPERTY_ID).toInt();
        if (m_pendingClickPath.isEmpty()) {
            sendEvent(id, "clicked"_L1);
            resolveNextQueuedClick();
        } else { // submenus are empty or stale until they are fetched
            refresh(id)->setProperty(DBUSMENU_PROPERTY_RESEND_CLICK, true);
        }
    }

    QDBusPendingCallWatcher *refresh(int id)
    {
        #ifdef BENCHMARK
        DMDEBUG << "Starting refresh chrono for id" << id;
        sChrono.start();
        #endif

        m_refreshing = true;

        QDBusPendingCall call = m_interface->asyncCall("GetLayout"_L1, id, 1, QStringList());
        QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(call, q);
        watcher->setProperty(DBUSMENU_PROPERTY_ID, id);
        if (m_resendClickIds.remove(id)) {
            watcher->setProperty(DBUSMENU_PROPERTY_RESEND_CLICK, true);
        }
        QObject::connect(watcher, SIGNAL(finished(QDBusPendingCallWatcher*)),
            q, SLOT(slotGetLayoutFinished(QDBusPendingCallWatcher*)));

        return watcher;
    }

    QMenu *createMenu(QWidget *parent)
    {
        QMenu *menu = q->createMenu(parent);
        QObject::connect(menu, SIGNAL(aboutToShow()),
            q, SLOT(slotMenuAboutToShow()));
        QObject::connect(menu, SIGNAL(aboutToHide()),
            q, SLOT(slotMenuAboutToHide()));
        return menu;
    }

    /**
     * Init all the immutable action properties here
     * TODO: Document immutable properties?
     *
     * Note: we remove properties we handle from the map (using QMap::take()
     * instead of QMap::value()) to avoid warnings about these properties in
     * updateAction()
     */
    QAction *createAction(int id, const QVariantMap &_map, QWidget *parent)
    {
        QVariantMap map = _map;
        QAction *action = new QAction(parent);
        action->setProperty(DBUSMENU_PROPERTY_ID, id);

        QString type = map.take("type"_L1).toString();
        if (type == "separator"_L1) {
            action->setSeparator(true);
        }

        if (map.take("children-display"_L1).toString() == "submenu"_L1) {
            QMenu *menu = createMenu(parent);
            action->setMenu(menu);
        }

        QString toggleType = map.take("toggle-type"_L1).toString();
        if (!toggleType.isEmpty()) {
            action->setCheckable(true);
            if (toggleType == "radio"_L1) {
                QActionGroup *group = new QActionGroup(action);
                group->addAction(action);
            }
        }

        bool isKdeTitle = map.take("x-kde-title"_L1).toBool();
        updateAction(action, map, map.keys());

        if (isKdeTitle) {
            action = createKdeTitle(action, parent);
        }

        return action;
    }

    // Whether "action" can show an item with these properties: only the
    // properties handled by "createAction()" cannot be changed afterwards.
    static bool canReuseAction(QAction *action, const QVariantMap &map)
    {
        if (qobject_cast<QWidgetAction*>(action) || map.value("x-kde-title"_L1).toBool()) {
            return false;
        }
        const QString toggleType = map.value("toggle-type"_L1).toString();
        return action->isSeparator() == (map.value("type"_L1).toString() == "separator"_L1)
            && (action->menu() != nullptr) == (map.value("children-display"_L1).toString() == "submenu"_L1)
            && action->isCheckable() == !toggleType.isEmpty()
            && (action->actionGroup() != nullptr) == (toggleType == "radio"_L1);
    }

    // Give an existing action the id and properties of a new item.
    void reuseAction(QAction *action, int id, const QVariantMap &_map)
    {
        const int oldId = action->property(DBUSMENU_PROPERTY_ID).toInt();
        if (oldId != id) {
            if (m_actionForId.value(oldId) == action) {
                m_actionForId.remove(oldId);
            }
            m_actionForId.insert(id, action);
            action->setProperty(DBUSMENU_PROPERTY_ID, id);
            m_mapper.setMapping(action, id);
        }

        QVariantMap map = _map;
        for (const auto key : {"type"_L1, "children-display"_L1, "toggle-type"_L1, "x-kde-title"_L1}) {
            map.remove(key);
        }

        // Recompute the icon as "createAction()" does only if its source changes.
        const QString iconName = map.value("icon-name"_L1).toString();
        const bool hasIconData = map.contains("icon-data"_L1);
        const QVariant iconDataHash = action->property(DBUSMENU_PROPERTY_ICON_DATA_HASH);
        if (iconName != action->property(DBUSMENU_PROPERTY_ICON_NAME).toString()
            || hasIconData != iconDataHash.isValid()
            || (hasIconData && uint(qHash(map.value("icon-data"_L1).toByteArray())) != iconDataHash.toUInt())) {
            action->setProperty(DBUSMENU_PROPERTY_ICON_NAME, QVariant());
            action->setProperty(DBUSMENU_PROPERTY_ICON_DATA_HASH, QVariant());
            if (iconName.isEmpty() && !hasIconData) {
                action->setIcon(QIcon());
            }
        }

        // The properties that the item does not have get their default values,
        // like in a new action.
        for (const auto key : {"label"_L1, "enabled"_L1, "visible"_L1}) {
            if (!map.contains(key)) {
                updateActionProperty(action, key, QVariant());
            }
        }
        if (!map.contains("shortcut"_L1)) {
            action->setShortcut(QKeySequence());
        }
        if (!map.contains("toggle-state"_L1) && action->isCheckable()) {
            action->setChecked(false);
        }

        updateAction(action, map, map.keys());

        // The items of a submenu are fetched again before it is shown, or now
        // if it is visible.
        if (QMenu *subMenu = action->menu()) {
            subMenu->setProperty(DBUSMENU_PROPERTY_STALE, true);
            if (subMenu->isVisible()) {
                m_pendingLayoutUpdates << id;
            }
        }
    }

    /**
     * Update mutable properties of an action. A property may be listed in
     * requestedProperties but not in map, this means we should use the default value
     * for this property.
     *
     * @param action the action to update
     * @param map holds the property values
     * @param requestedProperties which properties has been requested
     */
    void updateAction(QAction *action, const QVariantMap &map, const QStringList &requestedProperties)
    {
        for (const QString &key : requestedProperties) {
            updateActionProperty(action, key, map.value(key));
        }
    }

    void updateActionProperty(QAction *action, const QString &key, const QVariant &value)
    {
        if (key == "label"_L1) {
            updateActionLabel(action, value);
        } else if (key == "enabled"_L1) {
            updateActionEnabled(action, value);
        } else if (key == "toggle-state"_L1) {
            updateActionChecked(action, value);
        } else if (key == "icon-name"_L1) {
            updateActionIconByName(action, value);
        } else if (key == "icon-data"_L1) {
            updateActionIconByData(action, value);
        } else if (key == "visible"_L1) {
            updateActionVisible(action, value);
        } else if (key == "shortcut"_L1) {
            updateActionShortcut(action, value);
        } else if (key == "children-display"_L1) {
        } else {
            DMWARNING << "Unhandled property update" << key;
        }
    }

    void updateActionLabel(QAction *action, const QVariant &value)
    {
        QString text = swapMnemonicChar(value.toString(), u'_', u'&');
        action->setText(text);
    }

    void updateActionEnabled(QAction *action, const QVariant &value)
    {
        action->setEnabled(value.isValid() ? value.toBool(): true);
    }

    void updateActionChecked(QAction *action, const QVariant &value)
    {
        if (action->isCheckable() && value.isValid()) {
            action->setChecked(value.toInt() == 1);
        }
    }

    void updateActionIconByName(QAction *action, const QVariant &value)
    {
        QString iconName = value.toString();
        QString previous = action->property(DBUSMENU_PROPERTY_ICON_NAME).toString();
        if (previous == iconName) {
            return;
        }
        action->setProperty(DBUSMENU_PROPERTY_ICON_NAME, iconName);
        if (iconName.isEmpty()) {
            action->setIcon(QIcon());
            return;
        }
        action->setIcon(q->iconForName(iconName));
    }

    void updateActionIconByData(QAction *action, const QVariant &value)
    {
        QByteArray data = value.toByteArray();
        uint dataHash = qHash(data);
        uint previousDataHash = action->property(DBUSMENU_PROPERTY_ICON_DATA_HASH).toUInt();
        if (previousDataHash == dataHash) {
            return;
        }
        action->setProperty(DBUSMENU_PROPERTY_ICON_DATA_HASH, dataHash);
        QPixmap pix;
        if (!pix.loadFromData(data)) {
            DMWARNING << "Failed to decode icon-data property for action" << action->text();
            action->setIcon(QIcon());
            return;
        }
        action->setIcon(QIcon(pix));
    }

    void updateActionVisible(QAction *action, const QVariant &value)
    {
        action->setVisible(value.isValid() ? value.toBool() : true);
    }

    void updateActionShortcut(QAction *action, const QVariant &value)
    {
        QDBusArgument arg = value.value<QDBusArgument>();
        DBusMenuShortcut dmShortcut;
        arg >> dmShortcut;
        QKeySequence keySequence = dmShortcut.toKeySequence();
        action->setShortcut(keySequence);
    }

    QMenu *menuForId(int id) const
    {
        if (id == 0) {
            return q->menu();
        }
        QAction *action = m_actionForId.value(id);
        if (!action) {
            return 0;
        }
        return action->menu();
    }

    void slotItemsPropertiesUpdated(const DBusMenuItemList &updatedList, const DBusMenuItemKeysList &removedList);

    void sendEvent(int id, const QString &eventId)
    {
        QVariant empty = QVariant::fromValue(QDBusVariant(QString()));
        m_interface->asyncCall("Event"_L1, id, eventId, empty, 0u);
    }

    bool waitForWatcher(QDBusPendingCallWatcher * _watcher, int maxWait)
    {
        QPointer<QDBusPendingCallWatcher> watcher(_watcher);

        if(m_type == ASYNCHRONOUS) {
            QTimer timer;
            timer.setSingleShot(true);
            QEventLoop loop;
            loop.connect(&timer, SIGNAL(timeout()), SLOT(quit()));
            loop.connect(watcher, SIGNAL(finished(QDBusPendingCallWatcher *)), SLOT(quit()));
            timer.start(maxWait);
            // This runs inside "QMenu::popup()": user input could hide the menu
            // that is being opened. It is delivered after the wait.
            loop.exec(QEventLoop::ExcludeUserInputEvents);
            timer.stop();

            if (!watcher) {
                // Watcher died. This can happen if importer got deleted while we were
                // waiting. See:
                // https://bugs.kde.org/show_bug.cgi?id=237156
                return false;
            }

            if(!watcher->isFinished()) {
                // Timed out
                return false;
            }
        } else {
            watcher->waitForFinished();
        }

        if (watcher->isError()) {
            DMWARNING << watcher->error().message();
            return false;
        }

        return true;
    }
};

DBusMenuImporter::DBusMenuImporter(const QString &service, const QString &path, QObject *parent)
: DBusMenuImporter(service, path, ASYNCHRONOUS, parent)
{
}

DBusMenuImporter::DBusMenuImporter(const QString &service, const QString &path, DBusMenuImporterType type, QObject *parent)
: QObject(parent)
, d(new DBusMenuImporterPrivate)
{
    DBusMenuTypes_register();

    d->q = this;
    d->m_interface = new QDBusInterface(service, path, DBUSMENU_INTERFACE, QDBusConnection::sessionBus(), this);
    d->m_menu = 0;
    d->m_mustEmitMenuUpdated = false;
    d->m_refreshing = false;

    d->m_type = type;

    connect(&d->m_mapper, SIGNAL(mappedInt(int)), SLOT(sendClickedEvent(int)));

    d->m_pendingLayoutUpdateTimer = new QTimer(this);
    d->m_pendingLayoutUpdateTimer->setSingleShot(true);
    /**
     * WARNING: To avoid a Qt bug triggered by "QMenu::timerEvent" on refreshing
     * a menu that has a visible submenu, a minimum interval of two seconds is
     * set between two consecutive updates for the sake of certainty.
     *
     * With intervals shorter than one second, the pointer "d->currentAction" in
     * QMenu's code may become dangling and cause a crash.
     *
     * To be on the safe side, we also use "Menu", instead of QMenu, to block some
     * events during updates, especially the timer events.
     */
    d->m_pendingLayoutUpdateTimer->setInterval(LAYOUT_UPDATE_TIMEOUT);
    connect(d->m_pendingLayoutUpdateTimer, SIGNAL(timeout()), SLOT(processPendingLayoutUpdates()));

    // For some reason, using QObject::connect() does not work but
    // QDBusConnect::connect() does
    QDBusConnection::sessionBus().connect(service, path, DBUSMENU_INTERFACE, "LayoutUpdated"_L1, "ui"_L1,
        this, SLOT(slotLayoutUpdated(uint, int)));
    QDBusConnection::sessionBus().connect(service, path, DBUSMENU_INTERFACE, "ItemsPropertiesUpdated"_L1, "a(ia{sv})a(ias)"_L1,
        this, SLOT(slotItemsPropertiesUpdated(DBusMenuItemList, DBusMenuItemKeysList)));
    QDBusConnection::sessionBus().connect(service, path, DBUSMENU_INTERFACE, "ItemActivationRequested"_L1, "iu"_L1,
        this, SLOT(slotItemActivationRequested(int, uint)));

    d->refresh(0);
}

DBusMenuImporter::~DBusMenuImporter()
{
    // Do not use "delete d->m_menu": even if we are being deleted we should
    // leave enough time for the menu to finish what it was doing, for example
    // if it was being displayed.
    d->m_menu->deleteLater();
    delete d;
}

void DBusMenuImporter::slotLayoutUpdated(uint /*revision*/, int parentId)
{
    if (d->m_idsRefreshedByAboutToShow.remove(parentId)) {
        return;
    }
    if (!d->m_pendingLayoutUpdates.contains(parentId)) {
        d->m_pendingLayoutUpdates << parentId;
    }
    if (!d->m_pendingLayoutUpdateTimer->isActive() && !d->m_refreshing) {
        d->m_pendingLayoutUpdateTimer->start();
    }
}

void DBusMenuImporter::processPendingLayoutUpdates()
{
    const QSet<int> ids = d->m_pendingLayoutUpdates;
    d->m_pendingLayoutUpdates.clear();
    for (int id : ids) {
        d->refresh(id);
    }
}

QMenu *DBusMenuImporter::menu() const
{
    if (!d->m_menu) {
        d->m_menu = d->createMenu(0);
    }
    return d->m_menu;
}

void DBusMenuImporterPrivate::slotItemsPropertiesUpdated(const DBusMenuItemList &updatedList, const DBusMenuItemKeysList &removedList)
{
    for (const DBusMenuItem &item : updatedList) {
        QAction *action = m_actionForId.value(item.id);
        if (!action) {
            // We don't know this action. It probably is in a menu we haven't fetched yet.
            continue;
        }

        QVariantMap::ConstIterator
            it = item.properties.constBegin(),
            end = item.properties.constEnd();
        for(; it != end; ++it) {
            updateActionProperty(action, it.key(), it.value());
        }
    }

    for (const DBusMenuItemKeys &item : removedList) {
        QAction *action = m_actionForId.value(item.id);
        if (!action) {
            // We don't know this action. It probably is in a menu we haven't fetched yet.
            continue;
        }

        for (const QString &key : std::as_const(item.properties)) {
            updateActionProperty(action, key, QVariant());
        }
    }
}

void DBusMenuImporter::slotItemActivationRequested(int id, uint /*timestamp*/)
{
    QAction *action = d->m_actionForId.value(id);
    DMRETURN_IF_FAIL(action);
    actionActivationRequested(action);
}

void DBusMenuImporter::slotGetLayoutFinished(QDBusPendingCallWatcher *watcher)
{
    int parentId = watcher->property(DBUSMENU_PROPERTY_ID).toInt();
    const bool resendClick = watcher->property(DBUSMENU_PROPERTY_RESEND_CLICK).toBool();
    watcher->deleteLater();

    QDBusPendingReply<uint, DBusMenuLayoutItem> reply = *watcher;
    if (!reply.isValid()) {
        DMWARNING << reply.error().message();
        d->m_refreshing = false;
        if (resendClick) {
            d->resolvePendingClick(nullptr); // drop it
        }
        return;
    }

    #ifdef BENCHMARK
    DMDEBUG << "- items received:" << sChrono.elapsed() << "ms";
    #endif
    DBusMenuLayoutItem rootItem = reply.argumentAt<1>();

    QMenu *menu = d->menuForId(parentId);
    if (!menu) {
        DMWARNING << "No menu for id" << parentId;
        d->m_refreshing = false;
        if (resendClick) {
            d->resolvePendingClick(nullptr); // drop it
        }
        return;
    }

    if (d->m_lockedMenus.contains(menu)) {
        // WORKAROUND for a crash in Qt's menu code (see "slotMenuAboutToShow()"):
        // a submenu of this menu is being opened, and reloading this menu now
        // would hide it while Qt is still opening it. Fetch the layout again later.
        d->m_pendingLayoutUpdates << parentId;
        if (resendClick) {
            d->m_resendClickIds << parentId;
        }
        d->m_refreshing = false;
        d->m_pendingLayoutUpdateTimer->start();
        return;
    }

    // Block some menu events while actions are reloaded.
    auto m = qobject_cast<Menu*>(menu);
    const bool wasBlocked = m && m->blockEvents;
    if (m) {
        m->blockEvents = true;
    }

    menu->setProperty(DBUSMENU_PROPERTY_STALE, QVariant());
    menu->setUpdatesEnabled(false);
    QAction *activeBefore = menu->activeAction();
    const QList<QAction*> oldActions = menu->actions();
    const int activeIndex = oldActions.indexOf(activeBefore);

    // WORKAROUND for problems in Qt's menu code: keep the current actions
    // wherever the new items are of the same kind, and only update their ids
    // and properties. Apps like nm-applet rebuild their whole menu with new ids
    // but mostly the same items. Recreating all actions changed the size of the
    // visible menu while it was reloaded, which Qt's menu code does not handle
    // well: on X11, the menu closed; on Wayland, it blinked and a "Leave" event
    // cleared its active action; and it could grow and shrink suddenly. It also
    // deleted actions that Qt's menu code might still use (see the WARNING in
    // the constructor and "slotMenuAboutToShow()").
    QList<QAction*> keptActions;
    for (int i = 0; i < rootItem.children.size(); ++i) {
        const DBusMenuLayoutItem &dbusMenuItem = rootItem.children.at(i);
        QAction *old = oldActions.value(i);
        if (old && d->canReuseAction(old, dbusMenuItem.properties)) {
            d->reuseAction(old, dbusMenuItem.id, dbusMenuItem.properties);
            keptActions << old;
            continue;
        }

        // Insert the new action before removing the old one, so that a visible
        // menu never becomes empty. NOTE: The new submenus will be refreshed in
        // "slotMenuAboutToShow" on showing.
        QAction *action = d->createAction(dbusMenuItem.id, dbusMenuItem.properties, menu);
        menu->insertAction(old, action);
        d->m_actionForId.insert(dbusMenuItem.id, action);
        connect(action, SIGNAL(triggered()),
            &d->m_mapper, SLOT(map()));
        d->m_mapper.setMapping(action, dbusMenuItem.id);
    }

    // Remove the old actions that are not kept, like "QMenu::clear()" does.
    for (QAction *act : oldActions) {
        if (keptActions.contains(act)) {
            continue;
        }
        menu->removeAction(act);
        if (act->parent() == menu && act->associatedObjects().isEmpty()) {
            delete act;
        }
    }

    // If the active action was removed, activate the one at its place, such
    // that a visible submenu will be reopened, or the last one.
    if (activeIndex > -1 && !keptActions.contains(activeBefore)) {
        const auto acts = menu->actions();
        QAction *activeAction = nullptr;
        for (int i = activeIndex; i < acts.size() && !activeAction; ++i) {
            if (acts.at(i)->isEnabled() && !acts.at(i)->isSeparator()) {
                activeAction = acts.at(i);
            }
        }
        for (int i = acts.size() - 1; i >= 0 && !activeAction; --i) {
            if (acts.at(i)->isEnabled() && !acts.at(i)->isSeparator()) {
                activeAction = acts.at(i);
            }
        }
        if (activeAction) {
            menu->setActiveAction(activeAction);
        }
    }
    menu->setUpdatesEnabled(true);

    if (m) {
        m->blockEvents = wasBlocked;
    }

    if (resendClick) {
        d->resolvePendingClick(menu);
    }

    // The next update of pending layouts, if any.
    d->m_refreshing = false;
    if (d->m_pendingLayoutUpdateTimer) {
        d->m_pendingLayoutUpdateTimer->start();
    }

    #ifdef BENCHMARK
    DMDEBUG << "- Menu filled:" << sChrono.elapsed() << "ms";
    #endif
}

void DBusMenuImporter::sendClickedEvent(int id)
{
    // Some apps (e.g. nm-applet) renew all ids on each layout update. So, while
    // an update is pending or being fetched, the clicked id may be stale. Then,
    // fetch the layout now and resend the click to the item at the same place.
    if (d->m_pendingLayoutUpdates.isEmpty() && !d->m_refreshing) {
        d->sendEvent(id, "clicked"_L1);
        return;
    }

    // Find the path of the item, from the root menu.
    QList<DBusMenuImporterPrivate::ClickLevel> path;
    bool reachedRoot = false;
    QAction *action = d->m_actionForId.value(id);
    while (action) {
        QMenu *parentMenu = qobject_cast<QMenu*>(action->parent());
        if (!parentMenu || !parentMenu->actions().contains(action)) {
            break;
        }
        path.prepend(DBusMenuImporterPrivate::clickLevel(parentMenu, action));
        if (parentMenu == menu()) {
            reachedRoot = true;
            break;
        }
        QMenu *grandParent = qobject_cast<QMenu*>(parentMenu->parent());
        action = grandParent ? DBusMenuImporterPrivate::actionForMenu(grandParent, parentMenu) : nullptr;
    }
    if (!reachedRoot) {
        d->sendEvent(id, "clicked"_L1);
        return;
    }

    d->resolveClick(path);
}

void DBusMenuImporter::updateMenu()
{
    d->m_mustEmitMenuUpdated = true;
    QMetaObject::invokeMethod(menu(), "aboutToShow");
}

void DBusMenuImporter::slotMenuAboutToShow()
{
    QMenu *menu = qobject_cast<QMenu*>(sender());
    Q_ASSERT(menu);

    QAction *action = menu->menuAction();
    Q_ASSERT(action);

    int id = action->property(DBUSMENU_PROPERTY_ID).toInt();

    #ifdef BENCHMARK
    QTime time;
    time.start();
    #endif

    QDBusPendingCall call = d->m_interface->asyncCall("AboutToShow"_L1, id);
    QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(call, this);
    watcher->setProperty(DBUSMENU_PROPERTY_ID, id);
    connect(watcher, SIGNAL(finished(QDBusPendingCallWatcher*)),
        SLOT(slotAboutToShowDBusCallFinished(QDBusPendingCallWatcher*)));

    QPointer<QObject> guard(this);

    // WORKAROUND for a crash in Qt's menu code (a problem similar to
    // https://bugreports.qt.io/browse/QTBUG-77273): the wait below runs a nested
    // event loop inside "QMenu::popup()". If a parent menu is reloaded or
    // handles its timers meanwhile, this menu may be hidden while Qt is still
    // opening it, and Qt crashes in "QMenu::internalDelayedPopup()" because it
    // does not expect that. So, lock the parent menus until then.
    const QList<QMenu*> parents = d->parentMenus(menu);
    QList<QPointer<QMenu>> lockedParents;
    QList<bool> wereBlocked;
    for (QMenu *parent : parents) {
        d->m_lockedMenus << parent;
        lockedParents << parent;
        auto m = qobject_cast<Menu*>(parent);
        wereBlocked << (m && m->blockEvents);
        if (m) {
            m->blockEvents = true;
        }
    }

    if (!d->waitForWatcher(watcher, ABOUT_TO_SHOW_TIMEOUT)) {
        DMWARNING << "Application did not answer to AboutToShow() before timeout";
    }

    #ifdef BENCHMARK
    DMVAR(time.elapsed());
    #endif
    // "this" got deleted during the call to waitForWatcher(), get out
    if (!guard) {
        return;
    }

    for (int i = 0; i < parents.size(); ++i) {
        d->m_lockedMenus.removeOne(parents.at(i));
        if (auto m = qobject_cast<Menu*>(lockedParents.at(i).data())) {
            m->blockEvents = wereBlocked.at(i);
        }
    }

    if (menu == d->m_menu && d->m_mustEmitMenuUpdated) {
        d->m_mustEmitMenuUpdated = false;
        menuUpdated();
    }
    if (menu == d->m_menu) {
        menuReadyToBeShown();
    }

    d->sendEvent(id, "opened"_L1);
}

void DBusMenuImporter::slotAboutToShowDBusCallFinished(QDBusPendingCallWatcher *watcher)
{
    int id = watcher->property(DBUSMENU_PROPERTY_ID).toInt();
    watcher->deleteLater();

    QDBusPendingReply<bool> reply = *watcher;
    if (reply.isError()) {
        DMWARNING << "Call to AboutToShow() failed:" << reply.error().message();
        return;
    }
    bool needRefresh = reply.argumentAt<0>();

    QMenu *menu = d->menuForId(id);
    DMRETURN_IF_FAIL(menu);

    if (needRefresh || menu->actions().isEmpty() || menu->property(DBUSMENU_PROPERTY_STALE).toBool()) {
        d->m_idsRefreshedByAboutToShow << id;
        QDBusPendingCallWatcher *watcher2 = d->refresh(id);
        if (!d->waitForWatcher(watcher2, REFRESH_TIMEOUT)) {
            DMWARNING << "Application did not refresh before timeout";
        }
    }
}

void DBusMenuImporter::slotMenuAboutToHide()
{
    QMenu *menu = qobject_cast<QMenu*>(sender());
    Q_ASSERT(menu);

    QAction *action = menu->menuAction();
    Q_ASSERT(action);

    int id = action->property(DBUSMENU_PROPERTY_ID).toInt();
    d->sendEvent(id, QString("closed"_L1));
}

QMenu *DBusMenuImporter::createMenu(QWidget *parent)
{
    return new Menu(parent); // use "Menu" for our workaround
}

QIcon DBusMenuImporter::iconForName(const QString &/*name*/)
{
    return QIcon();
}

#include "moc_dbusmenuimporter.cpp"
