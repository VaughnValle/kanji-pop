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

#include "effects.h"

// local
#include <config-latte.h>
#include <coretypes.h>
#include "panelshadows_p.h"
#include "view.h"

// Qt
#include <QRegion>

// KDE
#include <KWindowEffects>
#include <KWindowSystem>


namespace Latte {
namespace ViewPart {

Effects::Effects(Latte::View *parent)
    : QObject(parent),
      m_view(parent)
{
    init();
}

Effects::~Effects()
{
}

void Effects::init()
{
    connect(this, &Effects::backgroundOpacityChanged, this, &Effects::updateEffects);
    connect(this, &Effects::drawEffectsChanged, this, &Effects::updateEffects);
    connect(this, &Effects::enabledBordersChanged, this, &Effects::updateEffects);
    connect(this, &Effects::rectChanged, this, &Effects::updateEffects);

    connect(this, &Effects::subtractedMaskRegionsChanged, this, &Effects::updateMask);
    connect(this, &Effects::unitedMaskRegionsChanged, this, &Effects::updateMask);

    connect(this, &Effects::drawShadowsChanged, this, [&]() {
        if (m_view->behaveAsPlasmaPanel()) {
            updateEnabledBorders();
        }
    });

    connect(m_view, &Latte::View::alignmentChanged, this, &Effects::updateEnabledBorders);
    connect(m_view, &Latte::View::screenEdgeMarginEnabledChanged, this, &Effects::updateEnabledBorders);
    connect(m_view, &Latte::View::behaveAsPlasmaPanelChanged, this, &Effects::updateEffects);
    connect(this, &Effects::drawShadowsChanged, this, &Effects::updateShadows);
    connect(m_view, &Latte::View::behaveAsPlasmaPanelChanged, this, &Effects::updateShadows);
    connect(m_view, &Latte::View::configWindowGeometryChanged, this, &Effects::updateMask);

    connect(&m_theme, &Plasma::Theme::themeChanged, this, [&]() {
        auto background = m_background;
        m_background = new Plasma::FrameSvg(this);

        if (background) {
            background->deleteLater();
        }

        if (m_background->imagePath() != "widgets/panel-background") {
            m_background->setImagePath(QStringLiteral("widgets/panel-background"));
        }

        updateBackgroundContrastValues();
        updateEffects();
    });
}

bool Effects::animationsBlocked() const
{
    return m_animationsBlocked;
}

void Effects::setAnimationsBlocked(bool blocked)
{
    if (m_animationsBlocked == blocked) {
        return;
    }

    m_animationsBlocked = blocked;
    emit animationsBlockedChanged();
}

bool Effects::drawShadows() const
{
    return m_drawShadows;
}

void Effects::setDrawShadows(bool draw)
{
    if (m_drawShadows == draw) {
        return;
    }

    m_drawShadows = draw;

    emit drawShadowsChanged();
}

bool Effects::drawEffects() const
{
    return m_drawEffects;
}

void Effects::setDrawEffects(bool draw)
{
    if (m_drawEffects == draw) {
        return;
    }

    m_drawEffects = draw;

    emit drawEffectsChanged();
}

void Effects::setForceBottomBorder(bool draw)
{
    if (m_forceBottomBorder == draw) {
        return;
    }

    m_forceBottomBorder = draw;
    updateEnabledBorders();
}

void Effects::setForceTopBorder(bool draw)
{
    if (m_forceTopBorder == draw) {
        return;
    }

    m_forceTopBorder = draw;
    updateEnabledBorders();
}

float Effects::backgroundOpacity() const
{
    return m_backgroundOpacity;
}

void Effects::setBackgroundOpacity(float opacity)
{
    if (m_backgroundOpacity == opacity) {
        return;
    }

    m_backgroundOpacity = opacity;

    updateBackgroundContrastValues();
    emit backgroundOpacityChanged();
}

int Effects::editShadow() const
{
    return m_editShadow;
}

void Effects::setEditShadow(int shadow)
{
    if (m_editShadow == shadow) {
        return;
    }

    m_editShadow = shadow;
    emit editShadowChanged();
}

int Effects::innerShadow() const
{
    return m_innerShadow;
}

void Effects::setInnerShadow(int shadow)
{
    if (m_innerShadow == shadow)
        return;

    m_innerShadow = shadow;

    emit innerShadowChanged();
}

QRect Effects::rect() const
{
    return m_rect;
}

void Effects::setRect(QRect area)
{
    if (m_rect == area) {
        return;
    }

    m_rect = area;

    emit rectChanged();
}

QRect Effects::mask() const
{
    return m_mask;
}

void Effects::setMask(QRect area)
{
    if (m_mask == area)
        return;

    m_mask = area;
    updateMask();

    // qDebug() << "dock mask set:" << m_mask;
    emit maskChanged();
}

void Effects::forceMaskRedraw()
{
    if (m_background) {
        delete m_background;
    }

    m_background = new Plasma::FrameSvg(this);
    m_background->setImagePath(QStringLiteral("widgets/panel-background"));
    m_background->setEnabledBorders(m_enabledBorders);

    updateMask();
}

void Effects::setSubtractedMaskRegion(const QString &regionid, const QRegion &region)
{
    if (m_subtractedMaskRegions.contains(regionid) && m_subtractedMaskRegions[regionid] == region) {
        return;
    }

    m_subtractedMaskRegions[regionid] = region;
    emit subtractedMaskRegionsChanged();
}

void Effects::removeSubtractedMaskRegion(const QString &regionid)
{
    if (!m_subtractedMaskRegions.contains(regionid)) {
        return;
    }

    m_subtractedMaskRegions.remove(regionid);
    emit subtractedMaskRegionsChanged();
}

void Effects::setUnitedMaskRegion(const QString &regionid, const QRegion &region)
{
    if (m_unitedMaskRegions.contains(regionid) && m_unitedMaskRegions[regionid] == region) {
        return;
    }

    m_unitedMaskRegions[regionid] = region;
    emit unitedMaskRegionsChanged();
}

void Effects::removeUnitedMaskRegion(const QString &regionid)
{
    if (!m_unitedMaskRegions.contains(regionid)) {
        return;
    }

    m_unitedMaskRegions.remove(regionid);
    emit unitedMaskRegionsChanged();
}

QRegion Effects::maskCombinedRegion()
{
    QRegion region = m_mask;

    for(auto subregion : m_subtractedMaskRegions) {
        region = region.subtracted(subregion);
    }

    for(auto subregion : m_unitedMaskRegions) {
        region = region.united(subregion);
    }

    return region;
}

void Effects::updateMask()
{
    if (KWindowSystem::compositingActive()) {
        if (m_view->behaveAsPlasmaPanel()) {
            m_view->setMask(QRect());
        } else {
            m_view->setMask(maskCombinedRegion());
        }
    } else {
        //! this is used when compositing is disabled and provides
        //! the correct way for the mask to be painted in order for
        //! rounded corners to be shown correctly
        //! the enabledBorders check was added because there was cases
        //! that the mask region wasn't calculated correctly after location changes
        if (!m_background) {
            if (m_background && m_background->enabledBorders() != m_enabledBorders) {
                delete m_background;
            }

            m_background = new Plasma::FrameSvg(this);
        }

        if (m_background->imagePath() != "widgets/panel-background") {
            m_background->setImagePath(QStringLiteral("widgets/panel-background"));
        }

        m_background->setEnabledBorders(m_enabledBorders);
        m_background->resizeFrame(m_mask.size());
        QRegion fixedMask = m_background->mask();
        fixedMask.translate(m_mask.x(), m_mask.y());

        //! fix for KF5.32 that return empty QRegion's for the mask
        if (fixedMask.isEmpty()) {
            fixedMask = QRegion(m_mask);
        }

        m_view->setMask(fixedMask);
    }
}

void Effects::clearShadows()
{
    PanelShadows::self()->removeWindow(m_view);
}

void Effects::updateShadows()
{
    if (m_view->behaveAsPlasmaPanel() && drawShadows()) {
        PanelShadows::self()->addWindow(m_view, enabledBorders());
    } else {
        PanelShadows::self()->removeWindow(m_view);
    }
}

void Effects::updateEffects()
{
    //! Don't apply any effect before the wayland surface is created under wayland
    //! https://bugs.kde.org/show_bug.cgi?id=392890
    if (KWindowSystem::isPlatformWayland() && !m_view->surface()) {
        return;
    }

    bool clearEffects{true};

    if (m_drawEffects) {
        if (!m_view->behaveAsPlasmaPanel()) {
            if (!m_rect.isNull() && !m_rect.isEmpty()) {
                //! this is used when compositing is disabled and provides
                //! the correct way for the mask to be painted in order for
                //! rounded corners to be shown correctly
                if (!m_background) {
                    m_background = new Plasma::FrameSvg(this);
                }

                if (m_background->imagePath() != "widgets/panel-background") {
                    m_background->setImagePath(QStringLiteral("widgets/panel-background"));
                }

                m_background->setEnabledBorders(m_enabledBorders);
                m_background->resizeFrame(m_rect.size());

                QRegion backMask = m_background->mask();

                //! There are cases that mask is NULL even though it should not
                //! Example: SideBar from v0.10 that BEHAVEASPLASMAPANEL in EditMode
                //! switching multiple times between inConfigureAppletsMode and LiveEditMode
                //! is such a case
                QRegion fixedMask;

                int fX = m_rect.x(); int fY = m_rect.y();

#if KF5_VERSION_MINOR >= 65
                //! Latte is now using GtkFrameExtents so Effects geometries must be adjusted
                //! windows that use GtkFrameExtents and apply Effects on them they take GtkFrameExtents
                //! as granted
                if (KWindowSystem::isPlatformX11()) {
                        if (m_view->location() == Plasma::Types::BottomEdge) {
                            fY = qMax(0, fY - m_view->headThicknessGap());
                        } else if (m_view->location() == Plasma::Types::RightEdge) {
                            fX = qMax(0, fX - m_view->headThicknessGap());
                        }
                }
#endif

                if (!backMask.isNull()) {
                    fixedMask = backMask;
                    fixedMask.translate(fX, fY);
                } else {
                    fixedMask = QRect(fX, fY, m_rect.width(), m_rect.height());
                }

                if (!fixedMask.isEmpty()) {
                    clearEffects = false;
                    KWindowEffects::enableBlurBehind(m_view->winId(), true, fixedMask);
                    KWindowEffects::enableBackgroundContrast(m_view->winId(),
                                                             m_theme.backgroundContrastEnabled(),
                                                             m_backEffectContrast,
                                                             m_backEffectIntesity,
                                                             m_backEffectSaturation,
                                                             fixedMask);
                }
            }
        } else {
            //!  BEHAVEASPLASMAPANEL case
            clearEffects = false;
            KWindowEffects::enableBlurBehind(m_view->winId(), true);
            KWindowEffects::enableBackgroundContrast(m_view->winId(),
                                                     m_theme.backgroundContrastEnabled(),
                                                     m_backEffectContrast,
                                                     m_backEffectIntesity,
                                                     m_backEffectSaturation);
        }
    }

    if (clearEffects) {
        KWindowEffects::enableBlurBehind(m_view->winId(), false);
        KWindowEffects::enableBackgroundContrast(m_view->winId(), false);
    }
}

//!BEGIN draw panel shadows outside the dock window
Plasma::FrameSvg::EnabledBorders Effects::enabledBorders() const
{
    return m_enabledBorders;
}

qreal Effects::currentMidValue(const qreal &max, const qreal &factor, const qreal &min) const
{
    if (max==min || factor==0) {
        return min;
    }

    qreal space = 0;
    qreal distance = 0;

    if (max<min) {
        space = min-max;
        distance = factor*space;
        return 1-distance;
    } else {
        space = max-min;
        distance = factor*space;
        return 1+distance;
    }
}

void Effects::updateBackgroundContrastValues()
{
    if (!m_theme.backgroundContrastEnabled()) {
        m_backEffectContrast = 1;
        m_backEffectIntesity = 1;
        m_backEffectSaturation = 1;
        return;
    }

    m_backEffectContrast = currentMidValue(m_theme.backgroundContrast(), m_backgroundOpacity, 1);
    m_backEffectIntesity = currentMidValue(m_theme.backgroundIntensity(), m_backgroundOpacity, 1);
    m_backEffectSaturation = currentMidValue(m_theme.backgroundSaturation(), m_backgroundOpacity, 1);
}

void Effects::updateEnabledBorders()
{
    if (!m_view->screen()) {
        return;
    }

    Plasma::FrameSvg::EnabledBorders borders = Plasma::FrameSvg::AllBorders;

    if (!m_view->screenEdgeMarginEnabled()) {
        switch (m_view->location()) {
        case Plasma::Types::TopEdge:
            borders &= ~Plasma::FrameSvg::TopBorder;
            break;

        case Plasma::Types::LeftEdge:
            borders &= ~Plasma::FrameSvg::LeftBorder;
            break;

        case Plasma::Types::RightEdge:
            borders &= ~Plasma::FrameSvg::RightBorder;
            break;

        case Plasma::Types::BottomEdge:
            borders &= ~Plasma::FrameSvg::BottomBorder;
            break;

        default:
            break;
        }
    }

    if ((m_view->location() == Plasma::Types::LeftEdge || m_view->location() == Plasma::Types::RightEdge)) {
        if (m_view->maxLength() == 1 && m_view->alignment() == Latte::Types::Justify) {
            if (!m_forceTopBorder) {
                borders &= ~Plasma::FrameSvg::TopBorder;
            }

            if (!m_forceBottomBorder) {
                borders &= ~Plasma::FrameSvg::BottomBorder;
            }
        }

        if (m_view->alignment() == Latte::Types::Top && !m_forceTopBorder && m_view->offset() == 0) {
            borders &= ~Plasma::FrameSvg::TopBorder;
        }

        if (m_view->alignment() == Latte::Types::Bottom && !m_forceBottomBorder && m_view->offset() == 0) {
            borders &= ~Plasma::FrameSvg::BottomBorder;
        }
    }

    if (m_view->location() == Plasma::Types::TopEdge || m_view->location() == Plasma::Types::BottomEdge) {
        if (m_view->maxLength() == 1 && m_view->alignment() == Latte::Types::Justify) {
            borders &= ~Plasma::FrameSvg::LeftBorder;
            borders &= ~Plasma::FrameSvg::RightBorder;
        }

        if (m_view->alignment() == Latte::Types::Left && m_view->offset() == 0) {
            borders &= ~Plasma::FrameSvg::LeftBorder;
        }

        if (m_view->alignment() == Latte::Types::Right  && m_view->offset() == 0) {
            borders &= ~Plasma::FrameSvg::RightBorder;
        }
    }

    if (m_enabledBorders != borders) {
        m_enabledBorders = borders;
        emit enabledBordersChanged();
    }

    if (!m_view->behaveAsPlasmaPanel() || !m_drawShadows) {
        PanelShadows::self()->removeWindow(m_view);
    } else {
        PanelShadows::self()->setEnabledBorders(m_view, borders);
    }
}
//!END draw panel shadows outside the dock window

}
}
