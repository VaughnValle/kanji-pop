/*
*  Copyright 2016  Smith AR <audoban@openmailbox.org>
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

#ifndef LATTECORONA_H
#define LATTECORONA_H

// local
#include <coretypes.h>
#include "plasma/quick/configview.h"
#include "layout/storage.h"
#include "view/panelshadows_p.h"

// Qt
#include <QObject>
#include <QTimer>

// Plasma
#include <Plasma/Corona>

// KDE
#include <KAboutApplicationDialog>

namespace KDeclarative {
class QmlObjectSharedEngine;
}

namespace Plasma {
class Corona;
class Containment;
class Types;
}

namespace PlasmaQuick {
class ConfigView;
}

namespace KActivities {
class Consumer;
}

namespace KWayland {
namespace Client {
class PlasmaShell;
}
}

namespace Latte {
class CentralLayout;
class ScreenPool;
class GlobalShortcuts;
class UniversalSettings;
class View;
namespace Indicator{
class Factory;
}
namespace Layout{
class GenericLayout;
class Storage;
}
namespace Layouts{
class LaunchersSignals;
class Manager;
}
namespace PlasmaExtended{
class ScreenGeometries;
class ScreenPool;
class Theme;
}
namespace WindowSystem{
class AbstractWindowInterface;
}
}

namespace Latte {

class Corona : public Plasma::Corona
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.LatteDock")

public:
    Corona(bool defaultLayoutOnStartup = false,
               QString layoutNameOnStartUp = QString(),
               int userSetMemoryUsage = -1,
               QObject *parent = nullptr);
    virtual ~Corona();

    int numScreens() const override;
    QRect screenGeometry(int id) const override;
    QRegion availableScreenRegion(int id) const override;
    QRect availableScreenRect(int id) const override;

    //! This is a very generic function in order to return the availableScreenRect of specific screen
    //! by calculating only the user specified visibility modes and edges. Empty QLists for both
    //! arguments mean that all choices are accepted in calculations. ignoreExternalPanels means that
    //! external panels should be not considered in the calculations
    QRect availableScreenRectWithCriteria(int id,
                                          QString forLayout = QString(),
                                          QList<Types::Visibility> ignoreModes = QList<Types::Visibility>(),
                                          QList<Plasma::Types::Location> ignoreEdges = QList<Plasma::Types::Location>(),
                                          bool ignoreExternalPanels = true,
                                          bool desktopUse = false) const;

    QRegion availableScreenRegionWithCriteria(int id,
                                              QString forLayout = QString(),
                                              QList<Types::Visibility> ignoreModes = QList<Types::Visibility>(),
                                              QList<Plasma::Types::Location> ignoreEdges = QList<Plasma::Types::Location>(),
                                              bool ignoreExternalPanels = true,
                                              bool desktopUse = false) const;

    int screenForContainment(const Plasma::Containment *containment) const override;

    KWayland::Client::PlasmaShell *waylandCoronaInterface() const;

    KActivities::Consumer *activitiesConsumer() const;
    GlobalShortcuts *globalShortcuts() const;
    ScreenPool *screenPool() const;
    UniversalSettings *universalSettings() const;
    Layouts::Manager *layoutsManager() const;   

    Indicator::Factory *indicatorFactory() const;

    PlasmaExtended::ScreenPool *plasmaScreenPool() const;
    PlasmaExtended::Theme *themeExtended() const;

    WindowSystem::AbstractWindowInterface *wm() const;

    PanelShadows *dialogShadows() const;

    //! Needs to be called in order to import and load application properly after application
    //! finished all its exit operations
    void importFullConfiguration(const QString &file);

    //! these functions are used from context menu through containmentactions    
    void quitApplication();
    void switchToLayout(QString layout);
    void showSettingsWindow(int page);
    void setContextMenuView(int id);
    QStringList contextMenuData();

public slots:
    void aboutApplication();
    void addViewForLayout(QString layoutName);
    void activateLauncherMenu();
    void loadDefaultLayout() override;
    void setBackgroundFromBroadcast(QString activity, QString screenName, QString filename);
    void setBroadcastedBackgroundsEnabled(QString activity, QString screenName, bool enabled);
    void showAlternativesForApplet(Plasma::Applet *applet);
    void toggleHiddenState(QString layoutName, QString screenName, int screenEdge);

    //! values are separated with a "-" character
    void windowColorScheme(QString windowIdAndScheme);
    void updateDockItemBadge(QString identifier, QString value);

    void unload();

signals:
    void configurationShown(PlasmaQuick::ConfigView *configView);
    void viewLocationChanged();
    void raiseViewsTemporaryChanged();
    void availableScreenRectChangedFrom(Latte::View *origin);
    void availableScreenRegionChangedFrom(Latte::View *origin);
    void verticalUnityViewHasFocus();

private slots:
    void alternativesVisibilityChanged(bool visible);
    void load();

    void addOutput(QScreen *screen);
    void primaryOutputChanged();
    void screenRemoved(QScreen *screen);
    void screenCountChanged();
    void syncLatteViewsToScreens();

private:
    void cleanConfig();
    void qmlRegisterTypes() const;
    void setupWaylandIntegration();

    bool appletExists(uint containmentId, uint appletId) const;
    bool containmentExists(uint id) const;

    int primaryScreenId() const;

    QStringList containmentsIds();
    QStringList appletsIds();

    Layout::GenericLayout *layout(QString name) const;
    CentralLayout *centralLayout(QString name) const;

private:

    bool m_activitiesStarting{true};
    bool m_defaultLayoutOnStartup{false}; //! this is used to enforce loading the default layout on startup
    bool m_quitTimedEnded{false}; //! this is used on destructor in order to delay it and slide-out the views

    //!it can be used on startup to change memory usage from command line
    int m_userSetMemoryUsage{ -1};

    int m_contextMenuViewId{-1};

    QString m_layoutNameOnStartUp;
    QString m_importFullConfigurationFile;

    QList<KDeclarative::QmlObjectSharedEngine *> m_alternativesObjects;

    QTimer m_viewsScreenSyncTimer;

    KActivities::Consumer *m_activitiesConsumer;
    QPointer<KAboutApplicationDialog> aboutDialog;

    ScreenPool *m_screenPool{nullptr};
    UniversalSettings *m_universalSettings{nullptr};
    GlobalShortcuts *m_globalShortcuts{nullptr};

    Indicator::Factory *m_indicatorFactory{nullptr};
    Layouts::Manager *m_layoutsManager{nullptr};

    PlasmaExtended::ScreenGeometries *m_plasmaGeometries{nullptr};
    PlasmaExtended::ScreenPool *m_plasmaScreenPool{nullptr};
    PlasmaExtended::Theme *m_themeExtended{nullptr};

    WindowSystem::AbstractWindowInterface *m_wm{nullptr};

    PanelShadows *m_dialogShadows{nullptr};

    KWayland::Client::PlasmaShell *m_waylandCorona{nullptr};

    friend class GlobalShortcuts;
    friend class Layout::Storage;
    friend class Layouts::LaunchersSignals;
    friend class Layouts::Manager;
};

}

#endif // LATTECORONA_H
