/*
*  Copyright 2016  Smith AR <audoban@openmaibox.org>
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

#include "lattecorona.h"

// local
#include <coretypes.h>
#include "alternativeshelper.h"
#include "apptypes.h"
#include "lattedockadaptor.h"
#include "screenpool.h"
#include "declarativeimports/interfaces.h"
#include "indicator/factory.h"
#include "layout/centrallayout.h"
#include "layout/genericlayout.h"
#include "layout/sharedlayout.h"
#include "layouts/importer.h"
#include "layouts/manager.h"
#include "layouts/synchronizer.h"
#include "layouts/launcherssignals.h"
#include "shortcuts/globalshortcuts.h"
#include "package/lattepackage.h"
#include "plasma/extended/backgroundcache.h"
#include "plasma/extended/backgroundtracker.h"
#include "plasma/extended/screengeometries.h"
#include "plasma/extended/screenpool.h"
#include "plasma/extended/theme.h"
#include "settings/universalsettings.h"
#include "settings/dialogs/settingsdialog.h"
#include "view/view.h"
#include "view/windowstracker/windowstracker.h"
#include "view/windowstracker/allscreenstracker.h"
#include "view/windowstracker/currentscreentracker.h"
#include "wm/abstractwindowinterface.h"
#include "wm/schemecolors.h"
#include "wm/waylandinterface.h"
#include "wm/xwindowinterface.h"
#include "wm/tracker/lastactivewindow.h"
#include "wm/tracker/schemes.h"
#include "wm/tracker/windowstracker.h"

// Qt
#include <QAction>
#include <QApplication>
#include <QScreen>
#include <QDBusConnection>
#include <QDebug>
#include <QDesktopWidget>
#include <QFile>
#include <QFontDatabase>
#include <QQmlContext>
#include <QProcess>

// Plasma
#include <Plasma>
#include <Plasma/Corona>
#include <Plasma/Containment>
#include <PlasmaQuick/ConfigView>

// KDE
#include <KActionCollection>
#include <KPluginMetaData>
#include <KGlobalAccel>
#include <KLocalizedString>
#include <KPackage/Package>
#include <KPackage/PackageLoader>
#include <KAboutData>
#include <KActivities/Consumer>
#include <KDeclarative/QmlObjectSharedEngine>
#include <KWindowSystem>
#include <KWayland/Client/connection_thread.h>
#include <KWayland/Client/registry.h>
#include <KWayland/Client/plasmashell.h>
#include <KWayland/Client/plasmawindowmanagement.h>

namespace Latte {

Corona::Corona(bool defaultLayoutOnStartup, QString layoutNameOnStartUp, int userSetMemoryUsage, QObject *parent)
    : Plasma::Corona(parent),
      m_defaultLayoutOnStartup(defaultLayoutOnStartup),
      m_userSetMemoryUsage(userSetMemoryUsage),
      m_layoutNameOnStartUp(layoutNameOnStartUp),
      m_activitiesConsumer(new KActivities::Consumer(this)),
      m_screenPool(new ScreenPool(KSharedConfig::openConfig(), this)),
      m_indicatorFactory(new Indicator::Factory(this)),
      m_universalSettings(new UniversalSettings(KSharedConfig::openConfig(), this)),
      m_globalShortcuts(new GlobalShortcuts(this)),
      m_plasmaScreenPool(new PlasmaExtended::ScreenPool(this)),
      m_themeExtended(new PlasmaExtended::Theme(KSharedConfig::openConfig(), this)),
      m_layoutsManager(new Layouts::Manager(this)),
      m_plasmaGeometries(new PlasmaExtended::ScreenGeometries(this)),
      m_dialogShadows(new PanelShadows(this, QStringLiteral("dialogs/background")))
{
    //! create the window manager

    if (KWindowSystem::isPlatformWayland()) {
        m_wm = new WindowSystem::WaylandInterface(this);
    } else {
        m_wm = new WindowSystem::XWindowInterface(this);
    }

    setupWaylandIntegration();

    KPackage::Package package(new Latte::Package(this));

    m_screenPool->load();

    if (!package.isValid()) {
        qWarning() << staticMetaObject.className()
                   << "the package" << package.metadata().rawData() << "is invalid!";
        return;
    } else {
        qDebug() << staticMetaObject.className()
                 << "the package" << package.metadata().rawData() << "is valid!";
    }

    setKPackage(package);
    //! universal settings / extendedtheme must be loaded after the package has been set
    m_universalSettings->load();
    m_themeExtended->load();

    qmlRegisterTypes();

    if (m_activitiesConsumer && (m_activitiesConsumer->serviceStatus() == KActivities::Consumer::Running)) {
        load();
    }

    connect(m_activitiesConsumer, &KActivities::Consumer::serviceStatusChanged, this, &Corona::load);

    m_viewsScreenSyncTimer.setSingleShot(true);
    m_viewsScreenSyncTimer.setInterval(m_universalSettings->screenTrackerInterval());
    connect(&m_viewsScreenSyncTimer, &QTimer::timeout, this, &Corona::syncLatteViewsToScreens);
    connect(m_universalSettings, &UniversalSettings::screenTrackerIntervalChanged, this, [this]() {
        m_viewsScreenSyncTimer.setInterval(m_universalSettings->screenTrackerInterval());
    });

    //! Dbus adaptor initialization
    new LatteDockAdaptor(this);
    QDBusConnection dbus = QDBusConnection::sessionBus();
    dbus.registerObject(QStringLiteral("/Latte"), this);
}

Corona::~Corona()
{
    //! BEGIN: Give the time to slide-out views when closing
    m_layoutsManager->synchronizer()->hideAllViews();

    //! Don't delay the destruction under wayland in any case
    //! because it creates a crash with kwin effects
    //! https://bugs.kde.org/show_bug.cgi?id=392890
    if (!KWindowSystem::isPlatformWayland()) {
        QTimer::singleShot(400, [this]() {
            m_quitTimedEnded = true;
        });

        while (!m_quitTimedEnded) {
            QGuiApplication::processEvents(QEventLoop::AllEvents, 50);
        }
    }

    //! END: slide-out views when closing

    m_viewsScreenSyncTimer.stop();

    if (m_layoutsManager->memoryUsage() == MemoryUsage::SingleLayout) {
        cleanConfig();
    }

    qDebug() << "Latte Corona - unload: containments ...";

    m_layoutsManager->unload();

    m_plasmaGeometries->deleteLater();
    m_wm->deleteLater();
    m_dialogShadows->deleteLater();
    m_globalShortcuts->deleteLater();
    m_layoutsManager->deleteLater();
    m_screenPool->deleteLater();
    m_universalSettings->deleteLater();
    m_plasmaScreenPool->deleteLater();
    m_themeExtended->deleteLater();
    m_indicatorFactory->deleteLater();

    disconnect(m_activitiesConsumer, &KActivities::Consumer::serviceStatusChanged, this, &Corona::load);
    delete m_activitiesConsumer;

    qDebug() << "Latte Corona - deleted...";

    if (!m_importFullConfigurationFile.isEmpty()) {
        //!NOTE: Restart latte to import the new configuration
        QString importCommand = "latte-dock --import-full \"" + m_importFullConfigurationFile + "\"";
        qDebug() << "Executing Import Full Configuration command : " << importCommand;

        QProcess::startDetached(importCommand);
    }
}

void Corona::load()
{
    if (m_activitiesConsumer && (m_activitiesConsumer->serviceStatus() == KActivities::Consumer::Running) && m_activitiesStarting) {
        m_activitiesStarting = false;

        disconnect(m_activitiesConsumer, &KActivities::Consumer::serviceStatusChanged, this, &Corona::load);

        m_layoutsManager->load();

        connect(this, &Corona::availableScreenRectChangedFrom, this, &Plasma::Corona::availableScreenRectChanged);
        connect(this, &Corona::availableScreenRegionChangedFrom, this, &Plasma::Corona::availableScreenRegionChanged);

        connect(qGuiApp, &QGuiApplication::primaryScreenChanged, this, &Corona::primaryOutputChanged, Qt::UniqueConnection);

        connect(m_screenPool, &ScreenPool::primaryPoolChanged, this, &Corona::screenCountChanged);

        QString assignedLayout = m_layoutsManager->synchronizer()->shouldSwitchToLayout(m_activitiesConsumer->currentActivity());

        QString loadLayoutName = "";

        if (!m_defaultLayoutOnStartup && m_layoutNameOnStartUp.isEmpty()) {
            if (!assignedLayout.isEmpty() && assignedLayout != m_universalSettings->currentLayoutName()) {
                loadLayoutName = assignedLayout;
            } else {
                loadLayoutName = m_universalSettings->currentLayoutName();
            }

            if (!m_layoutsManager->synchronizer()->layoutExists(loadLayoutName)) {
                loadLayoutName = m_layoutsManager->defaultLayoutName();
                m_layoutsManager->importDefaultLayout(false);
            }
        } else if (m_defaultLayoutOnStartup) {
            loadLayoutName = m_layoutsManager->importer()->uniqueLayoutName(m_layoutsManager->defaultLayoutName());
            m_layoutsManager->importDefaultLayout(true);
        } else {
            loadLayoutName = m_layoutNameOnStartUp;
        }

        if (m_userSetMemoryUsage != -1 && !KWindowSystem::isPlatformWayland()) {
            MemoryUsage::LayoutsMemory usage = static_cast<MemoryUsage::LayoutsMemory>(m_userSetMemoryUsage);

            m_universalSettings->setLayoutsMemoryUsage(usage);
        }

        if (KWindowSystem::isPlatformWayland()) {
            m_universalSettings->setLayoutsMemoryUsage(MemoryUsage::SingleLayout);
        }

        m_layoutsManager->loadLayoutOnStartup(loadLayoutName);


        //! load screens signals such screenGeometryChanged in order to support
        //! plasmoid.screenGeometry properly
        for (QScreen *screen : qGuiApp->screens()) {
            addOutput(screen);
        }

        connect(qGuiApp, &QGuiApplication::screenAdded, this, &Corona::addOutput, Qt::UniqueConnection);
        connect(qGuiApp, &QGuiApplication::screenRemoved, this, &Corona::screenRemoved, Qt::UniqueConnection);
    }
}

void Corona::unload()
{
    qDebug() << "unload: removing containments...";

    while (!containments().isEmpty()) {
        //deleting a containment will remove it from the list due to QObject::destroyed connect in Corona
        //this form doesn't crash, while qDeleteAll(containments()) does
        delete containments().first();
    }
}

void Corona::setupWaylandIntegration()
{
    if (!KWindowSystem::isPlatformWayland()) {
        return;
    }

    using namespace KWayland::Client;

    auto connection = ConnectionThread::fromApplication(this);

    if (!connection) {
        return;
    }

    Registry *registry{new Registry(this)};
    registry->create(connection);

    connect(registry, &Registry::plasmaShellAnnounced, this
            , [this, registry](quint32 name, quint32 version) {
        m_waylandCorona = registry->createPlasmaShell(name, version, this);
    });

    QObject::connect(registry, &KWayland::Client::Registry::plasmaWindowManagementAnnounced,
                     [this, registry](quint32 name, quint32 version) {
        KWayland::Client::PlasmaWindowManagement *pwm = registry->createPlasmaWindowManagement(name, version, this);

        WindowSystem::WaylandInterface *wI = qobject_cast<WindowSystem::WaylandInterface *>(m_wm);

        if (wI) {
            wI->initWindowManagement(pwm);
        }
    });

#if KF5_VERSION_MINOR >= 52
    QObject::connect(registry, &KWayland::Client::Registry::plasmaVirtualDesktopManagementAnnounced,
                     [this, registry] (quint32 name, quint32 version) {
        KWayland::Client::PlasmaVirtualDesktopManagement *vdm = registry->createPlasmaVirtualDesktopManagement(name, version, this);

        WindowSystem::WaylandInterface *wI = qobject_cast<WindowSystem::WaylandInterface *>(m_wm);

        if (wI) {
            wI->initVirtualDesktopManagement(vdm);
        }
    });
#endif

    registry->setup();
    connection->roundtrip();
}

KWayland::Client::PlasmaShell *Corona::waylandCoronaInterface() const
{
    return m_waylandCorona;
}

void Corona::cleanConfig()
{
    auto containmentsEntries = config()->group("Containments");
    bool changed = false;

    for(const auto &cId : containmentsEntries.groupList()) {
        if (!containmentExists(cId.toUInt())) {
            //cleanup obsolete containments
            containmentsEntries.group(cId).deleteGroup();
            changed = true;
            qDebug() << "obsolete containment configuration deleted:" << cId;
        } else {
            //cleanup obsolete applets of running containments
            auto appletsEntries = containmentsEntries.group(cId).group("Applets");

            for(const auto &appletId : appletsEntries.groupList()) {
                if (!appletExists(cId.toUInt(), appletId.toUInt())) {
                    appletsEntries.group(appletId).deleteGroup();
                    changed = true;
                    qDebug() << "obsolete applet configuration deleted:" << appletId;
                }
            }
        }
    }

    if (changed) {
        config()->sync();
        qDebug() << "configuration file cleaned...";
    }
}

bool Corona::containmentExists(uint id) const
{
    for(const auto containment : containments()) {
        if (id == containment->id()) {
            return true;
        }
    }

    return false;
}

bool Corona::appletExists(uint containmentId, uint appletId) const
{
    Plasma::Containment *containment = nullptr;

    for(const auto cont : containments()) {
        if (containmentId == cont->id()) {
            containment = cont;
            break;
        }
    }

    if (!containment) {
        return false;
    }

    for(const auto applet : containment->applets()) {
        if (applet->id() == appletId) {
            return true;
        }
    }

    return false;
}

KActivities::Consumer *Corona::activitiesConsumer() const
{
    return m_activitiesConsumer;
}

PanelShadows *Corona::dialogShadows() const
{
    return m_dialogShadows;
}

GlobalShortcuts *Corona::globalShortcuts() const
{
    return m_globalShortcuts;
}

ScreenPool *Corona::screenPool() const
{
    return m_screenPool;
}

UniversalSettings *Corona::universalSettings() const
{
    return m_universalSettings;
}

WindowSystem::AbstractWindowInterface *Corona::wm() const
{
    return m_wm;
}

Indicator::Factory *Corona::indicatorFactory() const
{
    return m_indicatorFactory;
}

Layouts::Manager *Corona::layoutsManager() const
{
    return m_layoutsManager;
}

PlasmaExtended::ScreenPool *Corona::plasmaScreenPool() const
{
    return m_plasmaScreenPool;
}

PlasmaExtended::Theme *Corona::themeExtended() const
{
    return m_themeExtended;
}

int Corona::numScreens() const
{
    return qGuiApp->screens().count();
}

QRect Corona::screenGeometry(int id) const
{
    const auto screens = qGuiApp->screens();
    const QScreen *screen{qGuiApp->primaryScreen()};

    QString screenName;

    if (m_screenPool->hasId(id)) {
        screenName = m_screenPool->connector(id);
    }

    for(const auto scr : screens) {
        if (scr->name() == screenName) {
            screen = scr;
            break;
        }
    }

    return screen->geometry();
}

CentralLayout *Corona::centralLayout(QString name) const
{
    CentralLayout *result{nullptr};

    if (name.isEmpty()) {
        result = m_layoutsManager->currentLayout();
    } else {
        CentralLayout *tempCentral = m_layoutsManager->synchronizer()->centralLayout(name);

        if (!tempCentral) {
            //! Identify best active layout to be used for metrics calculations.
            //! Active layouts are always take into account their shared layouts for their metrics
            SharedLayout *sharedLayout = m_layoutsManager->synchronizer()->sharedLayout(name);

            if (sharedLayout) {
                tempCentral = sharedLayout->currentCentralLayout();
            }
        }

        if (tempCentral) {
            result = tempCentral;
        }
    }

    return result;
}

Layout::GenericLayout *Corona::layout(QString name) const
{
    Layout::GenericLayout *result{nullptr};

    if (name.isEmpty()) {
        result = m_layoutsManager->currentLayout();
    } else {
        result = m_layoutsManager->synchronizer()->layout(name);

        if (!result) {
            result = m_layoutsManager->currentLayout();
        }
    }

    return result;
}

QRegion Corona::availableScreenRegion(int id) const
{
    return availableScreenRegionWithCriteria(id);
}

QRegion Corona::availableScreenRegionWithCriteria(int id,
                                                  QString forLayout,
                                                  QList<Types::Visibility> ignoreModes,
                                                  QList<Plasma::Types::Location> ignoreEdges,
                                                  bool ignoreExternalPanels,
                                                  bool desktopUse) const
{
    const QScreen *screen = m_screenPool->screenForId(id);
    CentralLayout *layout = centralLayout(forLayout);

    if (!screen) {
        return {};
    }

    QRegion available = ignoreExternalPanels ? screen->geometry() : screen->availableGeometry();

    if (!layout) {
        return available;
    }

    //! blacklist irrelevant visibility modes
    if (!ignoreModes.contains(Latte::Types::None)) {
        ignoreModes << Latte::Types::None;
    }

    if (!ignoreModes.contains(Latte::Types::NormalWindow)) {
        ignoreModes << Latte::Types::NormalWindow;
    }

    bool allEdges = ignoreEdges.isEmpty();
    QList<Latte::View *> views = layout->latteViews();

    for (const auto *view : views) {
        if (view && view->containment() && view->screen() == screen
                && ((allEdges || !ignoreEdges.contains(view->location()))
                    && (view->visibility() && !ignoreModes.contains(view->visibility()->mode())))) {
            int realThickness = view->normalThickness();

            int x = 0; int y = 0; int w = 0; int h = 0;

            switch (view->formFactor()) {
            case Plasma::Types::Horizontal:
                if (view->behaveAsPlasmaPanel()) {
                    w = view->width();
                    x = view->x();
                } else {
                    w = view->maxLength() * view->width();
                    int offsetW = view->offset() * view->width();

                    switch (view->alignment()) {
                    case Latte::Types::Left:
                        x = view->x() + offsetW;
                        break;

                    case Latte::Types::Center:
                    case Latte::Types::Justify:
                        x = (view->geometry().center().x() - w/2) + offsetW;
                        break;

                    case Latte::Types::Right:
                        x = view->geometry().right() - w - offsetW;
                        break;
                    }
                }
                break;
            case Plasma::Types::Vertical:
                if (view->behaveAsPlasmaPanel()) {
                    h = view->height();
                    y = view->y();
                } else {
                    h = view->maxLength() * view->height();
                    int offsetH = view->offset() * view->height();

                    switch (view->alignment()) {
                    case Latte::Types::Top:
                        y = view->y() + offsetH;
                        break;

                    case Latte::Types::Center:
                    case Latte::Types::Justify:
                        y = (view->geometry().center().y() - h/2) + offsetH;
                        break;

                    case Latte::Types::Bottom:
                        y = view->geometry().bottom() - h - offsetH;
                        break;
                    }
                }
                break;
            }

            // Usually availableScreenRect is used by the desktop,
            // but Latte don't have desktop, then here just
            // need calculate available space for top and bottom location,
            // because the left and right are those who dodge others views
            switch (view->location()) {
            case Plasma::Types::TopEdge:
                if (view->behaveAsPlasmaPanel()) {
                    QRect viewGeometry = view->geometry();

                    if (desktopUse) {
                        //! ignore any real window slide outs in all cases
                        viewGeometry.moveTop(view->screen()->geometry().top() + view->screenEdgeMargin());
                    }

                    available -= viewGeometry;
                } else {                  
                    y = view->y();
                    available -= QRect(x, y, w, realThickness);
                }

                break;

            case Plasma::Types::BottomEdge:
                if (view->behaveAsPlasmaPanel()) {
                    QRect viewGeometry = view->geometry();

                    if (desktopUse) {
                        //! ignore any real window slide outs in all cases
                        viewGeometry.moveTop(view->screen()->geometry().bottom() - view->screenEdgeMargin() - viewGeometry.height());
                    }

                    available -= viewGeometry;
                } else {
                    y = view->geometry().bottom() - realThickness + 1;
                    available -= QRect(x, y, w, realThickness);
                }

                break;

            case Plasma::Types::LeftEdge:
                if (view->behaveAsPlasmaPanel()) {
                    QRect viewGeometry = view->geometry();

                    if (desktopUse) {
                        //! ignore any real window slide outs in all cases
                        viewGeometry.moveLeft(view->screen()->geometry().left() + view->screenEdgeMargin());
                    }

                    available -= viewGeometry;
                } else {
                    x = view->x();
                    available -= QRect(x, y, realThickness, h);
                }

                break;

            case Plasma::Types::RightEdge:
                if (view->behaveAsPlasmaPanel()) {
                    QRect viewGeometry = view->geometry();

                    if (desktopUse) {
                        //! ignore any real window slide outs in all cases
                        viewGeometry.moveLeft(view->screen()->geometry().right() - view->screenEdgeMargin() - viewGeometry.width());
                    }

                    available -= viewGeometry;
                } else {                    
                    x = view->geometry().right() - realThickness + 1;
                    available -= QRect(x, y, realThickness, h);
                }

                break;

            default:
                //! bypass clang warnings
                break;
            }
        }
    }

    /*qDebug() << "::::: FREE AREAS :::::";

    for (int i = 0; i < available.rectCount(); ++i) {
        qDebug() << available.rects().at(i);
    }

    qDebug() << "::::: END OF FREE AREAS :::::";*/

    return available;
}

