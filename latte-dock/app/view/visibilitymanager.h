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

#ifndef VISIBILITYMANAGER_H
#define VISIBILITYMANAGER_H

#include <array>

// local
#include <coretypes.h>
#include "../plasma/quick/containmentview.h"

// Qt
#include <QObject>
#include <QTimer>

// Plasma
#include <Plasma/Containment>


namespace Latte {
class Corona;
class View;
namespace ViewPart {
class FloatingGapWindow;
class ScreenEdgeGhostWindow;
}
namespace WindowSystem {
class AbstractWindowInterface;
}
}

namespace Latte {
namespace ViewPart {

class VisibilityManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool hidingIsBlocked READ hidingIsBlocked NOTIFY hidingIsBlockedChanged)

    Q_PROPERTY(Latte::Types::Visibility mode READ mode WRITE setMode NOTIFY modeChanged)
    Q_PROPERTY(bool raiseOnDesktop READ raiseOnDesktop WRITE setRaiseOnDesktop NOTIFY raiseOnDesktopChanged)
    Q_PROPERTY(bool raiseOnActivity READ raiseOnActivity WRITE setRaiseOnActivity NOTIFY raiseOnActivityChanged)    
    Q_PROPERTY(bool isHidden READ isHidden WRITE setIsHidden NOTIFY isHiddenChanged)
    Q_PROPERTY(bool isBelowLayer READ isBelowLayer NOTIFY isBelowLayerChanged)    
    Q_PROPERTY(bool containsMouse READ containsMouse NOTIFY containsMouseChanged)

    //! KWin Edges Support Options
    Q_PROPERTY(bool enableKWinEdges READ enableKWinEdges WRITE setEnableKWinEdges NOTIFY enableKWinEdgesChanged)
    Q_PROPERTY(bool supportsKWinEdges READ supportsKWinEdges NOTIFY supportsKWinEdgesChanged)

    Q_PROPERTY(int timerShow READ timerShow WRITE setTimerShow NOTIFY timerShowChanged)
    Q_PROPERTY(int timerHide READ timerHide WRITE setTimerHide NOTIFY timerHideChanged)

public:
    explicit VisibilityManager(PlasmaQuick::ContainmentView *view);
    virtual ~VisibilityManager();

    Latte::Types::Visibility mode() const;
    void setMode(Latte::Types::Visibility mode);

    void applyActivitiesToHiddenWindows(const QStringList &activities);

    bool raiseOnDesktop() const;
    void setRaiseOnDesktop(bool enable);

    bool raiseOnActivity() const;
    void setRaiseOnActivity(bool enable);

    bool isBelowLayer() const;

    bool isHidden() const;
    void setIsHidden(bool isHidden);

    bool hidingIsBlocked() const;

    bool containsMouse() const;

    int timerShow() const;
    void setTimerShow(int msec);

    int timerHide() const;
    void setTimerHide(int msec);

    //! KWin Edges Support functions
    bool enableKWinEdges() const;
    void setEnableKWinEdges(bool enable);

    bool supportsKWinEdges() const;

    //! Used mostly to show / hide SideBars
    void toggleHiddenState();

public slots:
    Q_INVOKABLE void hide();
    Q_INVOKABLE void show();

    Q_INVOKABLE void setViewOnBackLayer();
    Q_INVOKABLE void setViewOnFrontLayer();

    Q_INVOKABLE void addBlockHidingEvent(const QString &type);
    Q_INVOKABLE void removeBlockHidingEvent(const QString &type);

    void initViewFlags();

signals:
    void mustBeShown();
    void mustBeHide();

    void slideOutFinished();
    void slideInFinished();

    void frameExtentsCleared();
    void modeChanged();
    void raiseOnDesktopChanged();
    void raiseOnActivityChanged();
    void isBelowLayerChanged();
    void isHiddenChanged();
    void hidingIsBlockedChanged();
    void containsMouseChanged();
    void timerShowChanged();
    void timerHideChanged();

    //! KWin Edges Support signals
    void enableKWinEdgesChanged();
    void supportsKWinEdgesChanged();

private slots:
    void saveConfig();
    void restoreConfig();

    void setIsBelowLayer(bool below);

    void on_hidingIsBlockedChanged();

    void on_publishFrameExtents(); //! delayed
    void publishFrameExtents(bool forceUpdate = false); //! direct

    //! KWin Edges Support functions
    void updateKWinEdgesSupport();

private:
    void setContainsMouse(bool contains);

    void raiseView(bool raise);
    void raiseViewTemporarily();

    //! KWin Edges Support functions
    void createEdgeGhostWindow();
    void deleteEdgeGhostWindow();
    void updateGhostWindowState();

    //! Floating Gap Support functions
    void createFloatingGapWindow();
    void deleteFloatingGapWindow();
    bool supportsFloatingGap() const;

    void updateStrutsBasedOnLayoutsAndActivities(bool forceUpdate = false);
    void viewEventManager(QEvent *ev);

    void checkMouseInFloatingArea();

    bool windowContainsMouse();

    QRect acceptableStruts();

private slots:
    void dodgeAllWindows();
    void dodgeActive();
    void dodgeMaximized();
    void updateHiddenState();

    bool isValidMode() const;

private:
    WindowSystem::AbstractWindowInterface *m_wm;
    Types::Visibility m_mode{Types::None};
    std::array<QMetaObject::Connection, 5> m_connections;

    QTimer m_timerShow;
    QTimer m_timerHide;
    QTimer m_timerStartUp;
    QTimer m_timerPublishFrameExtents;

    bool m_isBelowLayer{false};
    bool m_isHidden{false};
    bool m_dragEnter{false};
    bool m_containsMouse{false};
    bool m_raiseTemporarily{false};
    bool m_raiseOnDesktopChange{false};
    bool m_raiseOnActivityChange{false};
    bool m_hideNow{false};

    int m_frameExtentsHeadThicknessGap{0};
    Plasma::Types::Location m_frameExtentsLocation{Plasma::Types::BottomEdge};

    QStringList m_blockHidingEvents;

    QRect m_publishedStruts;
    QRegion m_lastMask;

    //! KWin Edges
    bool m_enableKWinEdgesFromUser{true};
    std::array<QMetaObject::Connection, 1> m_connectionsKWinEdges;
    ScreenEdgeGhostWindow *m_edgeGhostWindow{nullptr};

    //! Floating Gap
    FloatingGapWindow *m_floatingGapWindow{nullptr};

    Latte::Corona *m_corona{nullptr};
    Latte::View *m_latteView{nullptr};

};

}
}
#endif // VISIBILITYMANAGER_H
