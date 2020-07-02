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

import QtQuick 2.0

import org.kde.plasma.components 2.0 as PlasmaComponents

import org.kde.latte.core 0.2 as LatteCore

MouseArea {
    property var modelIndex
    property int winId // FIXME Legacy
    property Item rootTask

    acceptedButtons: Qt.LeftButton | Qt.MiddleButton | Qt.RightButton
    hoverEnabled: true
    enabled: LatteCore.WindowSystem.isPlatformWayland ||
             (!LatteCore.WindowSystem.isPlatformWayland && winId != 0)

    onClicked: {
        //!used mainly to not close the previews window when the user closes many windows simultaneously
        var keepVisibility = false;

        if (mouse.button == Qt.LeftButton) {
            tasksModel.requestActivate(modelIndex);
        } else if (mouse.button == Qt.MiddleButton) {
            if (isGroup) {
                keepVisibility = true;
            }

            tasksModel.requestClose(modelIndex);
        } else {
            root.createContextMenu(rootTask, modelIndex).show();
        }

        if (!keepVisibility) {
            windowsPreviewDlg.visible = false;
            root.forcePreviewsHiding(14.5);
        }
    }

    onContainsMouseChanged: {
        root.windowsHovered([winId], containsMouse);
    }
}