QRect Corona::availableScreenRect(int id) const
{
    return availableScreenRectWithCriteria(id);
}

QRect Corona::availableScreenRectWithCriteria(int id,
                                              QString forLayout,
                                              QList<Types::Visibility> ignoreModes,
                                              QList<Plasma::Types::Location> ignoreEdges,
                                              bool ignoreExternalPanels,
                                              bool desktopUse) const
{
    const QScreen *screen = m_screenPool->screenForId(id);
    CentralLayout *layout = centralLayout(forLayout);

    if (!screen) {
        return {};
    }

    QRect available = ignoreExternalPanels ? screen->geometry() : screen->availableGeometry();

    if (!layout) {
        return available;
    }

    //! blacklist irrelevant visibility modes
    if (!ignoreModes.contains(Latte::Types::None)) {
        ignoreModes << Latte::Types::None;
    }

    if (!ignoreModes.contains(Latte::Types::NormalWindow)) {
        ignoreModes << Latte::Types::NormalWindow;
    }

    bool allEdges = ignoreEdges.isEmpty();
    QList<Latte::View *> views = layout->latteViews();

    for (const auto *view : views) {
        if (view && view->containment() && view->screen() == screen
                && ((allEdges || !ignoreEdges.contains(view->location()))
                    && (view->visibility() && !ignoreModes.contains(view->visibility()->mode())))) {

            int appliedThickness = view->behaveAsPlasmaPanel() ? view->screenEdgeMargin() + view->normalThickness() : view->normalThickness();

            // Usually availableScreenRect is used by the desktop,
            // but Latte don't have desktop, then here just
            // need calculate available space for top and bottom location,
            // because the left and right are those who dodge others docks
            switch (view->location()) {
            case Plasma::Types::TopEdge:
                if (view->behaveAsPlasmaPanel() && desktopUse) {
                    //! ignore any real window slide outs in all cases
                    available.setTop(qMax(available.top(), view->screen()->geometry().top() + appliedThickness));
                } else {
                    available.setTop(qMax(available.top(), view->y() + appliedThickness));
                }
                break;

            case Plasma::Types::BottomEdge:
                if (view->behaveAsPlasmaPanel() && desktopUse) {
                    //! ignore any real window slide outs in all cases
                    available.setBottom(qMin(available.bottom(), view->screen()->geometry().bottom() - appliedThickness));
                } else {
                    available.setBottom(qMin(available.bottom(), view->y() + view->height() - appliedThickness));
                }
                break;

            case Plasma::Types::LeftEdge:
                if (view->behaveAsPlasmaPanel() && desktopUse) {
                    //! ignore any real window slide outs in all cases
                    available.setLeft(qMax(available.left(), view->screen()->geometry().left() + appliedThickness));
                } else {
                    available.setLeft(qMax(available.left(), view->x() + appliedThickness));
                }
                break;

            case Plasma::Types::RightEdge:
                if (view->behaveAsPlasmaPanel() && desktopUse) {
                    //! ignore any real window slide outs in all cases
                    available.setRight(qMin(available.right(), view->screen()->geometry().right() - appliedThickness));
                } else {
                    available.setRight(qMin(available.right(), view->x() + view->width() - appliedThickness));
                }
                break;

            default:
                //! bypass clang warnings
                break;
            }
        }
    }

    return available;
}

