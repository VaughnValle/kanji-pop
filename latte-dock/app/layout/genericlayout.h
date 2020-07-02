/*
*  Copyright 2019  Michail Vourlakos <mvourlakos@gmail.com>
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

#ifndef GENERICLAYOUT_H
#define GENERICLAYOUT_H

// local
#include <coretypes.h>
#include "abstractlayout.h"

// Qt
#include <QObject>
#include <QQuickView>
#include <QPointer>
#include <QScreen>

// Plasma
#include <Plasma>

namespace Plasma {
class Applet;
class Containment;
class Types;
}

namespace Latte {
class Corona;
class ScreenPool;
class View;
}

namespace Latte {
namespace Layout {
class Storage;
}
}

namespace Latte {
namespace Layout {

struct ViewData
{
    int id; //view id
    bool active; //is active
    bool onPrimary; //on primary
    int screenId; //explicit screen id
    int location; //edge location
    QList<int> systrays;
};

//! This is  views map in the following structure:
//! SCREEN_NAME -> EDGE -> VIEWID
typedef QHash<QString, QHash<Plasma::Types::Location, QList<uint>>> ViewsMap;

class GenericLayout : public AbstractLayout
{
    Q_OBJECT
    Q_PROPERTY(int viewsCount READ viewsCount NOTIFY viewsCountChanged)

public:   
    GenericLayout(QObject *parent, QString layoutFile, QString assignedName = QString());
    ~GenericLayout() override;

    virtual const QStringList appliedActivities() = 0; // to move at an interface

    void importToCorona();
    bool initToCorona(Latte::Corona *corona);

    bool isActive() const; //! is loaded and running
    virtual bool isCurrent() const;
    bool isWritable() const;
    bool layoutIsBroken() const;

    bool isInternalContainment(Plasma::Applet *applet) const;
    Plasma::Containment *internalContainmentOf(Plasma::Applet *applet) const;

    virtual int viewsCount(int screen) const;
    virtual int viewsCount(QScreen *screen) const;
    virtual int viewsCount() const;

    Type type() const override;

    Latte::Corona *corona();

    QStringList unloadedContainmentsIds();

    virtual Types::ViewType latteViewType(uint containmentId) const;
    const QList<Plasma::Containment *> *containments();

    Latte::View *highestPriorityView();
    Latte::View *viewForContainment(Plasma::Containment *containment);
    virtual QList<Latte::View *> sortedLatteViews(QList<Latte::View *> views = QList<Latte::View *>());
    virtual QList<Latte::View *> viewsWithPlasmaShortcuts();
    virtual QList<Latte::View *> latteViews();
    ViewsMap validViewsMap(ViewsMap *occupiedMap = nullptr);
    virtual void syncLatteViewsToScreens(Layout::ViewsMap *occupiedMap = nullptr);

    void syncToLayoutFile(bool removeLayoutId = false);

    void lock(); //! make it only read-only
    void renameLayout(QString newName);
    virtual void unloadContainments();
    void unloadLatteViews();
    void unlock(); //! make it writable which it should be the default

    virtual void setLastConfigViewFor(Latte::View *view);
    virtual Latte::View *lastConfigViewFor();

    //! this function needs the layout to have first set the corona through initToCorona() function
    virtual void addView(Plasma::Containment *containment, bool forceOnPrimary = false, int explicitScreen = -1, Layout::ViewsMap *occupied = nullptr);
    void copyView(Plasma::Containment *containment);
    void recreateView(Plasma::Containment *containment, bool delayed = true);
    bool latteViewExists(Plasma::Containment *containment);

    //! Available edges for specific view in that screen
    virtual QList<Plasma::Types::Location> availableEdgesForView(QScreen *scr, Latte::View *forView) const;
    //! All free edges in that screen
    virtual QList<Plasma::Types::Location> freeEdges(QScreen *scr) const;
    virtual QList<Plasma::Types::Location> freeEdges(int screen) const;

    //! Bind this latteView and its relevant containments(including systrays)
    //! to this layout. It is used for moving a Latte::View from layout to layout)
    void assignToLayout(Latte::View *latteView, QList<Plasma::Containment *> containments);
    //! Unassign that latteView from this layout (this is used for moving a latteView
    //! from layout to layout) and returns all the containments relevant to
    //! that latteView
    QList<Plasma::Containment *> unassignFromLayout(Latte::View *latteView);

    QString reportHtml(const ScreenPool *screenPool);
    QList<int> viewsScreens();

public slots:
    Q_INVOKABLE void addNewView();
    Q_INVOKABLE int viewsWithTasks() const;
    virtual Q_INVOKABLE QList<int> qmlFreeEdges(int screen) const;  //change <Plasma::Types::Location> to <int> types

    void toggleHiddenState(QString screenName, Plasma::Types::Location edge);

signals:
    void activitiesChanged(); // to move at an interface
    void viewsCountChanged();
    void viewEdgeChanged();

    //! used from ConfigView(s) in order to be informed which is one should be shown
    void lastConfigViewForChanged(Latte::View *view);

    //! used from LatteView(s) in order to exist only one each time that has the highest priority
    //! to use the global shortcuts activations
    void preferredViewForShortcutsChanged(Latte::View *view);

protected:
    void updateLastUsedActivity();

protected:
    Latte::Corona *m_corona{nullptr};

    QList<Plasma::Containment *> m_containments;

    QHash<const Plasma::Containment *, Latte::View *> m_latteViews;
    QHash<const Plasma::Containment *, Latte::View *> m_waitingLatteViews;

private slots:
    void addContainment(Plasma::Containment *containment);
    void appletCreated(Plasma::Applet *applet);
    void destroyedChanged(bool destroyed);
    void containmentDestroyed(QObject *cont);

private:
    //! It can be used in order for LatteViews to not be created automatically when
    //! their corresponding containments are created e.g. copyView functionality
    bool blockAutomaticLatteViewCreation() const;
    void setBlockAutomaticLatteViewCreation(bool block);

    bool explicitDockOccupyEdge(int screen, Plasma::Types::Location location) const;
    bool primaryDockOccupyEdge(Plasma::Types::Location location) const;

    bool viewAtLowerScreenPriority(Latte::View *test, Latte::View *base);
    bool viewAtLowerEdgePriority(Latte::View *test, Latte::View *base);

    bool viewDataAtLowerEdgePriority(const ViewData &test, const ViewData &base) const;
    bool viewDataAtLowerScreenPriority(const ViewData &test, const ViewData &base) const;
    bool viewDataAtLowerStatePriority(const ViewData &test, const ViewData &base) const;

    bool mapContainsId(const ViewsMap *map, uint viewId) const;

    QList<int> containmentSystrays(Plasma::Containment *containment) const;

    QList<ViewData> sortedViewsData(const QList<ViewData> &viewsData);

private:
    bool m_blockAutomaticLatteViewCreation{false};

    QPointer<Latte::View> m_lastConfigViewFor;

    QStringList m_unloadedContainmentsIds;

    QPointer<Storage> m_storage;

    //! try to avoid crashes from recreating the same views all the time
    QList<const Plasma::Containment *> m_viewsToRecreate;

    friend class Storage;
    friend class Latte::View;
};

}
}

#endif
