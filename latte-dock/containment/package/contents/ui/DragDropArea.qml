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

import QtQuick 2.7

import org.kde.plasma.core 2.0 as PlasmaCore
import org.kde.draganddrop 2.0 as DragDrop

DragDrop.DropArea {
    id: dragArea

    property bool isForeground: false

    readonly property Item dragInfo: Item {
        readonly property bool entered: latteView && latteView.containsDrag
        property bool isTask: false
        property bool isPlasmoid: false
        property bool isSeparator: false
        property bool isLatteTasks: false
        property bool onlyLaunchers: false

        property bool computationsAreValid: false
    }

    Connections{
        target: root.dragInfo

        onEnteredChanged: {
            if(!root.dragInfo.entered) {
                dragArea.clearInfo();
            }
        }
    }

    Connections{
        target: latteView

        onContainsDragChanged: {
            if(!latteView.containsDrag) {
                dragArea.clearInfo();
            }
        }
    }

    function clearInfo() {
        clearInfoTimer.restart();
    }

    //! Give the time when an applet is dropped to be positioned properly
    Timer {
        id: clearInfoTimer
        interval: 100 //dragArea.isForeground ? 100 : 500

        onTriggered: {
            dragArea.dragInfo.computationsAreValid = false;

            dragArea.dragInfo.isTask = false;
            dragArea.dragInfo.isPlasmoid = false;
            dragArea.dragInfo.isSeparator = false;
            dragArea.dragInfo.isLatteTasks = false;
            dragArea.dragInfo.onlyLaunchers = false;

            dndSpacer.parent = root;
            dndSpacer.opacity = 0;
        }
    }

    onDragEnter: {
        clearInfoTimer.stop();
        var isTask = event !== undefined
                && event.mimeData !== undefined
                && event.mimeData.formats !== undefined
                && event.mimeData.formats.indexOf("application/x-orgkdeplasmataskmanager_taskbuttonitem") >= 0;

        var isSeparator = event !== undefined
                && event.mimeData !== undefined
                && ( latteView.mimeContainsPlasmoid(event.mimeData, "audoban.applet.separator")
                    || latteView.mimeContainsPlasmoid(event.mimeData, "org.kde.latte.separator") );

        var isLatteTasks = event !== undefined
                && event.mimeData !== undefined
                && latteView.mimeContainsPlasmoid(event.mimeData, "org.kde.latte.plasmoid");

        var isPlasmoid = event !== undefined
                && event.mimeData !== undefined
                && event.mimeData.formats !== undefined
                && !event.mimeData.hasUrls
                && event.mimeData.formats.indexOf("text/x-plasmoidservicename") === 0;

        dragInfo.isTask = isTask;
        dragInfo.isPlasmoid = isPlasmoid;
        dragInfo.isSeparator = isSeparator;
        dragInfo.isLatteTasks = isLatteTasks;
        dragInfo.onlyLaunchers = latteApplet ? latteApplet.launchersDrop(event) : false;
        dragInfo.computationsAreValid = true;

        if (dragInfo.isTask || plasmoid.immutable || dockIsHidden || visibilityManager.inSlidingIn || visibilityManager.inSlidingOut) {
            event.ignore();
            return;
        }

        //! Send signal AFTER the dragging is confirmed otherwise the restore mask signal from animations
        //! may not be triggered #408926
        animations.needLength.addEvent(dragArea);

        if (latteApplet && (dragInfo.onlyLaunchers || dragInfo.isSeparator || !dragInfo.isPlasmoid)) {
            if (dragInfo.onlyLaunchers) {
                var hoversLatteTasks = root.latteAppletContainer.containsPos(event);
                root.addLaunchersMessage = root.addLaunchersInTaskManager || (!root.addLaunchersInTaskManager && hoversLatteTasks);

                if (root.addLaunchersInTaskManager || hoversLatteTasks) {
                    dndSpacer.opacity = 0;
                    dndSpacer.parent = root;
                    return;
                }
            } else {
                if ((dragInfo.isSeparator || !dragInfo.isPlasmoid) && root.latteAppletContainer.containsPos(event)) {
                    dndSpacer.opacity = 0;
                    dndSpacer.parent = root;
                    return;
                }
            }
        }

        if (!root.ignoreRegularFilesDragging && !dragResistaner.running) {
            if (!isForeground) {
                dragResistaner.start();
            }

            root.layoutManager().insertAtCoordinates2(dndSpacer, event.x, event.y)
            dndSpacer.opacity = 1;
        }
    }

    onDragMove: {
        clearInfoTimer.stop();
        if (dragInfo.isTask) {
            return;
        }

        if (latteApplet && (dragInfo.onlyLaunchers || dragInfo.isSeparator || !dragInfo.isPlasmoid)) {
            if (dragInfo.onlyLaunchers) {
                var hoversLatteTasks = root.latteAppletContainer.containsPos(event);
                root.addLaunchersMessage = root.addLaunchersInTaskManager || (!root.addLaunchersInTaskManager && hoversLatteTasks);

                if (root.addLaunchersInTaskManager || hoversLatteTasks) {
                    dndSpacer.opacity = 0;
                    dndSpacer.parent = root;
                    return;
                }
            } else {
                if ((dragInfo.isSeparator || !dragInfo.isPlasmoid) && root.latteAppletContainer.containsPos(event)) {
                    dndSpacer.opacity = 0;
                    dndSpacer.parent = root;
                    return;
                }
            }
        }

        if (!root.ignoreRegularFilesDragging && !dragResistaner.running) {
            if (!isForeground) {
                dragResistaner.start();
            }

            root.layoutManager().insertAtCoordinates2(dndSpacer, event.x, event.y)
            dndSpacer.opacity = 1;
        }
    }

    Timer {
        id: dragResistaner
        interval: 1000
    }

    onDragLeave: {
        animations.needLength.removeEvent(dragArea);

        root.addLaunchersMessage = false;

        if (!isForeground) {
            dndSpacer.opacity = 0;
            dndSpacer.parent = root;
        }
    }

    onDrop: {
        animations.needLength.removeEvent(dragArea);

        if (root.ignoreRegularFilesDragging && dragInfo.isTask || dockIsHidden || visibilityManager.inSlidingIn || visibilityManager.inSlidingOut) {
            return;
        }

        if (latteApplet && dragInfo.onlyLaunchers && (root.addLaunchersInTaskManager || root.latteAppletContainer.containsPos(event))) {
            latteApplet.launchersDropped(event.mimeData.urls);
        } else {
            plasmoid.processMimeData(event.mimeData, event.x, event.y);
            event.accept(event.proposedAction);
        }

        root.addLaunchersMessage = false;
        dndSpacer.opacity = 0;
    }
}