void Corona::addOutput(QScreen *screen)
{
    Q_ASSERT(screen);

    int id = m_screenPool->id(screen->name());

    if (id == -1) {
        int newId = m_screenPool->firstAvailableId();
        m_screenPool->insertScreenMapping(newId, screen->name());
    }

    connect(screen, &QScreen::geometryChanged, this, [ = ]() {
        const int id = m_screenPool->id(screen->name());

        if (id >= 0) {
            emit screenGeometryChanged(id);
            emit availableScreenRegionChanged();
            emit availableScreenRectChanged();
        }
    });

    emit availableScreenRectChanged();
    emit screenAdded(m_screenPool->id(screen->name()));

    screenCountChanged();
}

void Corona::primaryOutputChanged()
{
    m_viewsScreenSyncTimer.start();
}

void Corona::screenRemoved(QScreen *screen)
{
    screenCountChanged();
}

void Corona::screenCountChanged()
{
    m_viewsScreenSyncTimer.start();
}

//! the central functions that updates loading/unloading latteviews
//! concerning screen changed (for multi-screen setups mainly)
void Corona::syncLatteViewsToScreens()
{
    m_layoutsManager->synchronizer()->syncLatteViewsToScreens();
}

int Corona::primaryScreenId() const
{
    return m_screenPool->id(qGuiApp->primaryScreen()->name());
}

