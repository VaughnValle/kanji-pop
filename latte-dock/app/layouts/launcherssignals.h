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

#ifndef LAUNCHERSSIGNALS_H
#define LAUNCHERSSIGNALS_H

// Qt
#include <QObject>

namespace Plasma {
class Applet;
}

namespace Latte {
namespace Layouts {
class Manager;
}
}

namespace Latte {
namespace Layouts {

//! in order to support property the launcher groups Layout and Global
//! the latte plasmoids must communicate between them with signals when
//! there are changes in their models. This way we are trying to avoid
//! crashes that occur by setting the launcherList of the tasksModel so
//! often. The plasma devs of libtaskmanager have designed the launchers
//! model to be initialized only once during startup
class LaunchersSignals : public QObject
{
    Q_OBJECT

public:
    LaunchersSignals(QObject *parent);
    ~LaunchersSignals() override;

public slots:
    Q_INVOKABLE void addLauncher(QString layoutName, int launcherGroup, QString launcher);
    Q_INVOKABLE void removeLauncher(QString layoutName, int launcherGroup, QString launcher);
    Q_INVOKABLE void addLauncherToActivity(QString layoutName, int launcherGroup, QString launcher, QString activity);
    Q_INVOKABLE void removeLauncherFromActivity(QString layoutName, int launcherGroup, QString launcher, QString activity);
    Q_INVOKABLE void urlsDropped(QString layoutName, int launcherGroup, QStringList urls);
    //!Deprecated because it could create crashes, validateLaunchersOrder provides a better approach
    Q_INVOKABLE void moveTask(QString layoutName, uint senderId, int launcherGroup, int from, int to);
    Q_INVOKABLE void validateLaunchersOrder(QString layoutName, uint senderId, int launcherGroup, QStringList launchers);

private:
    QList<Plasma::Applet *> lattePlasmoids(QString layoutName);

private:
    Layouts::Manager *m_manager{nullptr};
};

}
}

#endif
