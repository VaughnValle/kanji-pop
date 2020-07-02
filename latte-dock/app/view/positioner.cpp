/*
*  Copyright 2018  Michail Vourlakos <mvourlakos@gmail.com>
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

#include "positioner.h"

// local
#include <coretypes.h>
#include "effects.h"
#include "view.h"
#include "visibilitymanager.h"
#include "../lattecorona.h"
#include "../screenpool.h"
#include "../settings/universalsettings.h"

// Qt
#include <QDebug>

// KDE
#include <KWayland/Client/plasmashell.h>
#include <KWayland/Client/surface.h>
#include <KWindowSystem>


namespace Latte {
namespace ViewPart {

Positioner::Positioner(Latte::View *parent)
    : QObject(parent),
      m_view(parent)
{
    m_screenSyncTimer.setSingleShot(true);
    m_screenSyncTimer.setInterval(2000);
    connect(&m_screenSyncTimer, &QTimer::timeout, this, &Positioner::reconsiderScreen);

    //! under X11 it was identified that windows many times especially under screen changes
    //! don't end up at the correct position and size. This timer will enforce repositionings
    //! and resizes every 500ms if the window hasn't end up to correct values and until this
    //! is achieved
    m_validateGeometryTimer.setSingleShot(true);
    m_validateGeometryTimer.setInterval(500);
    connect(&m_validateGeometryTimer, &QTimer::timeout, this, &Positioner::syncGeometry);

    m_corona = qobject_cast<Latte::Corona *>(m_view->corona());

    if (m_corona) {
        if (KWindowSystem::isPlatformX11()) {
            m_trackedWindowId = m_view->winId();
            m_corona->wm()->registerIgnoredWindow(m_trackedWindowId);

            connect(m_view, &Latte::View::forcedShown, this, [&]() {
                m_corona->wm()->unregisterIgnoredWindow(m_trackedWindowId);
                m_trackedWindowId = m_view->winId();
                m_corona->wm()->registerIgnoredWindow(m_trackedWindowId);
            });
        } else {
            connect(m_view, &QWindow::windowTitleChanged, this, &Positioner::updateWaylandId);
            connect(m_corona->wm(), &WindowSystem::AbstractWindowInterface::latteWindowAdded, this, &Positioner::updateWaylandId);
        }

        /////

        m_screenSyncTimer.setInterval(qMax(m_corona->universalSettings()->screenTrackerInterval() - 500, 1000));
        connect(m_corona->universalSettings(), &UniversalSettings::screenTrackerIntervalChanged, this, [&]() {
            m_screenSyncTimer.setInterval(qMax(m_corona->universalSettings()->screenTrackerInterval() - 500, 1000));
        });

        connect(m_corona, &Latte::Corona::viewLocationChanged, this, [&]() {
            //! check if an edge has been freed for a primary dock
            //! from another screen
            if (m_view->onPrimary()) {
                m_screenSyncTimer.start();
            }
        });
    }

    init();
}

Positioner::~Positioner()
{
    m_inDelete = true;
    m_corona->wm()->unregisterIgnoredWindow(m_trackedWindowId);

    m_screenSyncTimer.stop();
    m_validateGeometryTimer.stop();
}

void Positioner::init()
{
    //! connections
    connect(this, &Positioner::screenGeometryChanged, this, &Positioner::syncGeometry);

    connect(this, &Positioner::hideDockDuringLocationChangeStarted, this, &Positioner::updateInLocationAnimation);
    connect(this, &Positioner::hideDockDuringScreenChangeStarted, this, &Positioner::updateInLocationAnimation);
    connect(this, &Positioner::hideDockDuringMovingToLayoutStarted, this, &Positioner::updateInLocationAnimation);
    connect(this, &Positioner::showDockAfterLocationChangeFinished, this, &Positioner::updateInLocationAnimation);
    connect(this, &Positioner::showDockAfterScreenChangeFinished, this, &Positioner::updateInLocationAnimation);
    connect(this, &Positioner::showDockAfterMovingToLayoutFinished, this, &Positioner::updateInLocationAnimation);

    connect(this, &Positioner::showDockAfterLocationChangeFinished, this, &Positioner::syncLatteViews);
    connect(this, &Positioner::showDockAfterScreenChangeFinished, this, &Positioner::syncLatteViews);
    connect(this, &Positioner::showDockAfterMovingToLayoutFinished, this, &Positioner::syncLatteViews);
    connect(m_view, &Latte::View::onPrimaryChanged, this, &Positioner::syncLatteViews);

    connect(this, &Positioner::isStickedOnTopEdgeChanged, this, [&]() {
        if (m_view->formFactor() == Plasma::Types::Vertical) {
            syncGeometry();
        }
    });

    connect(this, &Positioner::isStickedOnBottomEdgeChanged, this, [&]() {
        if (m_view->formFactor() == Plasma::Types::Vertical) {
            syncGeometry();
        }
    });

    connect(this, &Positioner::slideOffsetChanged, this, [&]() {
        if (m_slideOffset == 0 && m_view->inEditMode()) {
            syncGeometry();
        }
    });

    connect(m_view, &QQuickWindow::xChanged, this, &Positioner::validateDockGeometry);
    connect(m_view, &QQuickWindow::yChanged, this, &Positioner::validateDockGeometry);
    connect(m_view, &QQuickWindow::widthChanged, this, &Positioner::validateDockGeometry);
    connect(m_view, &QQuickWindow::heightChanged, this, &Positioner::validateDockGeometry);
    connect(m_view, &QQuickWindow::screenChanged, this, &Positioner::currentScreenChanged);
    connect(m_view, &QQuickWindow::screenChanged, this, &Positioner::screenChanged);

    connect(m_view, &Latte::View::behaveAsPlasmaPanelChanged, this, &Positioner::syncGeometry);
    connect(m_view, &Latte::View::maxThicknessChanged, this, &Positioner::syncGeometry);
    connect(m_view, &Latte::View::maxLengthChanged, this, &Positioner::syncGeometry);
    connect(m_view, &Latte::View::offsetChanged, this, &Positioner::syncGeometry);

    connect(m_view, &Latte::View::locationChanged, this, [&]() {
        updateFormFactor();
        syncGeometry();
    });

    connect(m_view, &Latte::View::normalThicknessChanged, this, [&]() {
        if (m_view->behaveAsPlasmaPanel()) {
            syncGeometry();
        }
    });

    connect(m_view, &Latte::View::screenEdgeMarginEnabledChanged, this, [&]() {
        if (m_view->inEditMode() ) {
            syncGeometry();
        }
    });

    connect(m_view, &Latte::View::screenEdgeMarginChanged, this, [&]() {
        if (m_view->screenEdgeMarginEnabled() && m_view->inEditMode()) {
            syncGeometry();
        }
    });

    connect(m_view->effects(), &Latte::ViewPart::Effects::drawShadowsChanged, this, [&]() {
        if (!m_view->behaveAsPlasmaPanel()) {
            syncGeometry();
        }
    });

    connect(m_view->effects(), &Latte::ViewPart::Effects::innerShadowChanged, this, [&]() {
        if (m_view->behaveAsPlasmaPanel()) {
            syncGeometry();
        }
    });

    connect(qGuiApp, &QGuiApplication::screenAdded, this, &Positioner::screenChanged);
    connect(qGuiApp, &QGuiApplication::primaryScreenChanged, this, &Positioner::screenChanged);

    connect(m_view, &Latte::View::visibilityChanged, this, &Positioner::initDelayedSignals);

    initSignalingForLocationChangeSliding();
}

void Positioner::initDelayedSignals()
{
    connect(m_view->visibility(), &ViewPart::VisibilityManager::isHiddenChanged, this, [&]() {
        if (m_view->behaveAsPlasmaPanel() && !m_view->visibility()->isHidden() && qAbs(m_slideOffset)>0) {
            //! ignore any checks to make sure the panel geometry is up-to-date
            immediateSyncGeometry();
        }
    });
}

void Positioner::updateWaylandId()
{
    QString validTitle = m_view->validTitle();
    if (validTitle.isEmpty()) {
        return;
    }

    Latte::WindowSystem::WindowId newId = m_corona->wm()->winIdFor("latte-dock", validTitle);

    if (m_trackedWindowId != newId) {
        if (!m_trackedWindowId.isNull()) {
            m_corona->wm()->unregisterIgnoredWindow(m_trackedWindowId);
        }

        m_trackedWindowId = newId;
        m_corona->wm()->registerIgnoredWindow(m_trackedWindowId);
    }
}

int Positioner::currentScreenId() const
{
    auto *latteCorona = qobject_cast<Latte::Corona *>(m_view->corona());

    if (latteCorona) {
        return latteCorona->screenPool()->id(m_screenToFollowId);
    }

    return -1;
}

Latte::WindowSystem::WindowId Positioner::trackedWindowId()
{
    return m_trackedWindowId;
}

QString Positioner::currentScreenName() const
{
    return m_screenToFollowId;
}

void Positioner::syncLatteViews()
{
    if (m_view->layout()) {
        //! This is needed in case the edge there are views that must be deleted
        //! after screen edges changes
        m_view->layout()->syncLatteViewsToScreens();
    }
}

void Positioner::updateContainmentScreen()
{
    if (m_view->containment()) {
        m_view->containment()->reactToScreenChange();
    }
}

bool Positioner::setCurrentScreen(const QString id)
{
    QScreen *nextScreen{qGuiApp->primaryScreen()};

    if (id != "primary") {
        for (const auto scr : qGuiApp->screens()) {
            if (scr && scr->name() == id) {
                nextScreen = scr;
                break;
            }
        }
    }

    if (m_screenToFollow == nextScreen) {
        updateContainmentScreen();
        return true;
    }

    if (nextScreen) {
        if (m_view->layout()) {
            m_goToScreen = nextScreen;

            //! asynchronous call in order to not crash from configwindow
            //! deletion from sliding out animation
            QTimer::singleShot(100, [this]() {
                emit hideDockDuringScreenChangeStarted();
            });
        }
    }

    return true;
}

//! this function updates the dock's associated screen.
//! updateScreenId = true, update also the m_screenToFollowId
//! updateScreenId = false, do not update the m_screenToFollowId
//! that way an explicit dock can be shown in another screen when
//! there isnt a tasks dock running in the system and for that
//! dock its first origin screen is stored and that way when
//! that screen is reconnected the dock will return to its original
//! place
void Positioner::setScreenToFollow(QScreen *scr, bool updateScreenId)
{
    if (!scr || (scr && (m_screenToFollow == scr) && (m_view->screen() == scr))) {
        return;
    }

    qDebug() << "setScreenToFollow() called for screen:" << scr->name() << " update:" << updateScreenId;

    m_screenToFollow = scr;

    if (updateScreenId) {
        m_screenToFollowId = scr->name();
    }

    qDebug() << "adapting to screen...";
    m_view->setScreen(scr);

    updateContainmentScreen();

    connect(scr, &QScreen::geometryChanged, this, &Positioner::screenGeometryChanged);
    syncGeometry();
    m_view->updateAbsoluteGeometry(true);
    qDebug() << "setScreenToFollow() ended...";

    emit screenGeometryChanged();
    emit currentScreenChanged();
}

//! the main function which decides if this dock is at the
//! correct screen
void Positioner::reconsiderScreen()
{
    if (m_inDelete) {
        return;
    }

    qDebug() << "reconsiderScreen() called...";
    qDebug() << "  Delayer  ";

    for (const auto scr : qGuiApp->screens()) {
        qDebug() << "      D, found screen: " << scr->name();
    }

    bool screenExists{false};

    //!check if the associated screen is running
    for (const auto scr : qGuiApp->screens()) {
        if (m_screenToFollowId == scr->name()
                || (m_view->onPrimary() && scr == qGuiApp->primaryScreen())) {
            screenExists = true;
        }
    }

    qDebug() << "dock screen exists  ::: " << screenExists;

    //! 1.a primary dock must be always on the primary screen
    if (m_view->onPrimary() && (m_screenToFollowId != qGuiApp->primaryScreen()->name()
                                || m_screenToFollow != qGuiApp->primaryScreen()
                                || m_view->screen() != qGuiApp->primaryScreen())) {
        //! case 1
        qDebug() << "reached case 1: of updating dock primary screen...";
        setScreenToFollow(qGuiApp->primaryScreen());
    } else if (!m_view->onPrimary()) {
        //! 2.an explicit dock must be always on the correct associated screen
        //! there are cases that window manager misplaces the dock, this function
        //! ensures that this dock will return at its correct screen
        for (const auto scr : qGuiApp->screens()) {
            if (scr && scr->name() == m_screenToFollowId) {
                qDebug() << "reached case 2: updating the explicit screen for dock...";
                setScreenToFollow(scr);
                break;
            }
        }
    }

    syncGeometry();
    qDebug() << "reconsiderScreen() ended...";
}

void Positioner::screenChanged(QScreen *scr)
{
    m_screenSyncTimer.start();

    //! this is needed in order to update the struts on screen change
    //! and even though the geometry has been set correctly the offsets
    //! of the screen must be updated to the new ones
    if (m_view->visibility() && m_view->visibility()->mode() == Latte::Types::AlwaysVisible) {
        m_view->updateAbsoluteGeometry(true);
    }
}

void Positioner::syncGeometry()
{
    if (!(m_view->screen() && m_view->containment()) || m_inDelete || m_slideOffset!=0 || inSlideAnimation()) {
        return;
    }

    qDebug() << "syncGeometry() called...";

    immediateSyncGeometry();
}

void Positioner::immediateSyncGeometry()
{
    bool found{false};

    qDebug() << "immediateSyncGeometry() called...";

    //! before updating the positioning and geometry of the dock
    //! we make sure that the dock is at the correct screen
    if (m_view->screen() != m_screenToFollow) {
        qDebug() << "Sync Geometry screens inconsistent!!!! ";

        if (m_screenToFollow) {
            qDebug() << "Sync Geometry screens inconsistent for m_screenToFollow:" << m_screenToFollow->name() << " dock screen:" << m_view->screen()->name();
        }

        if (!m_screenSyncTimer.isActive()) {
            m_screenSyncTimer.start();
        }
    } else {
        found = true;
    }

    //! if the dock isnt at the correct screen the calculations
    //! are not executed
    if (found) {
        //! compute the free screen rectangle for vertical panels only once
        //! this way the costly QRegion computations are calculated only once
        //! instead of two times (both inside the resizeWindow and the updatePosition)
        QRegion freeRegion;;
        QRect maximumRect;
        QRect availableScreenRect{m_view->screen()->geometry()};

        if (m_view->formFactor() == Plasma::Types::Vertical) {
            QString layoutName = m_view->layout() ? m_view->layout()->name() : QString();
            auto latteCorona = qobject_cast<Latte::Corona *>(m_view->corona());
            int fixedScreen = m_view->onPrimary() ? latteCorona->screenPool()->primaryScreenId() : m_view->containment()->screen();

            QList<Types::Visibility> ignoreModes({Latte::Types::AutoHide,
                                                  Latte::Types::SideBar});

            QList<Plasma::Types::Location> ignoreEdges({Plasma::Types::LeftEdge,
                                                        Plasma::Types::RightEdge});

            if (m_isStickedOnTopEdge && m_isStickedOnBottomEdge) {
                //! dont send an empty edges array because that means include all screen edges in calculations
                ignoreEdges << Plasma::Types::TopEdge;
                ignoreEdges << Plasma::Types::BottomEdge;
            } else {
                if (m_isStickedOnTopEdge) {
                    ignoreEdges << Plasma::Types::TopEdge;
                }

                if (m_isStickedOnBottomEdge) {
                    ignoreEdges << Plasma::Types::BottomEdge;
                }
            }

            freeRegion = latteCorona->availableScreenRegionWithCriteria(fixedScreen, layoutName, ignoreModes, ignoreEdges);

            maximumRect = maximumNormalGeometry();
            QRegion availableRegion = freeRegion.intersected(maximumRect);

            availableScreenRect = freeRegion.intersected(maximumRect).boundingRect();
            float area = 0;

            //! it is used to choose which or the availableRegion rectangles will
            //! be the one representing dock geometry
            for (QRegion::const_iterator p_rect=availableRegion.begin(); p_rect!=availableRegion.end(); ++p_rect) {
                //! the area of each rectangle in calculated in squares of 50x50
                //! this is a way to avoid enourmous numbers for area value
                float tempArea = (float)((*p_rect).width() * (*p_rect).height()) / 2500;

                if (tempArea > area) {
                    availableScreenRect = (*p_rect);
                    area = tempArea;
                }
            }

            validateTopBottomBorders(availableScreenRect, freeRegion);
        } else {
            m_view->effects()->setForceTopBorder(false);
            m_view->effects()->setForceBottomBorder(false);
        }

        m_view->effects()->updateEnabledBorders();
        resizeWindow(availableScreenRect);
        updatePosition(availableScreenRect);

        qDebug() << "syncGeometry() calculations for screen: " << m_view->screen()->name() << " _ " << m_view->screen()->geometry();
        qDebug() << "syncGeometry() calculations for edge: " << m_view->location();
    }

    qDebug() << "syncGeometry() ended...";

    // qDebug() << "dock geometry:" << qRectToStr(geometry());
}

void Positioner::validateDockGeometry()
{
    if (m_slideOffset==0 && m_view->geometry() != m_validGeometry) {
        m_validateGeometryTimer.start();
    }
}

//! this is used mainly from vertical panels in order to
//! to get the maximum geometry that can be used from the dock
//! based on their alignment type and the location dock
QRect Positioner::maximumNormalGeometry()
{
    int xPos = 0;
    int yPos = m_view->screen()->geometry().y();;
    int maxHeight = m_view->screen()->geometry().height();
    int maxWidth = m_view->normalThickness();
    QRect maxGeometry;
    maxGeometry.setRect(0, 0, maxWidth, maxHeight);

    switch (m_view->location()) {
    case Plasma::Types::LeftEdge:
        xPos = m_view->screen()->geometry().x();
        maxGeometry.setRect(xPos, yPos, maxWidth, maxHeight);
        break;

    case Plasma::Types::RightEdge:
        xPos = m_view->screen()->geometry().right() - maxWidth + 1;
        maxGeometry.setRect(xPos, yPos, maxWidth, maxHeight);
        break;

    default:
        //! bypass clang warnings
        break;
    }

    return maxGeometry;
}

void Positioner::validateTopBottomBorders(QRect availableScreenRect, QRegion availableScreenRegion)
{
    //! Check if the the top/bottom borders must be drawn also
    int edgeMargin = qMax(1, m_view->screenEdgeMargin());

    if (availableScreenRect.top() != m_view->screenGeometry().top()) {
        //! check top border
        QRegion fitInRegion = QRect(m_view->screenGeometry().x(), availableScreenRect.y()-1, edgeMargin, 1);
        QRegion subtracted = fitInRegion.subtracted(availableScreenRegion);

        if (subtracted.isNull()) {
            //!FitIn rectangle fits TOTALLY in the free screen region and as such
            //!the top border should be drawn
            m_view->effects()->setForceTopBorder(true);
        } else {
            m_view->effects()->setForceTopBorder(false);
        }
    } else {
        m_view->effects()->setForceTopBorder(false);
    }

    if (availableScreenRect.bottom() != m_view->screenGeometry().bottom()) {
        //! check top border
        QRegion fitInRegion = QRect(m_view->screenGeometry().x(), availableScreenRect.bottom()+1, edgeMargin, 1);
        QRegion subtracted = fitInRegion.subtracted(availableScreenRegion);

        if (subtracted.isNull()) {
            //!FitIn rectangle fits TOTALLY in the free screen region and as such
            //!the BOTTOM border should be drawn
            m_view->effects()->setForceBottomBorder(true);
        } else {
            m_view->effects()->setForceBottomBorder(false);
        }
    } else {
        m_view->effects()->setForceBottomBorder(false);
    }
}

void Positioner::updatePosition(QRect availableScreenRect)
{
    QRect screenGeometry{availableScreenRect};
    QPoint position;
    position = {0, 0};

    const auto gap = [&](int scr_length) -> int {
        return static_cast<int>(scr_length * m_view->offset());
    };
    const auto gapCentered = [&](int scr_length) -> int {
        return static_cast<int>(scr_length * ((1 - m_view->maxLength()) / 2) + scr_length * m_view->offset());
    };
    const auto gapReversed = [&](int scr_length) -> int {
        return static_cast<int>(scr_length - (scr_length * m_view->maxLength()) - gap(scr_length));
    };

    int cleanThickness = m_view->normalThickness() - m_view->effects()->innerShadow();

    int screenEdgeMargin = m_view->behaveAsPlasmaPanel() ? m_view->screenEdgeMargin() - qAbs(m_slideOffset) : 0;

    switch (m_view->location()) {
    case Plasma::Types::TopEdge:
        if (m_view->behaveAsPlasmaPanel()) {
            int y = screenGeometry.y() + screenEdgeMargin;

            if (m_view->alignment() == Latte::Types::Left) {
                position = {screenGeometry.x() + gap(screenGeometry.width()), y};
            } else if (m_view->alignment() == Latte::Types::Right) {
                position = {screenGeometry.x() + gapReversed(screenGeometry.width()) + 1, y};
            } else {
                position = {screenGeometry.x() + gapCentered(screenGeometry.width()), y};
            }
        } else {
            position = {screenGeometry.x(), screenGeometry.y()};
        }

        break;

    case Plasma::Types::BottomEdge:
        if (m_view->behaveAsPlasmaPanel()) {
            int y = screenGeometry.y() + screenGeometry.height() - cleanThickness - screenEdgeMargin;

            if (m_view->alignment() == Latte::Types::Left) {
                position = {screenGeometry.x() + gap(screenGeometry.width()), y};
            } else if (m_view->alignment() == Latte::Types::Right) {
                position = {screenGeometry.x() + gapReversed(screenGeometry.width()) + 1, y};
            } else {
                position = {screenGeometry.x() + gapCentered(screenGeometry.width()), y};
            }
        } else {
            position = {screenGeometry.x(), screenGeometry.y() + screenGeometry.height() - m_view->height()};
        }

        break;

    case Plasma::Types::RightEdge:
        if (m_view->behaveAsPlasmaPanel()) {
            int x = availableScreenRect.right() - cleanThickness + 1 - screenEdgeMargin;

            if (m_view->alignment() == Latte::Types::Top) {
                position = {x, availableScreenRect.y() + gap(availableScreenRect.height())};
            } else if (m_view->alignment() == Latte::Types::Bottom) {
                position = {x, availableScreenRect.y() + gapReversed(availableScreenRect.height()) + 1};
            } else {
                position = {x, availableScreenRect.y() + gapCentered(availableScreenRect.height())};
            }
        } else {
            position = {availableScreenRect.right() - m_view->width() + 1, availableScreenRect.y()};
        }

        break;

    case Plasma::Types::LeftEdge:
        if (m_view->behaveAsPlasmaPanel()) {
            int x = availableScreenRect.x() + screenEdgeMargin;

            if (m_view->alignment() == Latte::Types::Top) {
                position = {x, availableScreenRect.y() + gap(availableScreenRect.height())};
            } else if (m_view->alignment() == Latte::Types::Bottom) {
                position = {x, availableScreenRect.y() + gapReversed(availableScreenRect.height()) + 1};
            } else {
                position = {x, availableScreenRect.y() + gapCentered(availableScreenRect.height())};
            }
        } else {
            position = {availableScreenRect.x(), availableScreenRect.y()};
        }

        break;

    default:
        qWarning() << "wrong location, couldn't update the panel position"
                   << m_view->location();
    }

    if (m_slideOffset == 0) {
        //! update valid geometry in normal positioning
        m_validGeometry.setTopLeft(position);
    } else {
        //! when sliding in/out update only the relevant axis for the screen_edge in
        //! to not mess the calculations and the automatic geometry checkers that
        //! View::Positioner is using.
        if (m_view->formFactor() == Plasma::Types::Horizontal) {
            m_validGeometry.moveLeft(position.x());
        } else {
            m_validGeometry.moveTop(position.y());
        }
    }

    m_view->setPosition(position);

    if (m_view->surface()) {
        m_view->surface()->setPosition(position);
    }
}

int Positioner::slideOffset() const
{
    return m_slideOffset;
}

void Positioner::setSlideOffset(int offset)
{
    if (m_slideOffset == offset) {
        return;
    }

    m_slideOffset = offset;

    QPoint slidedTopLeft;

    if (m_view->location() == Plasma::Types::TopEdge) {
        int boundedY = qMax(m_view->screenGeometry().top() - (m_validGeometry.height() + 1), m_validGeometry.y() - qAbs(m_slideOffset));
        slidedTopLeft = {m_validGeometry.x(), boundedY};

    } else if (m_view->location() == Plasma::Types::BottomEdge) {
        int boundedY = qMin(m_view->screenGeometry().bottom() - 1, m_validGeometry.y() + qAbs(m_slideOffset));
        slidedTopLeft = {m_validGeometry.x(), boundedY};

    } else if (m_view->location() == Plasma::Types::RightEdge) {
        int boundedX = qMin(m_view->screenGeometry().right() - 1, m_validGeometry.x() + qAbs(m_slideOffset));
        slidedTopLeft = {boundedX, m_validGeometry.y()};

    } else if (m_view->location() == Plasma::Types::LeftEdge) {
        int boundedX = qMax(m_view->screenGeometry().left() - (m_validGeometry.width() + 1), m_validGeometry.x() - qAbs(m_slideOffset));
        slidedTopLeft = {boundedX, m_validGeometry.y()};

    }

    m_view->setPosition(slidedTopLeft);

    if (m_view->surface()) {
        m_view->surface()->setPosition(slidedTopLeft);
    }

    emit slideOffsetChanged();
}


void Positioner::resizeWindow(QRect availableScreenRect)
{
    QSize screenSize = m_view->screen()->size();
    QSize size = (m_view->formFactor() == Plasma::Types::Vertical) ? QSize(m_view->maxThickness(), availableScreenRect.height()) : QSize(screenSize.width(), m_view->maxThickness());

    if (m_view->formFactor() == Plasma::Types::Vertical) {
        //qDebug() << "MAXIMUM RECT :: " << maximumRect << " - AVAILABLE RECT :: " << availableRect;
        if (m_view->behaveAsPlasmaPanel()) {
            size.setWidth(m_view->normalThickness());
            size.setHeight(static_cast<int>(m_view->maxLength() * availableScreenRect.height()));
        }
    } else {
        if (m_view->behaveAsPlasmaPanel()) {
            size.setWidth(static_cast<int>(m_view->maxLength() * screenSize.width()));
            size.setHeight(m_view->normalThickness());
        }
    }

    m_validGeometry.setSize(size);

    m_view->setMinimumSize(size);
    m_view->setMaximumSize(size);
    m_view->resize(size);

    if (m_view->formFactor() == Plasma::Types::Horizontal) {
        emit windowSizeChanged();
    }
}

void Positioner::updateFormFactor()
{
    if (!m_view->containment())
        return;

    switch (m_view->location()) {
    case Plasma::Types::TopEdge:
    case Plasma::Types::BottomEdge:
        m_view->containment()->setFormFactor(Plasma::Types::Horizontal);
        break;

    case Plasma::Types::LeftEdge:
    case Plasma::Types::RightEdge:
        m_view->containment()->setFormFactor(Plasma::Types::Vertical);
        break;

    default:
        qWarning() << "wrong location, couldn't update the panel position" << m_view->location();
    }
}

void Positioner::initSignalingForLocationChangeSliding()
{
    //! signals to handle the sliding-in/out during location changes
    connect(this, &Positioner::hideDockDuringLocationChangeStarted, this, &Positioner::onHideWindowsForSlidingOut);

    connect(m_view, &View::locationChanged, this, [&]() {
        if (m_goToLocation != Plasma::Types::Floating) {
            m_goToLocation = Plasma::Types::Floating;
            QTimer::singleShot(100, [this]() {
                m_view->effects()->setAnimationsBlocked(false);
                emit showDockAfterLocationChangeFinished();
                m_view->showSettingsWindow();
                emit edgeChanged();
            });
        }
    });

    //! signals to handle the sliding-in/out during screen changes
    connect(this, &Positioner::hideDockDuringScreenChangeStarted, this, &Positioner::onHideWindowsForSlidingOut);

    connect(this, &Positioner::currentScreenChanged, this, [&]() {
        if (m_goToScreen) {
            m_goToScreen = nullptr;
            QTimer::singleShot(100, [this]() {
                m_view->effects()->setAnimationsBlocked(false);
                emit showDockAfterScreenChangeFinished();
                m_view->showSettingsWindow();
                emit edgeChanged();
            });
        }
    });

    //! signals to handle the sliding-in/out during moving to another layout
    connect(this, &Positioner::hideDockDuringMovingToLayoutStarted, this, &Positioner::onHideWindowsForSlidingOut);

    connect(m_view, &View::layoutChanged, this, [&]() {
        if (!m_moveToLayout.isEmpty() && m_view->layout()) {
            m_moveToLayout = "";
            QTimer::singleShot(100, [this]() {
                m_view->effects()->setAnimationsBlocked(false);
                emit showDockAfterMovingToLayoutFinished();
                m_view->showSettingsWindow();
                emit edgeChanged();
            });
        }
    });

    //!    ----  both cases   ----  !//
    //! this is used for both location and screen change cases, this signal
    //! is send when the sliding-out animation has finished
    connect(this, &Positioner::hideDockDuringLocationChangeFinished, this, [&]() {
        m_view->effects()->setAnimationsBlocked(true);

        if (m_goToLocation != Plasma::Types::Floating) {
            m_view->setLocation(m_goToLocation);
        } else if (m_goToScreen) {
            setScreenToFollow(m_goToScreen);
        } else if (!m_moveToLayout.isEmpty()) {
            m_view->moveToLayout(m_moveToLayout);
        }
    });
}

bool Positioner::inLocationAnimation()
{
    return ((m_goToLocation != Plasma::Types::Floating) || (m_moveToLayout != "") || m_goToScreen);
}

bool Positioner::inSlideAnimation() const
{
    return m_inSlideAnimation;
}

void Positioner::setInSlideAnimation(bool active)
{
    if (m_inSlideAnimation == active) {
        return;
    }

    m_inSlideAnimation = active;
    emit inSlideAnimationChanged();
}

bool Positioner::isStickedOnTopEdge() const
{
    return m_isStickedOnTopEdge;
}

void Positioner::setIsStickedOnTopEdge(bool sticked)
{
    if (m_isStickedOnTopEdge == sticked) {
        return;
    }

    m_isStickedOnTopEdge = sticked;
    emit isStickedOnTopEdgeChanged();
}

bool Positioner::isStickedOnBottomEdge() const
{
    return m_isStickedOnBottomEdge;
}

void Positioner::setIsStickedOnBottomEdge(bool sticked)
{
    if (m_isStickedOnBottomEdge == sticked) {
        return;
    }

    m_isStickedOnBottomEdge = sticked;
    emit isStickedOnBottomEdgeChanged();
}

void Positioner::updateInLocationAnimation()
{
    bool inLocationAnimation = ((m_goToLocation != Plasma::Types::Floating) || (m_moveToLayout != "") || m_goToScreen);

    if (m_inLocationAnimation == inLocationAnimation) {
        return;
    }

    m_inLocationAnimation = inLocationAnimation;
    emit inLocationAnimationChanged();
}

void Positioner::hideDockDuringLocationChange(int goToLocation)
{
    m_goToLocation = static_cast<Plasma::Types::Location>(goToLocation);
    emit hideDockDuringLocationChangeStarted();
}

void Positioner::hideDockDuringMovingToLayout(QString layoutName)
{
    m_moveToLayout = layoutName;
    emit hideDockDuringMovingToLayoutStarted();
}

}
}