void Corona::quitApplication()
{
    //! this code must be called asynchronously because it is called
    //! also from qml (Settings window).
    QTimer::singleShot(300, [this]() {
        m_layoutsManager->hideLatteSettingsDialog();
        m_layoutsManager->synchronizer()->hideAllViews();
    });

    //! give the time for the views to hide themselves
    QTimer::singleShot(800, [this]() {
        qGuiApp->quit();
    });
}

void Corona::aboutApplication()
{
    if (aboutDialog) {
        aboutDialog->hide();
        aboutDialog->deleteLater();
    }

    aboutDialog = new KAboutApplicationDialog(KAboutData::applicationData());
    connect(aboutDialog.data(), &QDialog::finished, aboutDialog.data(), &QObject::deleteLater);
    m_wm->skipTaskBar(*aboutDialog);
    m_wm->setKeepAbove(aboutDialog->winId(), true);

    aboutDialog->show();
}

int Corona::screenForContainment(const Plasma::Containment *containment) const
{
    //FIXME: indexOf is not a proper way to support multi-screen
    // as for environment to environment the indexes change
    // also there is the following issue triggered
    // from latteView adaptToScreen()
    //
    // in a multi-screen environment that
    // primary screen is not set to 0 it was
    // created an endless showing loop at
    // startup (catch-up race) between
    // screen:0 and primaryScreen

    //case in which this containment is child of an applet, hello systray :)

    if (Plasma::Applet *parentApplet = qobject_cast<Plasma::Applet *>(containment->parent())) {
        if (Plasma::Containment *cont = parentApplet->containment()) {
            return screenForContainment(cont);
        } else {
            return -1;
        }
    }

    Plasma::Containment *c = const_cast<Plasma::Containment *>(containment);
    Latte::View *view =  m_layoutsManager->synchronizer()->viewForContainment(c);

    if (view && view->screen()) {
        return m_screenPool->id(view->screen()->name());
    }

    //Failed? fallback on lastScreen()
    //lastScreen() is the correct screen for panels
    //It is also correct for desktops *that have the correct activity()*
    //a containment with lastScreen() == 0 but another activity,
    //won't be associated to a screen
    //     qDebug() << "ShellCorona screenForContainment: " << containment << " Last screen is " << containment->lastScreen();

    for (auto screen : qGuiApp->screens()) {
        // containment->lastScreen() == m_screenPool->id(screen->name()) to check if the lastScreen refers to a screen that exists/it's known
        if (containment->lastScreen() == m_screenPool->id(screen->name()) &&
                (containment->activity() == m_activitiesConsumer->currentActivity() ||
                 containment->containmentType() == Plasma::Types::PanelContainment || containment->containmentType() == Plasma::Types::CustomPanelContainment)) {
            return containment->lastScreen();
        }
    }

    return -1;
}

