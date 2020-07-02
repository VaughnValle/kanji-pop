/*
*  Copyright 2017  Smith AR <audoban@openmailbox.org>
*                  Michail Vourlakos <mvourlakos@gmail.com>
*
*  This file is part of Latte-Dock
*
*  Latte-Dock is free software; you can redistribute it and/or
*  modify it under the terms of the GNU General Public License as
*  published by the Free Software Foundation; either version 2 of
*  the License, or (at your option) any later version.
*
*  Latte-Dock is distributed in the hope that it will be useful,
*  but WITHOUT ANY WARRANTY; without even the implied warranty of
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*  GNU General Public License for more details.
*
*  You should have received a copy of the GNU General Public License
*  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "launcherssignals.h"

// local
#include <coretypes.h>
#include "../lattecorona.h"
#include "../layout/centrallayout.h"
#include "../layouts/manager.h"
#include "../layouts/synchronizer.h"

// Qt
#include <QQuickItem>

// Plasma
#include <Plasma/Applet>
#include <Plasma/Containment>


namespace Latte {
namespace Layouts {

LaunchersSignals::LaunchersSignals(QObject *parent)
    : QObject(parent)
{
    m_manager = qobject_cast<Layouts::Manager *>(parent);
}

LaunchersSignals::~LaunchersSignals()
{
}

QList<Plasma::Applet *> LaunchersSignals::lattePlasmoids(QString layoutName)
{
    QList<Plasma::Applet *> applets;

    CentralLayout *layout = m_manager->synchronizer()->centralLayout(layoutName);
    QList<Plasma::Containment *> containments;

    if (layoutName.isEmpty()) {
        containments = m_manager->corona()->containments();
    } else if (layout) {
        containments = *(layout->containments());
    }

    for(const auto containment : containments) {
        for(auto *applet : containment->applets()) {
            KPluginMetaData meta = applet->kPackage().metadata();

            if (meta.pluginId() == "org.kde.latte.plasmoid") {
                applets.append(applet);
            }
        }
    }

    return applets;
}

void LaunchersSignals::addLauncher(QString layoutName, int launcherGroup, QString launcher)
{
    Types::LaunchersGroup group = static_cast<Types::LaunchersGroup>(launcherGroup);

    if ((Types::LaunchersGroup)group == Types::UniqueLaunchers) {
        return;
    }

    QString lName = (group == Types::LayoutLaunchers) ? layoutName : "";

    for(const auto applet : lattePlasmoids(lName)) {
        if (QQuickItem *appletInterface = applet->property("_plasma_graphicObject").value<QQuickItem *>()) {
            const auto &childItems = appletInterface->childItems();

            if (childItems.isEmpty()) {
                continue;
            }

            for (QQuickItem *item : childItems) {
                if (auto *metaObject = item->metaObject()) {
                    int methodIndex = metaObject->indexOfMethod("extSignalAddLauncher(QVariant,QVariant)");

                    if (methodIndex == -1) {
                        continue;
                    }

                    QMetaMethod method = metaObject->method(methodIndex);
                    method.invoke(item, Q_ARG(QVariant, launcherGroup), Q_ARG(QVariant, launcher));
                }
            }
        }
    }
}

void LaunchersSignals::removeLauncher(QString layoutName, int launcherGroup, QString launcher)
{
    Types::LaunchersGroup group = static_cast<Types::LaunchersGroup>(launcherGroup);

    if ((Types::LaunchersGroup)group == Types::UniqueLaunchers) {
        return;
    }

    QString lName = (group == Types::LayoutLaunchers) ? layoutName : "";

    for(const auto applet : lattePlasmoids(lName)) {
        if (QQuickItem *appletInterface = applet->property("_plasma_graphicObject").value<QQuickItem *>()) {
            const auto &childItems = appletInterface->childItems();

            if (childItems.isEmpty()) {
                continue;
            }

            for (QQuickItem *item : childItems) {
                if (auto *metaObject = item->metaObject()) {
                    int methodIndex = metaObject->indexOfMethod("extSignalRemoveLauncher(QVariant,QVariant)");

                    if (methodIndex == -1) {
                        continue;
                    }

                    QMetaMethod method = metaObject->method(methodIndex);
                    method.invoke(item, Q_ARG(QVariant, launcherGroup), Q_ARG(QVariant, launcher));
                }
            }
        }
    }
}

void LaunchersSignals::addLauncherToActivity(QString layoutName, int launcherGroup, QString launcher, QString activity)
{
    Types::LaunchersGroup group = static_cast<Types::LaunchersGroup>(launcherGroup);

    if ((Types::LaunchersGroup)group == Types::UniqueLaunchers) {
        return;
    }

    QString lName = (group == Types::LayoutLaunchers) ? layoutName : "";

    for(const auto applet : lattePlasmoids(lName)) {
        if (QQuickItem *appletInterface = applet->property("_plasma_graphicObject").value<QQuickItem *>()) {
            const auto &childItems = appletInterface->childItems();

            if (childItems.isEmpty()) {
                continue;
            }

            for (QQuickItem *item : childItems) {
                if (auto *metaObject = item->metaObject()) {
                    int methodIndex = metaObject->indexOfMethod("extSignalAddLauncherToActivity(QVariant,QVariant,QVariant)");

                    if (methodIndex == -1) {
                        continue;
                    }

                    QMetaMethod method = metaObject->method(methodIndex);
                    method.invoke(item, Q_ARG(QVariant, launcherGroup), Q_ARG(QVariant, launcher), Q_ARG(QVariant, activity));
                }
            }
        }
    }
}

void LaunchersSignals::removeLauncherFromActivity(QString layoutName, int launcherGroup, QString launcher, QString activity)
{
    Types::LaunchersGroup group = static_cast<Types::LaunchersGroup>(launcherGroup);

    if ((Types::LaunchersGroup)group == Types::UniqueLaunchers) {
        return;
    }

    QString lName = (group == Types::LayoutLaunchers) ? layoutName : "";

    for(const auto applet : lattePlasmoids(lName)) {
        if (QQuickItem *appletInterface = applet->property("_plasma_graphicObject").value<QQuickItem *>()) {
            const auto &childItems = appletInterface->childItems();

            if (childItems.isEmpty()) {
                continue;
            }

            for (QQuickItem *item : childItems) {
                if (auto *metaObject = item->metaObject()) {
                    int methodIndex = metaObject->indexOfMethod("extSignalRemoveLauncherFromActivity(QVariant,QVariant,QVariant)");

                    if (methodIndex == -1) {
                        continue;
                    }

                    QMetaMethod method = metaObject->method(methodIndex);
                    method.invoke(item, Q_ARG(QVariant, launcherGroup), Q_ARG(QVariant, launcher), Q_ARG(QVariant, activity));
                }
            }
        }
    }
}

void LaunchersSignals::urlsDropped(QString layoutName, int launcherGroup, QStringList urls)
{
    Types::LaunchersGroup group = static_cast<Types::LaunchersGroup>(launcherGroup);

    if ((Types::LaunchersGroup)group == Types::UniqueLaunchers) {
        return;
    }

    QString lName = (group == Types::LayoutLaunchers) ? layoutName : "";

    for(const auto applet : lattePlasmoids(lName)) {
        if (QQuickItem *appletInterface = applet->property("_plasma_graphicObject").value<QQuickItem *>()) {
            const auto &childItems = appletInterface->childItems();

            if (childItems.isEmpty()) {
                continue;
            }

            for (QQuickItem *item : childItems) {
                if (auto *metaObject = item->metaObject()) {
                    int methodIndex = metaObject->indexOfMethod("extSignalUrlsDropped(QVariant,QVariant)");

                    if (methodIndex == -1) {
                        continue;
                    }

                    QMetaMethod method = metaObject->method(methodIndex);
                    method.invoke(item, Q_ARG(QVariant, launcherGroup), Q_ARG(QVariant, urls));
                }
            }
        }
    }
}

void LaunchersSignals::moveTask(QString layoutName, uint senderId, int launcherGroup, int from, int to)
{
    Types::LaunchersGroup group = static_cast<Types::LaunchersGroup>(launcherGroup);

    if ((Types::LaunchersGroup)group == Types::UniqueLaunchers) {
        return;
    }

    QString lName = (group == Types::LayoutLaunchers) ? layoutName : "";

    for(const auto applet : lattePlasmoids(lName)) {
        if (applet->id() != senderId) {
            if (QQuickItem *appletInterface = applet->property("_plasma_graphicObject").value<QQuickItem *>()) {
                const auto &childItems = appletInterface->childItems();

                if (childItems.isEmpty()) {
                    continue;
                }

                for (QQuickItem *item : childItems) {
                    if (auto *metaObject = item->metaObject()) {
                        int methodIndex = metaObject->indexOfMethod("extSignalMoveTask(QVariant,QVariant,QVariant)");

                        if (methodIndex == -1) {
                            continue;
                        }

                        QMetaMethod method = metaObject->method(methodIndex);
                        method.invoke(item, Q_ARG(QVariant, launcherGroup), Q_ARG(QVariant, from), Q_ARG(QVariant, to));
                    }
                }
            }
        }
    }
}

void LaunchersSignals::validateLaunchersOrder(QString layoutName, uint senderId, int launcherGroup, QStringList launchers)
{
    Types::LaunchersGroup group = static_cast<Types::LaunchersGroup>(launcherGroup);

    if ((Types::LaunchersGroup)group == Types::UniqueLaunchers) {
        return;
    }

    QString lName = (group == Types::LayoutLaunchers) ? layoutName : "";

    for(const auto applet : lattePlasmoids(lName)) {
        if (applet->id() != senderId) {
            if (QQuickItem *appletInterface = applet->property("_plasma_graphicObject").value<QQuickItem *>()) {
                const auto &childItems = appletInterface->childItems();

                if (childItems.isEmpty()) {
                    continue;
                }

                for (QQuickItem *item : childItems) {
                    if (auto *metaObject = item->metaObject()) {
                        int methodIndex = metaObject->indexOfMethod("extSignalValidateLaunchersOrder(QVariant,QVariant)");

                        if (methodIndex == -1) {
                            continue;
                        }

                        QMetaMethod method = metaObject->method(methodIndex);
                        method.invoke(item, Q_ARG(QVariant, launcherGroup), Q_ARG(QVariant, launchers));
                    }
                }
            }
        }
    }
}

}
} //end of namespace
