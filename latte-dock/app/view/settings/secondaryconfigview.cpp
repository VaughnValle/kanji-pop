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

#include "secondaryconfigview.h"

// local
#include <config-latte.h>
#include "primaryconfigview.h"
#include "../panelshadows_p.h"
#include "../view.h"
#include "../../lattecorona.h"
#include "../../wm/abstractwindowinterface.h"

// Qt
#include <QQuickItem>
#include <QQmlContext>
#include <QQmlEngine>
#include <QScreen>

// KDE
#include <KLocalizedContext>
#include <KDeclarative/KDeclarative>
#include <KWayland/Client/plasmashell.h>
#include <KWayland/Client/surface.h>
#include <KWindowEffects>
#include <KWindowSystem>

// Plasma
#include <Plasma/Package>

namespace Latte {
namespace ViewPart {

SecondaryConfigView::SecondaryConfigView(Latte::View *view, QWindow *parent)
    : QQuickView(nullptr),
      m_latteView(view)
{
    m_parent = qobject_cast<PrimaryConfigView *>(parent);
    m_corona = qobject_cast<Latte::Corona *>(m_latteView->containment()->corona());

    setupWaylandIntegration();

    setTitle(validTitle());

    if (KWindowSystem::isPlatformX11()) {
        m_corona->wm()->registerIgnoredWindow(winId());
    } else {
        connect(m_corona->wm(), &WindowSystem::AbstractWindowInterface::latteWindowAdded, this, &SecondaryConfigView::updateWaylandId);
    }

    setResizeMode(QQuickView::SizeViewToRootObject);
    setScreen(m_latteView->screen());

    if (m_latteView && m_latteView->containment()) {
        setIcon(qGuiApp->windowIcon());
    }

    m_screenSyncTimer.setSingleShot(true);
    m_screenSyncTimer.setInterval(100);

    connect(this, &QQuickView::widthChanged, this, &SecondaryConfigView::updateEffects);
    connect(this, &QQuickView::heightChanged, this, &SecondaryConfigView::updateEffects);

    connect(this, &QQuickView::statusChanged, [&](QQuickView::Status status) {
        if (status == QQuickView::Ready) {
            updateEffects();
        }
    });

    connections << connect(m_parent, &PrimaryConfigView::availableScreenGeometryChanged, this, &SecondaryConfigView::syncGeometry);

    connections << connect(&m_screenSyncTimer, &QTimer::timeout, this, [this]() {
        setScreen(m_latteView->screen());
        setFlags(wFlags());

        if (KWindowSystem::isPlatformX11()) {
            m_corona->wm()->setViewExtraFlags(this, false, Latte::Types::NormalWindow);
        }

        syncGeometry();
        syncSlideEffect();
    });
    connections << connect(m_latteView->visibility(), &VisibilityManager::modeChanged, this, &SecondaryConfigView::syncGeometry);

    m_thicknessSyncTimer.setSingleShot(true);
    m_thicknessSyncTimer.setInterval(200);
    connections << connect(&m_thicknessSyncTimer, &QTimer::timeout, this, [this]() {
        syncGeometry();
    });

    connections << connect(m_latteView, &Latte::View::normalThicknessChanged, [&]() {
        m_thicknessSyncTimer.start();
    });
}

SecondaryConfigView::~SecondaryConfigView()
{
    qDebug() << "SecDockConfigView deleting ...";

    m_corona->dialogShadows()->removeWindow(this);

    m_corona->wm()->unregisterIgnoredWindow(KWindowSystem::isPlatformX11() ? winId() : m_waylandWindowId);

    for (const auto &var : connections) {
        QObject::disconnect(var);
    }
}

void SecondaryConfigView::init()
{
    qDebug() << "dock secondary config view : initialization started...";

    setDefaultAlphaBuffer(true);
    setColor(Qt::transparent);
    m_corona->dialogShadows()->addWindow(this);
    rootContext()->setContextProperty(QStringLiteral("latteView"), m_latteView);
    rootContext()->setContextProperty(QStringLiteral("viewConfig"), this);
    rootContext()->setContextProperty(QStringLiteral("plasmoid"), m_latteView->containment()->property("_plasma_graphicObject").value<QObject *>());

    KDeclarative::KDeclarative kdeclarative;
    kdeclarative.setDeclarativeEngine(engine());
    kdeclarative.setTranslationDomain(QStringLiteral("latte-dock"));
#if KF5_VERSION_MINOR >= 45
    kdeclarative.setupContext();
    kdeclarative.setupEngine(engine());
#else
    kdeclarative.setupBindings();
#endif

    QByteArray tempFilePath = "lattedocksecondaryconfigurationui";

    updateEnabledBorders();

    auto source = QUrl::fromLocalFile(m_latteView->containment()->corona()->kPackage().filePath(tempFilePath));
    setSource(source);
    syncGeometry();
    syncSlideEffect();

    if (m_parent && KWindowSystem::isPlatformX11()) {
        m_parent->requestActivate();
    }

    qDebug() << "dock secondary config view : initialization ended...";
}

inline Qt::WindowFlags SecondaryConfigView::wFlags() const
{
    return (flags() | Qt::FramelessWindowHint /*| Qt::WindowStaysOnTopHint*/) & ~Qt::WindowDoesNotAcceptFocus;
}

QString SecondaryConfigView::validTitle() const
{
    return QString("#secconfig#" + QString::number(m_latteView->containment()->id()));
}

QRect SecondaryConfigView::geometryWhenVisible() const
{
    return m_geometryWhenVisible;
}

void SecondaryConfigView::requestActivate()
{
    if (KWindowSystem::isPlatformWayland() && m_shellSurface) {
        updateWaylandId();
        m_corona->wm()->requestActivate(m_waylandWindowId);
    } else {
        QQuickView::requestActivate();
    }
}

void SecondaryConfigView::syncGeometry()
{
    if (!m_latteView || !m_latteView->layout() || !m_latteView->containment() || !m_parent || !rootObject()) {
        return;
    }

    const QSize size(rootObject()->width(), rootObject()->height());
    const auto location = m_latteView->containment()->location();
    const auto scrGeometry = m_latteView->screenGeometry();
    const auto availGeometry = m_parent->availableScreenGeometry();

    int clearThickness = m_latteView->editThickness();

    int secondaryConfigSpacing = 2 * m_latteView->fontPixelSize();

    QPoint position{0, 0};

    int xPos{0};
    int yPos{0};

    switch (m_latteView->containment()->formFactor()) {
    case Plasma::Types::Horizontal: {
        if (qApp->isLeftToRight()) {
            xPos = availGeometry.x() + secondaryConfigSpacing;
        } else {
            xPos = availGeometry.x() + availGeometry.width() - size.width() - secondaryConfigSpacing;
        }

        if (location == Plasma::Types::TopEdge) {
            yPos = scrGeometry.y() + clearThickness;
        } else if (location == Plasma::Types::BottomEdge) {
            yPos = scrGeometry.y() + scrGeometry.height() - clearThickness - size.height();
        }
    }
        break;

    case Plasma::Types::Vertical: {
        yPos = availGeometry.y() + secondaryConfigSpacing;

        if (location == Plasma::Types::LeftEdge) {
            xPos = scrGeometry.x() + clearThickness;
        } else if (location == Plasma::Types::RightEdge) {
            xPos = scrGeometry.x() + scrGeometry.width() - clearThickness - size.width();
        }
    }
        break;

    default:
        qWarning() << "no sync geometry, wrong formFactor";
        break;
    }

    position = {xPos, yPos};

    updateEnabledBorders();

    auto geometry = QRect(position.x(), position.y(), size.width(), size.height());

    if (m_geometryWhenVisible == geometry) {
        return;
    }

    m_geometryWhenVisible = geometry;

    setPosition(position);

    if (m_shellSurface) {
        m_shellSurface->setPosition(position);
    }

    setMaximumSize(size);
    setMinimumSize(size);
    resize(size);

    updateViewMask();

    //! after placement request to activate the main config window in order to avoid
    //! rare cases of closing settings window from secondaryConfigView->focusOutEvent
    if (m_parent && KWindowSystem::isPlatformX11()) {
        m_parent->requestActivate();
    }
}

void SecondaryConfigView::updateViewMask()
{
    bool environmentState = (KWindowSystem::isPlatformX11() && KWindowSystem::compositingActive());

    if (!environmentState) {
        return;
    }

    int x, y, thickness, length;
    QRegion area;

    thickness = m_latteView->effects()->editShadow();

    if (m_latteView->formFactor() == Plasma::Types::Vertical) {
        length = m_geometryWhenVisible.height();
    } else {
        length = m_geometryWhenVisible.width();
    }

    if (m_latteView->formFactor() == Plasma::Types::Horizontal) {
        x = m_geometryWhenVisible.x() - m_latteView->x();
    } else {
        y = m_geometryWhenVisible.y() - m_latteView->y();
    }

    if (m_latteView->location() == Plasma::Types::BottomEdge) {
        y = m_latteView->height() - m_latteView->editThickness() - m_latteView->effects()->editShadow();
    } else if (m_latteView->location() == Plasma::Types::TopEdge) {
        y = m_latteView->editThickness();
    } else if (m_latteView->location() == Plasma::Types::LeftEdge) {
        x = m_latteView->editThickness();
    } else if (m_latteView->location() == Plasma::Types::RightEdge) {
        x = m_latteView->width() - m_latteView->editThickness() - m_latteView->effects()->editShadow();
    }

    if (m_latteView->formFactor() == Plasma::Types::Horizontal) {
        area = QRect(x, y, length, thickness);
    } else {
        area = QRect(x, y, thickness, length);
    }

    m_latteView->effects()->setSubtractedMaskRegion(validTitle(), area);
}

void SecondaryConfigView::syncSlideEffect()
{
    if (!m_latteView || !m_latteView->containment()) {
        return;
    }

    auto slideLocation = WindowSystem::AbstractWindowInterface::Slide::None;

    switch (m_latteView->containment()->location()) {
    case Plasma::Types::TopEdge:
        slideLocation = WindowSystem::AbstractWindowInterface::Slide::Top;
        break;

    case Plasma::Types::RightEdge:
        slideLocation = WindowSystem::AbstractWindowInterface::Slide::Right;
        break;

    case Plasma::Types::BottomEdge:
        slideLocation = WindowSystem::AbstractWindowInterface::Slide::Bottom;
        break;

    case Plasma::Types::LeftEdge:
        slideLocation = WindowSystem::AbstractWindowInterface::Slide::Left;
        break;

    default:
        qDebug() << staticMetaObject.className() << "wrong location";
        break;
    }

    m_corona->wm()->slideWindow(*this, slideLocation);
}

void SecondaryConfigView::showEvent(QShowEvent *ev)
{
    QQuickWindow::showEvent(ev);

    if (!m_latteView) {
        return;
    }

    setFlags(wFlags());
    m_corona->wm()->setViewExtraFlags(this, false, Latte::Types::NormalWindow);

    syncGeometry();
    syncSlideEffect();

    m_screenSyncTimer.start();
    QTimer::singleShot(400, this, &SecondaryConfigView::syncGeometry);

    updateViewMask();
    emit showSignal();
}

void SecondaryConfigView::focusOutEvent(QFocusEvent *ev)
{
    Q_UNUSED(ev);

    const auto *focusWindow = qGuiApp->focusWindow();

    if ((focusWindow && (focusWindow->flags().testFlag(Qt::Popup)
                         || focusWindow->flags().testFlag(Qt::ToolTip)))
            || m_latteView->alternativesIsShown()) {
        return;
    }

    const auto parent = qobject_cast<PrimaryConfigView *>(m_parent);

    if (!m_latteView->containsMouse() && parent && !parent->sticker() && !parent->isActive()) {
        parent->hideConfigWindow();
    }
}

void SecondaryConfigView::setupWaylandIntegration()
{
    if (m_shellSurface || !KWindowSystem::isPlatformWayland() || !m_latteView || !m_latteView->containment()) {
        // already setup
        return;
    }

    if (m_corona) {
        using namespace KWayland::Client;
        PlasmaShell *interface = m_corona->waylandCoronaInterface();

        if (!interface) {
            return;
        }

        Surface *s = Surface::fromWindow(this);

        if (!s) {
            return;
        }

        qDebug() << "wayland secondary settings surface was created...";

        m_shellSurface = interface->createSurface(s, this);
        m_corona->wm()->setViewExtraFlags(m_shellSurface, false);

        syncGeometry();
    }
}

bool SecondaryConfigView::event(QEvent *e)
{
    if (e->type() == QEvent::PlatformSurface) {
        if (auto pe = dynamic_cast<QPlatformSurfaceEvent *>(e)) {
            switch (pe->surfaceEventType()) {
            case QPlatformSurfaceEvent::SurfaceCreated:

                if (m_shellSurface) {
                    break;
                }

                setupWaylandIntegration();
                break;

            case QPlatformSurfaceEvent::SurfaceAboutToBeDestroyed:
                if (m_shellSurface) {
                    delete m_shellSurface;
                    m_shellSurface = nullptr;
                    qDebug() << "WAYLAND secondary config window surface was deleted...";
                }

                break;
            }
        }
    }

    return QQuickView::event(e);
}

void SecondaryConfigView::hideConfigWindow()
{
    m_latteView->effects()->removeSubtractedMaskRegion(validTitle());

    if (m_shellSurface) {
        //!NOTE: Avoid crash in wayland environment with qt5.9
        close();
    } else {
        hide();
    }
}

void SecondaryConfigView::updateWaylandId()
{
    Latte::WindowSystem::WindowId newId = m_corona->wm()->winIdFor("latte-dock", validTitle());

    if (m_waylandWindowId != newId) {
        if (!m_waylandWindowId.isNull()) {
            m_corona->wm()->unregisterIgnoredWindow(m_waylandWindowId);
        }

        m_waylandWindowId = newId;
        m_corona->wm()->registerIgnoredWindow(m_waylandWindowId);
    }
}

void SecondaryConfigView::updateEffects()
{
    //! Don't apply any effect before the wayland surface is created under wayland
    //! https://bugs.kde.org/show_bug.cgi?id=392890
    if (KWindowSystem::isPlatformWayland() && !m_shellSurface) {
        return;
    }

    //! Don't apply any effect before the wayland surface is created under wayland
    //! https://bugs.kde.org/show_bug.cgi?id=392890
    if (KWindowSystem::isPlatformWayland() && !m_shellSurface) {
        return;
    }

    if (!m_background) {
        m_background = new Plasma::FrameSvg(this);
    }

    if (m_background->imagePath() != "dialogs/background") {
        m_background->setImagePath(QStringLiteral("dialogs/background"));
    }

    m_background->setEnabledBorders(m_enabledBorders);
    m_background->resizeFrame(size());

    QRegion mask = m_background->mask();

    QRegion fixedMask = mask.isNull() ? QRegion(QRect(0,0,width(),height())) : mask;

    if (!fixedMask.isEmpty()) {
        setMask(fixedMask);
    } else {
        setMask(QRegion());
    }

    if (KWindowSystem::compositingActive()) {
        KWindowEffects::enableBlurBehind(winId(), true, fixedMask);
    } else {
        KWindowEffects::enableBlurBehind(winId(), false);
    }
}

//!BEGIN borders
Plasma::FrameSvg::EnabledBorders SecondaryConfigView::enabledBorders() const
{
    return m_enabledBorders;
}

void SecondaryConfigView::updateEnabledBorders()
{
    if (!this->screen()) {
        return;
    }

    Plasma::FrameSvg::EnabledBorders borders = Plasma::FrameSvg::AllBorders;

    switch (m_latteView->location()) {
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
        borders &=  ~Plasma::FrameSvg::BottomBorder;
        break;

    default:
        break;
    }

    if (m_enabledBorders != borders) {
        m_enabledBorders = borders;

        m_corona->dialogShadows()->addWindow(this, m_enabledBorders);

        emit enabledBordersChanged();
    }
}

//!END borders

}
}