void Corona::showAlternativesForApplet(Plasma::Applet *applet)
{
    const QString alternativesQML = kPackage().filePath("appletalternativesui");

    if (alternativesQML.isEmpty()) {
        return;
    }

    Latte::View *latteView =  m_layoutsManager->synchronizer()->viewForContainment(applet->containment());

    KDeclarative::QmlObjectSharedEngine *qmlObj{nullptr};

    if (latteView) {
        latteView->setAlternativesIsShown(true);
        qmlObj = new KDeclarative::QmlObjectSharedEngine(latteView);
    } else {
        qmlObj = new KDeclarative::QmlObjectSharedEngine(this);
    }

    qmlObj->setInitializationDelayed(true);
    qmlObj->setSource(QUrl::fromLocalFile(alternativesQML));

    AlternativesHelper *helper = new AlternativesHelper(applet, qmlObj);
    qmlObj->rootContext()->setContextProperty(QStringLiteral("alternativesHelper"), helper);

    m_alternativesObjects << qmlObj;
    qmlObj->completeInitialization();

    //! Alternative dialog signals
    connect(helper, &QObject::destroyed, this, [latteView]() {
        latteView->setAlternativesIsShown(false);
    });

    connect(qmlObj->rootObject(), SIGNAL(visibleChanged(bool)),
            this, SLOT(alternativesVisibilityChanged(bool)));

    connect(applet, &Plasma::Applet::destroyedChanged, this, [this, qmlObj](bool destroyed) {
        if (!destroyed) {
            return;
        }

        QMutableListIterator<KDeclarative::QmlObjectSharedEngine *> it(m_alternativesObjects);

        while (it.hasNext()) {
            KDeclarative::QmlObjectSharedEngine *obj = it.next();

            if (obj == qmlObj) {
                it.remove();
                obj->deleteLater();
            }
        }
    });
}

