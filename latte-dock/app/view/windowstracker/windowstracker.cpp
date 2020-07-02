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

#include "windowstracker.h"

// local
#include "currentscreentracker.h"
#include "allscreenstracker.h"
#include "../view.h"
#include "../../lattecorona.h"
#include "../../wm/tracker/windowstracker.h"


namespace Latte {
namespace ViewPart {

WindowsTracker::WindowsTracker(Latte::View *parent)
    : QObject(parent),
      m_latteView(parent)
{
    qDebug() << "WindowsTracker creating...";

    auto corona = qobject_cast<Latte::Corona *>(m_latteView->corona());
    m_wm = corona->wm();

    m_allScreensTracker = new TrackerPart::AllScreensTracker(this);
    m_currentScreenTracker = new TrackerPart::CurrentScreenTracker(this);

    connect(m_wm->windowsTracker(), &WindowSystem::Tracker::Windows::enabledChanged, this, [&](const Latte::View *view) {
        if (m_latteView == view) {
            emit enabledChanged();
        }
    });

    m_wm->windowsTracker()->addView(m_latteView);

    emit allScreensChanged();
    emit currentScreenChanged();
}

WindowsTracker::~WindowsTracker()
{
    qDebug() << "WindowsTracker removing...";

    if (m_allScreensTracker) {
        m_allScreensTracker->deleteLater();
    }

    if (m_currentScreenTracker) {
        m_currentScreenTracker->deleteLater();
    }
}

Latte::View *WindowsTracker::view() const
{
    return m_latteView;
}

WindowSystem::AbstractWindowInterface *WindowsTracker::wm() const
{
    return m_wm;
}

bool WindowsTracker::enabled() const
{
    return m_wm->windowsTracker()->enabled(m_latteView);
}

void WindowsTracker::setEnabled(bool active)
{
    m_wm->windowsTracker()->setEnabled(m_latteView, active);
}


TrackerPart::AllScreensTracker *WindowsTracker::allScreens() const
{
    return m_allScreensTracker;
}

TrackerPart::CurrentScreenTracker *WindowsTracker::currentScreen() const
{
    return m_currentScreenTracker;
}

//! Window Functions
void WindowsTracker::setWindowOnActivities(QWindow &window, const QStringList &activities)
{
    m_wm->setWindowOnActivities(window, activities);
}

//! Environment Functions
void WindowsTracker::switchToNextActivity()
{
    m_wm->switchToNextActivity();
}

void WindowsTracker::switchToPreviousActivity()
{
    m_wm->switchToPreviousActivity();
}

void WindowsTracker::switchToNextVirtualDesktop()
{
    m_wm->switchToNextVirtualDesktop();
}

void WindowsTracker::switchToPreviousVirtualDesktop()
{
    m_wm->switchToPreviousVirtualDesktop();
}

}
}
