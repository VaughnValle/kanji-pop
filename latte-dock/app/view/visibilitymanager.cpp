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

#include "visibilitymanager.h"

// local
#include "positioner.h"
#include "view.h"
#include "helpers/floatinggapwindow.h"
#include "helpers/screenedgeghostwindow.h"
#include "windowstracker/currentscreentracker.h"
#include "../apptypes.h"
#include "../lattecorona.h"
#include "../screenpool.h"
#include "../layouts/manager.h"
#include "../wm/abstractwindowinterface.h"

// Qt
#include <QDebug>

// KDE
#include <KWindowSystem>
#include <KWayland/Client/plasmashell.h>
#include <KWayland/Client/surface.h>

//! Hide Timer can create cases that when it is low it does not allow the
//! view to be show. For example !compositing+kwin_edges+hide inteval<50ms
const int HIDEMINIMUMINTERVAL = 50;

namespace Latte {
namespace ViewPart {

//! BEGIN: VisiblityManager implementation

VisibilityManager::VisibilityManager(PlasmaQuick::ContainmentView *view)
    : QObject(view)
{
    qDebug() << "VisibilityManager creating...";

    m_latteView = qobject_cast<Latte::View *>(view);
    m_corona = qobject_cast<Latte::Corona *>(view->corona());
    m_wm = m_corona->wm();

    connect(this, &VisibilityManager::hidingIsBlockedChanged, this, &VisibilityManager::on_hidingIsBlockedChanged);

    connect(this, &VisibilityManager::slideOutFinished, this, &VisibilityManager::updateHiddenState);
    connect(this, &VisibilityManager::slideInFinished, this, &VisibilityManager::updateHiddenState);

    connect(this, &VisibilityManager::enableKWinEdgesChanged, this, &VisibilityManager::updateKWinEdgesSupport);
    connect(this, &VisibilityManager::modeChanged, this, &VisibilityManager::updateKWinEdgesSupport);

    if (m_latteView) {
        connect(m_latteView, &Latte::View::eventTriggered, this, &VisibilityManager::viewEventManager);
        connect(m_latteView, &Latte::View::behaveAsPlasmaPanelChanged , this, &VisibilityManager::updateKWinEdgesSupport);
        connect(m_latteView, &Latte::View::byPassWMChanged, this, &VisibilityManager::updateKWinEdgesSupport);
        connect(m_latteView, &Latte::View::inEditModeChanged, this, &VisibilityManager::initViewFlags);

        connect(m_latteView, &Latte::View::absoluteGeometryChanged, this, [&]() {
            if (m_mode == Types::AlwaysVisible) {
                updateStrutsBasedOnLayoutsAndActivities();
            }
        });

        //! Frame Extents
        connect(m_latteView, &Latte::View::headThicknessGapChanged, this, &VisibilityManager::on_publishFrameExtents);
        connect(m_latteView, &Latte::View::locationChanged, this, [&]() {
            //! Resend frame extents because their geometry has changed
            const bool forceUpdate{true};
            publishFrameExtents(forceUpdate);
        });

        connect(m_latteView, &Latte::View::typeChanged, this, [&]() {
            if (m_latteView->inEditMode()) {
                //! Resend frame extents because type has changed
                const bool forceUpdate{true};
                publishFrameExtents(forceUpdate);
            }
        });

        connect(m_latteView, &Latte::View::forcedShown, this, [&]() {
            //! Resend frame extents to compositor otherwise because compositor cleared
            //! them with no reason when the user is closing an activity
            const bool forceUpdate{true};
            publishFrameExtents(forceUpdate);
        });

        connect(m_latteView, &Latte::View::screenEdgeMarginEnabledChanged, this, [&]() {
            if (!m_latteView->screenEdgeMarginEnabled()) {
                deleteFloatingGapWindow();
            }
        });

        connect(this, &VisibilityManager::modeChanged, this, [&]() {
            emit m_latteView->availableScreenRectChangedFrom(m_latteView);
        });
    }

    m_timerStartUp.setInterval(4000);
    m_timerStartUp.setSingleShot(true);
    m_timerShow.setSingleShot(true);
    m_timerHide.setSingleShot(true);

    connect(&m_timerShow, &QTimer::timeout, this, [&]() {
        if (m_isHidden ||  m_isBelowLayer) {
            //   qDebug() << "must be shown";
            emit mustBeShown();
        }
    });
    connect(&m_timerHide, &QTimer::timeout, this, [&]() {
        if (!hidingIsBlocked() && !m_isHidden && !m_isBelowLayer && !m_dragEnter) {
            if (m_latteView->isFloatingWindow()) {
                //! first check if mouse is inside the floating gap
                checkMouseInFloatingArea();
            } else {
                //! immediate call
                emit mustBeHide();
            }
        }
    });

    m_timerPublishFrameExtents.setInterval(1500);
    m_timerPublishFrameExtents.setSingleShot(true);
    connect(&m_timerPublishFrameExtents, &QTimer::timeout, this, [&]() { publishFrameExtents(); });

    restoreConfig();
}

VisibilityManager::~VisibilityManager()
{
    qDebug() << "VisibilityManager deleting...";
    m_wm->removeViewStruts(*m_latteView);

    if (m_edgeGhostWindow) {
        m_edgeGhostWindow->deleteLater();
    }

    if (m_floatingGapWindow) {
        m_floatingGapWindow->deleteLater();
    }
}

Types::Visibility VisibilityManager::mode() const
{
    return m_mode;
}

void VisibilityManager::initViewFlags()
{
    if ((m_mode == Types::WindowsCanCover || m_mode == Types::WindowsAlwaysCover) && (!m_latteView->inEditMode())) {
        setViewOnBackLayer();
    } else {
        setViewOnFrontLayer();
    }
}

void VisibilityManager::setViewOnBackLayer()
{
    m_wm->setViewExtraFlags(m_latteView, false, Types::WindowsAlwaysCover);
    setIsBelowLayer(true);
}

void VisibilityManager::setViewOnFrontLayer()
{
    m_wm->setViewExtraFlags(m_latteView, true);
    setIsBelowLayer(false);
}

void VisibilityManager::setMode(Latte::Types::Visibility mode)
{
    if (m_mode == mode)
        return;

    Q_ASSERT_X(mode != Types::None, staticMetaObject.className(), "set visibility to Types::None");

    // clear mode
    for (auto &c : m_connections) {
        disconnect(c);
    }

    int base{0};

    m_publishedStruts = QRect();

    if (m_mode == Types::AlwaysVisible) {
        //! remove struts for old always visible mode
        m_wm->removeViewStruts(*m_latteView);
    }

    m_timerShow.stop();
    m_timerHide.stop();
    m_mode = mode;

    initViewFlags();

    if (mode != Types::AlwaysVisible && mode != Types::WindowsGoBelow) {
        m_connections[0] = connect(m_wm, &WindowSystem::AbstractWindowInterface::currentDesktopChanged, this, [&] {
            if (m_raiseOnDesktopChange) {
                raiseViewTemporarily();
            }
        });
        m_connections[1] = connect(m_wm, &WindowSystem::AbstractWindowInterface::currentActivityChanged, this, [&]() {
            if (m_raiseOnActivityChange) {
                raiseViewTemporarily();
            } else {
                updateHiddenState();
            }
        });

        base = 2;
    }

    switch (m_mode) {
    case Types::AlwaysVisible: {
        if (m_latteView->containment() && m_latteView->screen()) {
            updateStrutsBasedOnLayoutsAndActivities();
        }

        m_connections[base] = connect(m_latteView, &Latte::View::normalThicknessChanged, this, [&]() {
            updateStrutsBasedOnLayoutsAndActivities();
        });

        m_connections[base+1] = connect(m_corona->layoutsManager(),  &Layouts::Manager::currentLayoutNameChanged, this, [&]() {
            if (m_corona && m_corona->layoutsManager()->memoryUsage() == MemoryUsage::MultipleLayouts) {
                updateStrutsBasedOnLayoutsAndActivities(true);
            }
        });

        m_connections[base+2] = connect(m_latteView, &Latte::View::activitiesChanged, this, [&]() {
            updateStrutsBasedOnLayoutsAndActivities(true);
        });

        raiseView(true);
        break;
    }

    case Types::AutoHide: {
        m_connections[base] = connect(this, &VisibilityManager::containsMouseChanged, this, [&]() {
            raiseView(m_containsMouse);
        });

        raiseView(m_containsMouse);
        break;
    }

    case Types::DodgeActive: {
        m_connections[base] = connect(this, &VisibilityManager::containsMouseChanged
                                      , this, &VisibilityManager::dodgeActive);
        m_connections[base+1] = connect(m_latteView->windowsTracker()->currentScreen(), &TrackerPart::CurrentScreenTracker::activeWindowTouchingChanged
                                        , this, &VisibilityManager::dodgeActive);

        dodgeActive();
        break;
    }

    case Types::DodgeMaximized: {
        m_connections[base] = connect(this, &VisibilityManager::containsMouseChanged
                                      , this, &VisibilityManager::dodgeMaximized);
        m_connections[base+1] = connect(m_latteView->windowsTracker()->currentScreen(), &TrackerPart::CurrentScreenTracker::activeWindowMaximizedChanged
                                        , this, &VisibilityManager::dodgeMaximized);

        dodgeMaximized();
        break;
    }

    case Types::DodgeAllWindows: {
        m_connections[base] = connect(this, &VisibilityManager::containsMouseChanged
                                      , this, &VisibilityManager::dodgeAllWindows);

        m_connections[base+1] = connect(m_latteView->windowsTracker()->currentScreen(), &TrackerPart::CurrentScreenTracker::existsWindowTouchingChanged
                                        , this, &VisibilityManager::dodgeAllWindows);

        dodgeAllWindows();
        break;
    }

    case Types::WindowsGoBelow:
        break;

    case Types::WindowsCanCover:
        m_connections[base] = connect(this, &VisibilityManager::containsMouseChanged, this, [&]() {
            if (m_containsMouse) {
                emit mustBeShown();
            } else {
                raiseView(false);
            }
        });

        raiseView(m_containsMouse);
        break;

    case Types::WindowsAlwaysCover:
        break;

    case Types::SideBar:
        m_connections[base] = connect(m_latteView, &Latte::View::inEditModeChanged, this, [&]() {
            if (!m_latteView->inEditMode()) {
                //! Give the time to View to change from !behaveAsPlasmaPanel to behaveAsPlasmaPanel
                //! if this is needed when changing to !inEditMode
                QTimer::singleShot(100, [this]() {
                    toggleHiddenState();
                });
            }
        });

        toggleHiddenState();

    default:
        break;
    }

    m_latteView->containment()->config().writeEntry("visibility", static_cast<int>(m_mode));

    updateKWinEdgesSupport();

    emit modeChanged();
}

void VisibilityManager::updateStrutsBasedOnLayoutsAndActivities(bool forceUpdate)
{
    bool multipleLayoutsAndCurrent = (m_corona->layoutsManager()->memoryUsage() == MemoryUsage::MultipleLayouts
                                      && m_latteView->layout() && !m_latteView->positioner()->inLocationAnimation()
                                      && m_latteView->layout()->isCurrent());

    if (m_corona->layoutsManager()->memoryUsage() == MemoryUsage::SingleLayout || multipleLayoutsAndCurrent) {
        QRect computedStruts = acceptableStruts();
        if (m_publishedStruts != computedStruts || forceUpdate) {
            //! Force update is needed when very important events happen in DE and there is a chance
            //! that previously even though struts where sent the DE did not accept them.
            //! Such a case is when STOPPING an Activity and windows faulty become invisible even
            //! though they should not. In such case setting struts when the windows are hidden
            //! the struts do not take any effect
            m_publishedStruts = computedStruts;
            m_wm->setViewStruts(*m_latteView, m_publishedStruts, m_latteView->location());
        }
    } else {
        m_publishedStruts = QRect();
        m_wm->removeViewStruts(*m_latteView);
    }
}

QRect VisibilityManager::acceptableStruts()
{
    QRect calcs;

    int screenEdgeMargin = (m_latteView->behaveAsPlasmaPanel() && m_latteView->screenEdgeMarginEnabled()) ? m_latteView->screenEdgeMargin() : 0;
    int shownThickness = m_latteView->normalThickness() + screenEdgeMargin;

    switch (m_latteView->location()) {
    case Plasma::Types::TopEdge: {
        calcs = QRect(m_latteView->x(), m_latteView->y(), m_latteView->width(), shownThickness);
        break;
    }

    case Plasma::Types::BottomEdge: {
        int y = m_latteView->y() + m_latteView->height() - shownThickness;
        calcs = QRect(m_latteView->x(), y, m_latteView->width(), shownThickness);
        break;
    }

    case Plasma::Types::LeftEdge: {
        calcs = QRect(m_latteView->x(), m_latteView->y(), shownThickness, m_latteView->height());
        break;
    }

    case Plasma::Types::RightEdge: {
        int x = m_latteView->x() + m_latteView->width() - shownThickness;
        calcs = QRect(x, m_latteView->y(), shownThickness, m_latteView->height());
        break;
    }
    }

    return calcs;
}

bool VisibilityManager::raiseOnDesktop() const
{
    return m_raiseOnDesktopChange;
}

void VisibilityManager::setRaiseOnDesktop(bool enable)
{
    if (enable == m_raiseOnDesktopChange)
        return;

    m_raiseOnDesktopChange = enable;
    emit raiseOnDesktopChanged();
}

bool VisibilityManager::raiseOnActivity() const
{
    return m_raiseOnActivityChange;
}

void VisibilityManager::setRaiseOnActivity(bool enable)
{
    if (enable == m_raiseOnActivityChange)
        return;

    m_raiseOnActivityChange = enable;
    emit raiseOnActivityChanged();
}

bool VisibilityManager::isBelowLayer() const
{
    return m_isBelowLayer;
}

void VisibilityManager::setIsBelowLayer(bool below)
{
    if (m_isBelowLayer == below) {
        return;
    }

    m_isBelowLayer = below;

    updateGhostWindowState();

    emit isBelowLayerChanged();
}

bool VisibilityManager::isHidden() const
{
    return m_isHidden;
}

void VisibilityManager::setIsHidden(bool isHidden)
{
    if (m_isHidden == isHidden)
        return;

    if (hidingIsBlocked() && isHidden) {
        qWarning() << "isHidden property is blocked, ignoring update";
        return;
    }

    m_isHidden = isHidden;

    updateGhostWindowState();

    emit isHiddenChanged();
}

bool VisibilityManager::hidingIsBlocked() const
{
    return (m_blockHidingEvents.count() > 0);
}


void VisibilityManager::addBlockHidingEvent(const QString &type)
{
    if (m_blockHidingEvents.contains(type) || type.isEmpty()) {
        return;
    }

    //qDebug() << "  {{ ++++ adding block hiding event :: " << type;

    bool prevHidingIsBlocked = hidingIsBlocked();

    m_blockHidingEvents << type;

    if (prevHidingIsBlocked != hidingIsBlocked()) {
        emit hidingIsBlockedChanged();
    }
}

void VisibilityManager::removeBlockHidingEvent(const QString &type)
{
    if (!m_blockHidingEvents.contains(type) || type.isEmpty()) {
        return;
    }
    //qDebug() << "  {{ ---- remove block hiding event :: " << type;

    bool prevHidingIsBlocked = hidingIsBlocked();

    m_blockHidingEvents.removeAll(type);

    if (prevHidingIsBlocked != hidingIsBlocked()) {
        emit hidingIsBlockedChanged();
    }
}

void VisibilityManager::on_hidingIsBlockedChanged()
{
    if (hidingIsBlocked()) {
        m_timerHide.stop();

        if (m_isHidden) {
            emit mustBeShown();
        }
    } else {
        updateHiddenState();
    }
}

void VisibilityManager::on_publishFrameExtents()
{
    if (!m_timerPublishFrameExtents.isActive()) {
        m_timerPublishFrameExtents.start();
    }
}

void VisibilityManager::publishFrameExtents(bool forceUpdate)
{
    if (m_frameExtentsHeadThicknessGap != m_latteView->headThicknessGap()
            || m_frameExtentsLocation != m_latteView->location()
            || forceUpdate) {

        m_frameExtentsHeadThicknessGap = m_latteView->headThicknessGap();
        m_frameExtentsLocation = m_latteView->location();

        QMargins frameExtents;

        if (m_latteView->location() == Plasma::Types::LeftEdge) {
            frameExtents.setRight(m_frameExtentsHeadThicknessGap);
        } else if (m_latteView->location() == Plasma::Types::TopEdge) {
            frameExtents.setBottom(m_frameExtentsHeadThicknessGap);
        } else if (m_latteView->location() == Plasma::Types::RightEdge) {
            frameExtents.setLeft(m_frameExtentsHeadThicknessGap);
        } else {
            frameExtents.setTop(m_frameExtentsHeadThicknessGap);
        }

        qDebug() << " -> Frame Extents :: " << m_frameExtentsLocation << " __ " << " extents :: " << frameExtents;

        if (!frameExtents.isNull()) {
            //! When a view returns its frame extents to zero then that triggers a compositor
            //! strange behavior that moves/hides the view totally and freezes entire Latte
            //! this is why we have blocked that setting
            m_wm->setFrameExtents(m_latteView, frameExtents);
        } else if (m_latteView->behaveAsPlasmaPanel()) {
            emit frameExtentsCleared();
        }
    }
}

int VisibilityManager::timerShow() const
{
    return m_timerShow.interval();
}

void VisibilityManager::setTimerShow(int msec)
{
    if (m_timerShow.interval() == msec) {
        return;
    }

    m_timerShow.setInterval(msec);
    emit timerShowChanged();
}

int VisibilityManager::timerHide() const
{
    return m_timerHide.interval();
}

void VisibilityManager::setTimerHide(int msec)
{
    int interval = qMax(HIDEMINIMUMINTERVAL, msec);

    if (m_timerHide.interval() == interval) {
        return;
    }

    m_timerHide.setInterval(interval);
    emit timerHideChanged();
}

bool VisibilityManager::supportsKWinEdges() const
{
    return (m_edgeGhostWindow != nullptr);
}

void VisibilityManager::updateGhostWindowState()
{
    if (supportsKWinEdges()) {
        bool inCurrentLayout = (m_corona->layoutsManager()->memoryUsage() == MemoryUsage::SingleLayout ||
                                (m_corona->layoutsManager()->memoryUsage() == MemoryUsage::MultipleLayouts
                                 && m_latteView->layout() && !m_latteView->positioner()->inLocationAnimation()
                                 && m_latteView->layout()->isCurrent()));

        if (inCurrentLayout) {
            if (m_mode == Latte::Types::WindowsCanCover) {
                m_wm->setActiveEdge(m_edgeGhostWindow, m_isBelowLayer && !m_containsMouse);
            } else {
                bool activated = (m_isHidden && !windowContainsMouse());

                m_wm->setActiveEdge(m_edgeGhostWindow, activated);
            }
        } else {
            m_wm->setActiveEdge(m_edgeGhostWindow, false);
        }
    }
}

void VisibilityManager::hide()
{
    if (KWindowSystem::isPlatformX11()) {
        m_lastMask = m_latteView->mask();
        m_latteView->setMask(QRect(-1, -1, 1, 1));
    } else {
        //! wayland case
        m_latteView->hide();
    }
}

void VisibilityManager::show()
{
    if (KWindowSystem::isPlatformX11()) {
        if (m_latteView->mask() == QRect(-1, -1 , 1, 1)) {
            m_latteView->setMask(m_lastMask);
        }
    } else {
        //! wayland case
        m_latteView->show();
    }
}


void VisibilityManager::raiseView(bool raise)
{
    if (hidingIsBlocked() || m_mode == Latte::Types::SideBar)
        return;

    if (raise) {
        m_timerHide.stop();

        if (!m_timerShow.isActive()) {
            m_timerShow.start();
        }
    } else if (!m_dragEnter) {
        m_timerShow.stop();

        if (m_hideNow) {
            m_hideNow = false;
            emit mustBeHide();
        } else if (!m_timerHide.isActive()) {
            m_timerHide.start();
        }
    }
}

void VisibilityManager::raiseViewTemporarily()
{
    if (m_raiseTemporarily)
        return;

    m_raiseTemporarily = true;
    m_timerHide.stop();
    m_timerShow.stop();

    if (m_isHidden)
        emit mustBeShown();

    QTimer::singleShot(qBound(1800, 2 * m_timerHide.interval(), 3000), this, [&]() {
        m_raiseTemporarily = false;
        m_hideNow = true;
        updateHiddenState();
    });
}

bool VisibilityManager::isValidMode() const
{
    return (m_mode != Types::None && m_mode != Types::NormalWindow);
}

void VisibilityManager::toggleHiddenState()
{
    if (!m_latteView->inEditMode()) {
        if (m_mode == Latte::Types::SideBar) {
            if (m_blockHidingEvents.contains(Q_FUNC_INFO)) {
                removeBlockHidingEvent(Q_FUNC_INFO);
            }

            if (m_isHidden) {
                emit mustBeShown();
            } else {
                emit mustBeHide();
            }
        } else {
            if (!m_blockHidingEvents.contains(Q_FUNC_INFO)) {
                addBlockHidingEvent(Q_FUNC_INFO);
            } else {
                removeBlockHidingEvent(Q_FUNC_INFO);
            }
        }
    }
}

void VisibilityManager::updateHiddenState()
{
    if (m_dragEnter)
        return;

    switch (m_mode) {
    case Types::AutoHide:
    case Types::WindowsCanCover:
        raiseView(m_containsMouse);
        break;

    case Types::DodgeActive:
        dodgeActive();
        break;

    case Types::DodgeMaximized:
        dodgeMaximized();
        break;

    case Types::DodgeAllWindows:
        dodgeAllWindows();
        break;

    default:
        break;
    }
}

void VisibilityManager::applyActivitiesToHiddenWindows(const QStringList &activities)
{
    if (m_edgeGhostWindow) {
        m_wm->setWindowOnActivities(*m_edgeGhostWindow, activities);
    }

    if (m_floatingGapWindow) {
        m_wm->setWindowOnActivities(*m_floatingGapWindow, activities);
    }
}

void VisibilityManager::dodgeActive()
{
    if (m_raiseTemporarily)
        return;

    //!don't send false raiseView signal when containing mouse
    if (m_containsMouse) {
        raiseView(true);
        return;
    }

    raiseView(!m_latteView->windowsTracker()->currentScreen()->activeWindowTouching());
}

void VisibilityManager::dodgeMaximized()
{
    if (m_raiseTemporarily)
        return;

    //!don't send false raiseView signal when containing mouse
    if (m_containsMouse) {
        raiseView(true);
        return;
    }

    raiseView(!m_latteView->windowsTracker()->currentScreen()->activeWindowMaximized());
}

void VisibilityManager::dodgeAllWindows()
{
    if (m_raiseTemporarily)
        return;

    if (m_containsMouse) {
        raiseView(true);
        return;
    }

    bool windowIntersects{m_latteView->windowsTracker()->currentScreen()->activeWindowTouching() || m_latteView->windowsTracker()->currentScreen()->existsWindowTouching()};

    raiseView(!windowIntersects);
}

void VisibilityManager::saveConfig()
{
    if (!m_latteView->containment())
        return;

    auto config = m_latteView->containment()->config();

    config.writeEntry("enableKWinEdges", m_enableKWinEdgesFromUser);
    config.writeEntry("timerShow", m_timerShow.interval());
    config.writeEntry("timerHide", m_timerHide.interval());
    config.writeEntry("raiseOnDesktopChange", m_raiseOnDesktopChange);
    config.writeEntry("raiseOnActivityChange", m_raiseOnActivityChange);

    m_latteView->containment()->configNeedsSaving();
}

void VisibilityManager::restoreConfig()
{
    if (!m_latteView || !m_latteView->containment()){
        return;
    }

    auto config = m_latteView->containment()->config();
    m_timerShow.setInterval(config.readEntry("timerShow", 0));
    m_timerHide.setInterval(qMax(HIDEMINIMUMINTERVAL, config.readEntry("timerHide", 700)));
    emit timerShowChanged();
    emit timerHideChanged();

    m_enableKWinEdgesFromUser = config.readEntry("enableKWinEdges", true);
    emit enableKWinEdgesChanged();

    setRaiseOnDesktop(config.readEntry("raiseOnDesktopChange", false));
    setRaiseOnActivity(config.readEntry("raiseOnActivityChange", false));

    auto storedMode = (Types::Visibility)(m_latteView->containment()->config().readEntry("visibility", (int)(Types::DodgeActive)));

    if (storedMode == Types::AlwaysVisible) {
        qDebug() << "Loading visibility mode: Always Visible , on startup...";
        setMode(Types::AlwaysVisible);
    } else {
        connect(&m_timerStartUp, &QTimer::timeout, this, [&]() {
            if (!m_latteView || !m_latteView->containment()) {
                return;
            }

            Types::Visibility fMode = (Types::Visibility)(m_latteView->containment()->config().readEntry("visibility", (int)(Types::DodgeActive)));
            qDebug() << "Loading visibility mode:" << fMode << " on startup...";
            setMode(fMode);
        });
        connect(m_latteView->containment(), &Plasma::Containment::userConfiguringChanged
                , this, [&](bool configuring) {
            if (configuring && m_timerStartUp.isActive())
                m_timerStartUp.start(100);
        });

        m_timerStartUp.start();
    }

    connect(m_latteView->containment(), &Plasma::Containment::userConfiguringChanged
            , this, [&](bool configuring) {
        if (!configuring) {
            saveConfig();
        }
    });
}

bool VisibilityManager::containsMouse() const
{
    return m_containsMouse;
}

void VisibilityManager::setContainsMouse(bool contains)
{
    if (m_containsMouse == contains) {
        return;
    }

    m_containsMouse = contains;
    emit containsMouseChanged();
}

bool VisibilityManager::windowContainsMouse()
{
    return m_containsMouse || (m_edgeGhostWindow && m_edgeGhostWindow->containsMouse());
}

void VisibilityManager::checkMouseInFloatingArea()
{
    if (m_latteView->isFloatingWindow()) {
        if (!m_floatingGapWindow) {
            createFloatingGapWindow();
        }

        m_floatingGapWindow->callAsyncContainsMouse();
    }
}

void VisibilityManager::viewEventManager(QEvent *ev)
{
    switch (ev->type()) {
    case QEvent::Enter:
        setContainsMouse(true);
        break;

    case QEvent::Leave:
        m_dragEnter = false;
        setContainsMouse(false);
        break;

    case QEvent::DragEnter:
        m_dragEnter = true;

        if (m_isHidden && m_mode != Latte::Types::SideBar) {
            emit mustBeShown();
        }

        break;

    case QEvent::DragLeave:
    case QEvent::Drop:
        m_dragEnter = false;
        updateHiddenState();
        break;

    default:
        break;
    }
}

//! KWin Edges Support functions
bool VisibilityManager::enableKWinEdges() const
{
    return m_enableKWinEdgesFromUser;
}

void VisibilityManager::setEnableKWinEdges(bool enable)
{
    if (m_enableKWinEdgesFromUser == enable) {
        return;
    }

    m_enableKWinEdgesFromUser = enable;

    emit enableKWinEdgesChanged();
}

void VisibilityManager::updateKWinEdgesSupport()
{
    if ((m_mode == Types::AutoHide
         || m_mode == Types::DodgeActive
         || m_mode == Types::DodgeAllWindows
         || m_mode == Types::DodgeMaximized)
            && !m_latteView->byPassWM()) {

        if (m_enableKWinEdgesFromUser || m_latteView->behaveAsPlasmaPanel()) {
            createEdgeGhostWindow();
            if (m_latteView->isFloatingWindow()) {
                createFloatingGapWindow();
            }
        } else if (!m_enableKWinEdgesFromUser) {
            deleteEdgeGhostWindow();
            deleteFloatingGapWindow();
        }

    } else if (m_mode == Types::WindowsCanCover) {
        createEdgeGhostWindow();
    } else {
        deleteEdgeGhostWindow();
        deleteFloatingGapWindow();
    }
}

void VisibilityManager::createEdgeGhostWindow()
{
    if (!m_edgeGhostWindow) {
        m_edgeGhostWindow = new ScreenEdgeGhostWindow(m_latteView);

        connect(m_edgeGhostWindow, &ScreenEdgeGhostWindow::containsMouseChanged, this, [ = ](bool contains) {
            if (contains) {
                raiseView(true);
            } else {
                m_timerShow.stop();
                updateGhostWindowState();
            }
        });

        connect(m_edgeGhostWindow, &ScreenEdgeGhostWindow::dragEntered, this, [&]() {
            if (m_isHidden) {
                emit mustBeShown();
            }
        });

        m_connectionsKWinEdges[0] = connect(m_wm, &WindowSystem::AbstractWindowInterface::currentActivityChanged,
                                            this, [&]() {
            bool inCurrentLayout = (m_corona->layoutsManager()->memoryUsage() == MemoryUsage::SingleLayout ||
                                    (m_corona->layoutsManager()->memoryUsage() == MemoryUsage::MultipleLayouts
                                     && m_latteView->layout() && !m_latteView->positioner()->inLocationAnimation()
                                     && m_latteView->layout()->isCurrent()));

            if (m_edgeGhostWindow) {
                if (inCurrentLayout) {
                    m_wm->setActiveEdge(m_edgeGhostWindow, m_isHidden);
                } else {
                    m_wm->setActiveEdge(m_edgeGhostWindow, false);
                }
            }
        });

        emit supportsKWinEdgesChanged();
    }
}

void VisibilityManager::deleteEdgeGhostWindow()
{
    if (m_edgeGhostWindow) {
        m_edgeGhostWindow->deleteLater();
        m_edgeGhostWindow = nullptr;

        for (auto &c : m_connectionsKWinEdges) {
            disconnect(c);
        }

        emit supportsKWinEdgesChanged();
    }
}

void VisibilityManager::createFloatingGapWindow()
{
    if (!m_floatingGapWindow) {
        m_floatingGapWindow = new FloatingGapWindow(m_latteView);

        connect(m_floatingGapWindow, &FloatingGapWindow::asyncContainsMouseChanged, this, [ = ](bool contains) {
            if (contains) {
                if (m_latteView->isFloatingWindow() && !m_isHidden) {
                    //! immediate call after contains mouse checks for mouse in sensitive floating areas
                    updateHiddenState();
                }
            } else {
                if (m_latteView->isFloatingWindow() && !m_isHidden) {
                    //! immediate call after contains mouse checks for mouse in sensitive floating areas
                    emit mustBeHide();
                }
            }
        });
    }
}

void VisibilityManager::deleteFloatingGapWindow()
{
    if (m_floatingGapWindow) {
        m_floatingGapWindow->deleteLater();
        m_floatingGapWindow = nullptr;
    }
}

bool VisibilityManager::supportsFloatingGap() const
{
    return (m_floatingGapWindow != nullptr);
}


//! END: VisibilityManager implementation

}
}