void Corona::alternativesVisibilityChanged(bool visible)
{
    if (visible) {
        return;
    }

    QObject *root = sender();

    QMutableListIterator<KDeclarative::QmlObjectSharedEngine *> it(m_alternativesObjects);

    while (it.hasNext()) {
        KDeclarative::QmlObjectSharedEngine *obj = it.next();

        if (obj->rootObject() == root) {
            it.remove();
            obj->deleteLater();
        }
    }
}

void Corona::addViewForLayout(QString layoutName)
{
    qDebug() << "loading default layout";
    //! Setting mutable for create a containment
    setImmutability(Plasma::Types::Mutable);
    QVariantList args;
    auto defaultContainment = createContainmentDelayed("org.kde.latte.containment", args);
    defaultContainment->setContainmentType(Plasma::Types::PanelContainment);
    defaultContainment->init();

    if (!defaultContainment || !defaultContainment->kPackage().isValid()) {
        qWarning() << "the requested containment plugin can not be located or loaded";
        return;
    }

    auto config = defaultContainment->config();
    defaultContainment->restore(config);

    using Plasma::Types;
    QList<Types::Location> edges{Types::BottomEdge, Types::LeftEdge,
                Types::TopEdge, Types::RightEdge};

    Layout::GenericLayout *currentLayout = m_layoutsManager->synchronizer()->layout(layoutName);

    if (currentLayout) {
        edges = currentLayout->freeEdges(defaultContainment->screen());
    }

    if ((edges.count() > 0)) {
        defaultContainment->setLocation(edges.at(0));
    } else {
        defaultContainment->setLocation(Plasma::Types::BottomEdge);
    }

    if (m_layoutsManager->memoryUsage() == MemoryUsage::MultipleLayouts) {
        config.writeEntry("layoutId", layoutName);
    }

    defaultContainment->updateConstraints(Plasma::Types::StartupCompletedConstraint);

    defaultContainment->save(config);
    requestConfigSync();

    defaultContainment->flushPendingConstraintsEvents();
    emit containmentAdded(defaultContainment);
    emit containmentCreated(defaultContainment);

    defaultContainment->createApplet(QStringLiteral("org.kde.latte.plasmoid"));
}

