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

import QtQuick 2.7
import QtQuick.Layouts 1.0

import org.kde.plasma.plasmoid 2.0
import org.kde.plasma.core 2.0 as PlasmaCore
import org.kde.plasma.components 2.0 as PlasmaComponents
import org.kde.kquickcontrolsaddons 2.0

import QtGraphicalEffects 1.0

MouseArea {
    id: configurationArea

    z: 1000

    width: plasmoid.formFactor === PlasmaCore.Types.Horizontal ? root.width : thickness
    height: plasmoid.formFactor === PlasmaCore.Types.Vertical ? root.height : thickness

    visible: root.inConfigureAppletsMode
    hoverEnabled: root.inConfigureAppletsMode

    focus: true

    cursorShape: {
        if (currentApplet && tooltip.visible && currentApplet.latteStyleApplet) {
            return root.isHorizontal ? Qt.SizeHorCursor : Qt.SizeVerCursor;
        }

        return Qt.ArrowCursor;
    }

    property bool isResizingLeft: false
    property bool isResizingRight: false
    property Item currentApplet
    property Item previousCurrentApplet

    property Item currentHoveredLayout: {
        if (placeHolder.parent !== configurationArea) {
            return placeHolder.parent;
        }

        return currentApplet ? currentApplet.parent : null
    }

    property int lastX
    property int lastY
    property int appletX
    property int appletY

    readonly property int thickness: visibilityManager.thicknessNormalOriginalValue - visibilityManager.extraNormalThickMask
    readonly property int spacerHandleSize: units.smallSpacing

    onHeightChanged: tooltip.visible = false;
    onWidthChanged: tooltip.visible = false;


    function hoveredItem(x, y) {
        //! main layout
        var relevantLayout = mapFromItem(layoutsContainer.mainLayout,0,0);
        var item = layoutsContainer.mainLayout.childAt(x-relevantLayout.x, y-relevantLayout.y);

        if (!item) {
            // start layout
            relevantLayout = mapFromItem(layoutsContainer.startLayout,0,0);
            item = layoutsContainer.startLayout.childAt(x-relevantLayout.x, y-relevantLayout.y);
        }

        if (!item) {
            relevantLayout = mapFromItem(layoutsContainer.endLayout,0,0);
            item = layoutsContainer.endLayout.childAt(x-relevantLayout.x, y-relevantLayout.y);
        }

        return item;
    }

    function relevantLayoutForApplet(curapplet) {
        var relevantLayout;

        if (curapplet.parent === layoutsContainer.mainLayout) {
            relevantLayout = mapFromItem(layoutsContainer.mainLayout, 0, 0);
        } else if (curapplet.parent === layoutsContainer.startLayout) {
            relevantLayout = mapFromItem(layoutsContainer.startLayout, 0, 0);
        } else if (curapplet.parent === layoutsContainer.endLayout) {
            relevantLayout = mapFromItem(layoutsContainer.endLayout, 0, 0);
        }

        return relevantLayout;
    }


    onPositionChanged: {
        if (pressed) {
            var padding = units.gridUnit * 3;
            if (currentApplet && (mouse.x < -padding || mouse.y < -padding ||
                                  mouse.x > width + padding || mouse.y > height + padding)) {
                var newCont = plasmoid.containmentAt(mouse.x, mouse.y);

                if (newCont && newCont != plasmoid) {
                    var newPos = newCont.mapFromApplet(plasmoid, mouse.x, mouse.y);
                    newCont.addApplet(currentApplet.applet, newPos.x, newPos.y);
                    root.dragOverlay.currentApplet = null;
                    return;
                }
            }

            if(currentApplet){
                if (plasmoid.formFactor === PlasmaCore.Types.Vertical) {
                    currentApplet.y += (mouse.y - lastY);
                    //    handle.y = currentApplet.y;
                } else {
                    currentApplet.x += (mouse.x - lastX);
                    //     handle.x = currentApplet.x;
                }
            }

            lastX = mouse.x;
            lastY = mouse.y;

            var item =  hoveredItem(mouse.x, mouse.y);

            if (item && item !== placeHolder) {
                placeHolder.parent = configurationArea;
                var posInItem = mapToItem(item, mouse.x, mouse.y);

                if ((plasmoid.formFactor === PlasmaCore.Types.Vertical && posInItem.y < item.height/2) ||
                        (plasmoid.formFactor !== PlasmaCore.Types.Vertical && posInItem.x < item.width/2)) {
                    root.layoutManagerInsertBefore(item, placeHolder);
                } else {
                    root.layoutManagerInsertAfter(item, placeHolder);
                }
            }

        } else {           
            var item = hoveredItem(mouse.x, mouse.y);

            if (root.dragOverlay && item && item !== lastSpacer) {
                root.dragOverlay.currentApplet = item;
            } else {
                currentApplet = null;
                root.dragOverlay.currentApplet = null;
            }
        }

        if (root.dragOverlay.currentApplet) {
            hideTimer.stop();

            tooltip.visible = true;
            tooltip.raise();
        }
    }

    onExited: hideTimer.restart();

    onCurrentAppletChanged: {
        previousCurrentApplet = currentApplet;

        if (!currentApplet || !root.dragOverlay.currentApplet) {
            hideTimer.restart();
            return;
        }

        var relevantLayout = relevantLayoutForApplet(currentApplet) ;

        if (!relevantLayout) {
            return;
        }

        handle.x = relevantLayout.x + currentApplet.x;
        handle.y = relevantLayout.y + currentApplet.y;
        handle.width = currentApplet.width;
        handle.height = currentApplet.height;

        lockButton.checked = currentApplet.lockZoom;
        colorizingButton.checked = !currentApplet.userBlocksColorizing;

        repositionHandler.restart();
    }

    onPressed: {
        if (!root.dragOverlay.currentApplet) {
            return;
        }

        root.layouter.appletsInParentChange = true;

        var relevantApplet = mapFromItem(currentApplet, 0, 0);
        var rootArea = mapFromItem(root, 0, 0);

        appletX = mouse.x - relevantApplet.x + rootArea.x;
        appletY = mouse.y - relevantApplet.y + rootArea.y;

        lastX = mouse.x;
        lastY = mouse.y;
        placeHolder.width = currentApplet.width;
        placeHolder.height = currentApplet.height;
        handle.width = currentApplet.width;
        handle.height = currentApplet.height;
        root.layoutManagerInsertBefore(currentApplet, placeHolder);
        currentApplet.parent = root;
        currentApplet.x = lastX-appletX;
        currentApplet.y = lastY-appletY;
        currentApplet.z = 900;
    }

    onReleased: {
        if (!root.dragOverlay.currentApplet) {
            return;
        }

        if(currentApplet && currentApplet.applet){
            if (plasmoid.formFactor === PlasmaCore.Types.Vertical) {
                currentApplet.applet.configuration.length = handle.height;
            } else {
                currentApplet.applet.configuration.length = handle.width;
            }
        }

        configurationArea.isResizingLeft = false;
        configurationArea.isResizingRight = false;

        root.layoutManagerInsertBefore(placeHolder, currentApplet);
        placeHolder.parent = configurationArea;
        currentApplet.z = 1;

        var relevantLayout = mapFromItem(layoutsContainer.mainLayout, 0, 0);

        handle.x = relevantLayout.x + currentApplet.x;
        handle.y = relevantLayout.y + currentApplet.y;
        //     handle.width = currentApplet.width;
        //    handle.height = currentApplet.height;
        root.layoutManagerSave();
        root.layoutManagerMoveAppletsBasedOnJustifyAlignment();
        root.layouter.appletsInParentChange = false;
        layouter.updateSizeForAppletsInFill();
    }

    onWheel: {
        if (!currentApplet || !currentApplet.latteStyleApplet) {
            return;
        }

        var angle = wheel.angleDelta.y / 8;

        if (angle > 12)
            currentApplet.latteStyleApplet.increaseLength();
        else if (angle < 12)
            currentApplet.latteStyleApplet.decreaseLength();
    }

    Item {
        id: placeHolder
        visible: configurationArea.containsMouse
        Layout.fillWidth: currentApplet ? currentApplet.Layout.fillWidth : false
        Layout.fillHeight: currentApplet ? currentApplet.Layout.fillHeight : false

        readonly property bool isPlaceHolder: true
    }

    //Because of the animations for the applets the handler can not catch up to
    //reposition itself when the currentApplet position or size changes.
    //This timer fixes that situation
    Timer {
        id: repositionHandler
        interval: 100
        onTriggered: handle.updatePlacement();
    }

    Timer {
        id: hideTimer
        interval: animations.duration.large * 2
        onTriggered: {
            if (!tooltipMouseArea.containsMouse) {
                tooltip.visible = false;
                currentApplet = null;
            }
        }
    }

    Connections {
        target: currentApplet
        onXChanged: handle.updatePlacement();
        onYChanged: handle.updatePlacement();
        onWidthChanged: handle.updatePlacement();
        onHeightChanged: handle.updatePlacement()
    }

    Item {
        id: handle
        visible: currentApplet && (configurationArea.containsMouse || tooltipMouseArea.containsMouse)

        Loader {
            anchors.fill: parent
            active: root.debugMode
            sourceComponent: Rectangle {
                color: "transparent"
                border.width:1
                border.color: "yellow"
            }
        }

        //BEGIN functions
        function updatePlacement(){
            if(currentApplet){
                var transformChoords = configurationArea.mapFromItem(currentApplet, 0, 0)

                handle.x = transformChoords.x;
                handle.y = transformChoords.y;
                handle.width = currentApplet.width;
                handle.height = currentApplet.height;

                repositionHandler.restart();
            }
        }

        //END functions

        Item {
            id: handleVisualItem
            width: root.isHorizontal ? parent.width : thickness
            height: root.isHorizontal ? thickness : parent.height

            readonly property int thickness: root.isHorizontal ? parent.height - metrics.margin.screenEdge : parent.width - metrics.margin.screenEdge

            Rectangle{
                anchors.fill: parent
                color: theme.backgroundColor
                radius: 3
                opacity: 0.35
            }

            PlasmaCore.IconItem {
                source: "transform-move"
                width: Math.min(144, parent.width, parent.height)
                height: width
                anchors.centerIn: parent
                opacity: 0.9
                layer.enabled: graphicsSystem.isAccelerated
                layer.effect: DropShadow {
                    radius: root.appShadowSize
                    fast: true
                    samples: 2 * radius
                    color: root.appShadowColor

                    verticalOffset: 2
                }
            }


            states:[
                State{
                    name: "bottom"
                    when: plasmoid.location === PlasmaCore.Types.BottomEdge

                    AnchorChanges{
                        target: handleVisualItem;
                        anchors.horizontalCenter: parent.horizontalCenter; anchors.verticalCenter: undefined;
                        anchors.right: undefined; anchors.left: undefined; anchors.top: undefined; anchors.bottom: parent.bottom;
                    }
                    PropertyChanges{
                        target: handleVisualItem;
                        anchors.leftMargin: 0;    anchors.rightMargin: 0;     anchors.topMargin:0;    anchors.bottomMargin: metrics.margin.screenEdge;
                        anchors.horizontalCenterOffset: 0; anchors.verticalCenterOffset: 0;
                    }
                },
                State{
                    name: "top"
                    when: plasmoid.location === PlasmaCore.Types.TopEdge

                    AnchorChanges{
                        target: handleVisualItem;
                        anchors.horizontalCenter: parent.horizontalCenter; anchors.verticalCenter: undefined;
                        anchors.right: undefined; anchors.left: undefined; anchors.top: parent.top; anchors.bottom: undefined;
                    }
                    PropertyChanges{
                        target: handleVisualItem;
                        anchors.leftMargin: 0;    anchors.rightMargin: 0;     anchors.topMargin: metrics.margin.screenEdge;    anchors.bottomMargin: 0;
                        anchors.horizontalCenterOffset: 0; anchors.verticalCenterOffset: 0;
                    }
                },
                State{
                    name: "left"
                    when: plasmoid.location === PlasmaCore.Types.LeftEdge

                    AnchorChanges{
                        target: handleVisualItem;
                        anchors.horizontalCenter: undefined; anchors.verticalCenter: parent.verticalCenter;
                        anchors.right: undefined; anchors.left: parent.left; anchors.top: undefined; anchors.bottom: undefined;
                    }
                    PropertyChanges{
                        target: handleVisualItem;
                        anchors.leftMargin: metrics.margin.screenEdge;    anchors.rightMargin: 0;     anchors.topMargin:0;    anchors.bottomMargin: 0;
                        anchors.horizontalCenterOffset: 0; anchors.verticalCenterOffset: 0;
                    }
                },
                State{
                    name: "right"
                    when: plasmoid.location === PlasmaCore.Types.RightEdge

                    AnchorChanges{
                        target: handleVisualItem;
                        anchors.horizontalCenter: undefined; anchors.verticalCenter: parent.verticalCenter;
                        anchors.right: parent.right; anchors.left: undefined; anchors.top: undefined; anchors.bottom: undefined;
                    }
                    PropertyChanges{
                        target: handleVisualItem;
                        anchors.leftMargin: 0;    anchors.rightMargin: metrics.margin.screenEdge;     anchors.topMargin:0;    anchors.bottomMargin: 0;
                        anchors.horizontalCenterOffset: 0; anchors.verticalCenterOffset: 0;
                    }
                }
            ]

        }

        Behavior on opacity {
            NumberAnimation {
                duration: animations.duration.large
                easing.type: Easing.InOutQuad
            }
        }
    }
    PlasmaCore.Dialog {
        id: tooltip
        visualParent: currentApplet

        type: PlasmaCore.Dialog.Dock
        flags: Qt.WindowStaysOnTopHint | Qt.WindowDoesNotAcceptFocus | Qt.BypassWindowManagerHint | Qt.ToolTip
        location: plasmoid.location

        onVisualParentChanged: {
            if (visualParent && currentApplet
                    && (currentApplet.applet || currentApplet.isSeparator || currentApplet.isInternalViewSplitter)) {

                configureButton.visible = !currentApplet.isInternalViewSplitter && (currentApplet.applet.pluginName !== root.plasmoidName)
                        && currentApplet.applet.action("configure") && currentApplet.applet.action("configure").enabled;
                closeButton.visible = !currentApplet.isInternalViewSplitter && currentApplet.applet.action("remove") && currentApplet.applet.action("remove").enabled;
                lockButton.visible = !currentApplet.isInternalViewSplitter
                        && (currentApplet.applet.pluginName !== root.plasmoidName)
                        && !currentApplet.isSeparator;

                colorizingButton.visible = root.colorizerEnabled && !currentApplet.appletBlocksColorizing && !currentApplet.isInternalViewSplitter;

                label.text = currentApplet.isInternalViewSplitter ? i18n("Justify Splitter") : currentApplet.applet.title;
            }
        }

        mainItem: MouseArea {
            id: tooltipMouseArea
            enabled: currentApplet
            width: handleRow.childrenRect.width + (2 * handleRow.spacing)
            height: Math.max(configureButton.height, label.contentHeight, closeButton.height)
            hoverEnabled: true
            LayoutMirroring.enabled: Qt.application.layoutDirection === Qt.RightToLeft
            LayoutMirroring.childrenInherit: true

            onEntered: hideTimer.stop();
            onExited: hideTimer.restart();

            Row {
                id: handleRow
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: 2*units.smallSpacing

                Row{
                    spacing: units.smallSpacing
                    PlasmaComponents.ToolButton {
                        id: configureButton
                        anchors.verticalCenter: parent.verticalCenter
                        iconSource: "configure"
                        tooltip: i18n("Configure applet")
                        onClicked: {
                            tooltip.visible = false;
                            currentApplet.applet.action("configure").trigger();
                        }
                    }

                    PlasmaComponents.Label {
                        id: label
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.rightMargin: units.smallSpacing
                        textFormat: Text.PlainText
                        maximumLineCount: 1
                    }

                    Row{
                        spacing: units.smallSpacing/2

                        PlasmaComponents.ToolButton{
                            id: colorizingButton
                            checkable: true
                            iconSource: "color-picker"
                            tooltip: i18n("Enable painting  for this applet")

                            onCheckedChanged: {
                                currentApplet.userBlocksColorizing = !checked;
                                root.layoutManagerSaveOptions();
                            }
                        }

                        PlasmaComponents.ToolButton{
                            id: lockButton
                            checkable: true
                            iconSource: checked ? "lock" : "unlock"
                            tooltip: i18n("Disable parabolic effect for this applet")

                            onCheckedChanged: {
                                currentApplet.lockZoom = checked;
                                root.layoutManagerSaveOptions();
                            }
                        }

                        PlasmaComponents.ToolButton {
                            id: closeButton
                            anchors.verticalCenter: parent.verticalCenter
                            iconSource: "delete"
                            tooltip: i18n("Remove applet")
                            onClicked: {
                                tooltip.visible = false;
                                if(currentApplet && currentApplet.applet)
                                    currentApplet.applet.action("remove").trigger();
                            }
                        }
                    }
                }
            }
        }
    }

    states: [
        State {
            name: "bottom"
            when: (plasmoid.location === PlasmaCore.Types.BottomEdge)

            AnchorChanges {
                target: configurationArea
                anchors{ top:undefined; bottom:parent.bottom; left:undefined; right:undefined;
                    horizontalCenter:parent.horizontalCenter; verticalCenter:undefined}
            }
        },
        State {
            name: "top"
            when: (plasmoid.location === PlasmaCore.Types.TopEdge)

            AnchorChanges {
                target: configurationArea
                anchors{ top:parent.top; bottom:undefined; left:undefined; right:undefined;
                    horizontalCenter:parent.horizontalCenter; verticalCenter:undefined}
            }
        },
        State {
            name: "left"
            when: (plasmoid.location === PlasmaCore.Types.LeftEdge)

            AnchorChanges {
                target: configurationArea
                anchors{ top:undefined; bottom:undefined; left:parent.left; right:undefined;
                    horizontalCenter:undefined; verticalCenter:parent.verticalCenter}
            }
        },
        State {
            name: "right"
            when: (plasmoid.location === PlasmaCore.Types.RightEdge)

            AnchorChanges {
                target: configurationArea
                anchors{ top:undefined; bottom:undefined; left:undefined; right:parent.right;
                    horizontalCenter:undefined; verticalCenter:parent.verticalCenter}
            }
        }
    ]
}