void Corona::loadDefaultLayout()
{
    addViewForLayout(m_layoutsManager->currentLayoutName());
}

QStringList Corona::containmentsIds()
{
    QStringList ids;

    for(const auto containment : containments()) {
        ids << QString::number(containment->id());
    }

    return ids;
}

QStringList Corona::appletsIds()
{
    QStringList ids;

    for(const auto containment : containments()) {
        auto applets = containment->config().group("Applets");
        ids << applets.groupList();
    }

    return ids;
}

//! Activate launcher menu through dbus interface
void Corona::activateLauncherMenu()
{
    m_globalShortcuts->activateLauncherMenu();
}

void Corona::windowColorScheme(QString windowIdAndScheme)
{
    int firstSlash = windowIdAndScheme.indexOf("-");
    QString windowIdStr = windowIdAndScheme.mid(0, firstSlash);
    QString schemeStr = windowIdAndScheme.mid(firstSlash + 1);

    if (KWindowSystem::isPlatformWayland()) {
        QTimer::singleShot(200, [this, schemeStr]() {
            //! [Wayland Case] - give the time to be informed correctly for the active window id
            //! otherwise the active window id may not be the same with the one trigerred
            //! the color scheme dbus signal
            QString windowIdStr = m_wm->activeWindow().toString();
            m_wm->schemesTracker()->setColorSchemeForWindow(windowIdStr.toUInt(), schemeStr);
        });
    } else {
        m_wm->schemesTracker()->setColorSchemeForWindow(windowIdStr.toUInt(), schemeStr);
    }
}

//! update badge for specific view item
void Corona::updateDockItemBadge(QString identifier, QString value)
{
    m_globalShortcuts->updateViewItemBadge(identifier, value);
}


void Corona::switchToLayout(QString layout)
{
    if ((layout.startsWith("file:/") || layout.startsWith("/")) && layout.endsWith(".layout.latte")) {
        //! Import and load runtime a layout through dbus interface
        //! It can be used from external programs that want to update runtime
        //! the Latte shown layout
        QString layoutPath = layout;

        //! cleanup layout path
        if (layoutPath.startsWith("file:///")) {
            layoutPath = layout.remove("file://");
        } else if (layoutPath.startsWith("file://")) {
            layoutPath = layout.remove("file:/");
        }

        //! check out layoutpath existence
        if (QFileInfo(layoutPath).exists()) {
            qDebug() << " Layout is going to be imported and loaded from file :: " << layoutPath;

            QString importedLayout = m_layoutsManager->importer()->importLayoutHelper(layoutPath);

            if (importedLayout.isEmpty()) {
                qDebug() << i18n("The layout cannot be imported from file :: ") << layoutPath;
            } else {
                m_layoutsManager->synchronizer()->loadLayouts();
                m_layoutsManager->switchToLayout(importedLayout);
            }
        } else {
            qDebug() << " Layout from missing file can not be imported and loaded :: " << layoutPath;
        }
    } else {
        m_layoutsManager->switchToLayout(layout);
    }
}

void Corona::showSettingsWindow(int page)
{
    Settings::Dialog::ConfigurationPage p = Settings::Dialog::LayoutPage;

    if (page >= Settings::Dialog::LayoutPage && page <= Settings::Dialog::PreferencesPage) {
        p = static_cast<Settings::Dialog::ConfigurationPage>(page);
    }

    m_layoutsManager->showLatteSettingsDialog(p);
}

void Corona::setContextMenuView(int id)
{
    //! set context menu view id
    m_contextMenuViewId = id;
}

QStringList Corona::contextMenuData()
{
    QStringList data;
    Types::ViewType viewType{Types::DockView};

    Latte::CentralLayout *currentLayout = m_layoutsManager->currentLayout();

    if (currentLayout) {
        viewType = currentLayout->latteViewType(m_contextMenuViewId);
    }

    data << QString::number((int)m_layoutsManager->memoryUsage());
    data << m_layoutsManager->currentLayoutName();
    data << QString::number((int)viewType);

    for(const auto &layoutName : m_layoutsManager->menuLayouts()) {
        if (m_layoutsManager->synchronizer()->centralLayout(layoutName)) {
            data << QString("1," + layoutName);
        } else {
            data << QString("0," + layoutName);
        }
    }

    //! reset context menu view id
    m_contextMenuViewId = -1;
    return data;
}

void Corona::setBackgroundFromBroadcast(QString activity, QString screenName, QString filename)
{
    if (filename.startsWith("file://")) {
        filename = filename.remove(0,7);
    }

    PlasmaExtended::BackgroundCache::self()->setBackgroundFromBroadcast(activity, screenName, filename);
}

void Corona::setBroadcastedBackgroundsEnabled(QString activity, QString screenName, bool enabled)
{
    PlasmaExtended::BackgroundCache::self()->setBroadcastedBackgroundsEnabled(activity, screenName, enabled);
}

void Corona::toggleHiddenState(QString layoutName, QString screenName, int screenEdge)
{
    Layout::GenericLayout *gLayout = layout(layoutName);

    if (gLayout) {
        gLayout->toggleHiddenState(screenName, (Plasma::Types::Location)screenEdge);
    }
}

void Corona::importFullConfiguration(const QString &file)
{
    m_importFullConfigurationFile = file;
    quitApplication();
}

inline void Corona::qmlRegisterTypes() const
{   
    qmlRegisterUncreatableMetaObject(Latte::Settings::staticMetaObject,
                                     "org.kde.latte.private.app",          // import statement
                                     0, 1,                                 // major and minor version of the import
                                     "Settings",                           // name in QML
                                     "Error: only enums of latte app settings");

    qmlRegisterType<Latte::BackgroundTracker>("org.kde.latte.private.app", 0, 1, "BackgroundTracker");
    qmlRegisterType<Latte::Interfaces>("org.kde.latte.private.app", 0, 1, "Interfaces");


#if QT_VERSION < QT_VERSION_CHECK(5, 14, 0)
    qmlRegisterType<QScreen>();
    qmlRegisterType<Latte::View>();
    qmlRegisterType<Latte::ViewPart::WindowsTracker>();
    qmlRegisterType<Latte::ViewPart::TrackerPart::CurrentScreenTracker>();
    qmlRegisterType<Latte::ViewPart::TrackerPart::AllScreensTracker>();
    qmlRegisterType<Latte::WindowSystem::SchemeColors>();
    qmlRegisterType<Latte::WindowSystem::Tracker::LastActiveWindow>();
    qmlRegisterType<Latte::Types>();
#else
    qmlRegisterAnonymousType<QScreen>("latte-dock", 1);
    qmlRegisterAnonymousType<Latte::View>("latte-dock", 1);
    qmlRegisterAnonymousType<Latte::ViewPart::WindowsTracker>("latte-dock", 1);
    qmlRegisterAnonymousType<Latte::ViewPart::TrackerPart::CurrentScreenTracker>("latte-dock", 1);
    qmlRegisterAnonymousType<Latte::ViewPart::TrackerPart::AllScreensTracker>("latte-dock", 1);
    qmlRegisterAnonymousType<Latte::WindowSystem::SchemeColors>("latte-dock", 1);
    qmlRegisterAnonymousType<Latte::WindowSystem::Tracker::LastActiveWindow>("latte-dock", 1);
    qmlRegisterAnonymousType<Latte::Types>("latte-dock", 1);
#endif
}

}
