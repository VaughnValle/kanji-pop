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

import QtQuick 2.1
import QtGraphicalEffects 1.0

import org.kde.plasma.plasmoid 2.0
import org.kde.plasma.core 2.0 as PlasmaCore
import org.kde.plasma.components 2.0 as PlasmaComponents

import org.kde.latte.core 0.2 as LatteCore
import org.kde.latte.components 1.0 as LatteComponents

import "../applet" as Applet

Item{
    id: editVisual
    width: root.isHorizontal ? (latteView ? latteView.width : root.width) :
                               visibilityManager.thicknessEditMode
    height: root.isVertical ? (latteView ? latteView.height : root.height) :
                              visibilityManager.thicknessEditMode

    visible: editVisual.inEditMode

    readonly property int settingsThickness: settingsOverlay.thickness

    property int speed: LatteCore.WindowSystem.compositingActive ? animations.speedFactor.normal*3.6*animations.duration.large : 10
    property int thickness: visibilityManager.thicknessEditMode + root.editShadow
    property int rootThickness: visibilityManager.thicknessZoomOriginal + root.editShadow //- visibilityManager.thicknessEditMode
    property int editLength: root.isHorizontal ? (root.behaveAsPlasmaPanel ? root.width - metrics.maxIconSize/4 : root.width)://root.maxLength) :
                                                 (root.behaveAsPlasmaPanel ? root.height - metrics.maxIconSize/4 : root.height)

    property bool farEdge: (plasmoid.location===PlasmaCore.Types.BottomEdge) || (plasmoid.location===PlasmaCore.Types.RightEdge)
    property bool editAnimationEnded: false
    property bool editAnimationInFullThickness: false
    property bool editAnimationRunning: false
    property bool plasmaEditMode: plasmoid.userConfiguring
    property bool inEditMode: false

    property rect efGeometry

    readonly property real appliedOpacity: imageTiler.opacity
    readonly property real maxOpacity: root.inConfigureAppletsMode || !LatteCore.WindowSystem.compositingActive ?
                                           1 : plasmoid.configuration.editBackgroundOpacity

    LatteComponents.ExternalShadow{
        id: editExternalShadow
        width: root.isHorizontal ? imageTiler.width : root.editShadow
        height: root.isHorizontal ? root.editShadow : imageTiler.height
        visible: !editTransition.running && root.editMode && LatteCore.WindowSystem.compositingActive

        shadowSize: root.editShadow
        shadowOpacity: Math.max(0.35, imageTiler.opacity)
        shadowDirection: plasmoid.location

        states: [
            ///topShadow
            State {
                name: "topShadow"
                when: (plasmoid.location === PlasmaCore.Types.BottomEdge)

                AnchorChanges {
                    target: editExternalShadow
                    anchors{ top:undefined; bottom:imageTiler.top; left:undefined; right:undefined;
                        horizontalCenter:imageTiler.horizontalCenter; verticalCenter:undefined}
                }
            },
            ///bottomShadow
            State {
                name: "bottomShadow"
                when: (plasmoid.location === PlasmaCore.Types.TopEdge)

                AnchorChanges {
                    target: editExternalShadow
                    anchors{ top:imageTiler.bottom; bottom:undefined; left:undefined; right:undefined;
                        horizontalCenter:imageTiler.horizontalCenter; verticalCenter:undefined}
                }
            },
            ///leftShadow
            State {
                name: "leftShadow"
                when: (plasmoid.location === PlasmaCore.Types.RightEdge)

                AnchorChanges {
                    target: editExternalShadow
                    anchors{ top:undefined; bottom:undefined; left:undefined; right:imageTiler.left;
                        horizontalCenter:undefined; verticalCenter:imageTiler.verticalCenter}
                }
            },
            ///rightShadow
            State {
                name: "rightShadow"
                when: (plasmoid.location === PlasmaCore.Types.LeftEdge)

                AnchorChanges {
                    target: editExternalShadow
                    anchors{ top:undefined; bottom:undefined; left:imageTiler.right; right:undefined;
                        horizontalCenter:undefined; verticalCenter:imageTiler.verticalCenter}
                }
            }
        ]
    }

    Image{
        id: imageTiler
        anchors.centerIn: parent
        width: parent.width
        height: parent.height
        opacity: 0

        fillMode: Image.Tile
        source: {
            if (hasBackground) {
                return viewLayout.background;
            }

            return viewLayout ? "../../icons/"+viewLayout.background+"print.jpg" : "../../icons/blueprint.jpg"
        }

        readonly property bool hasBackground: (viewLayout && viewLayout.background.startsWith("/")) ? true : false

        Connections {
            target: editVisual

            onMaxOpacityChanged: {
                if (editVisual.editAnimationEnded) {
                    imageTiler.opacity = editVisual.maxOpacity;
                }
            }
        }

        Behavior on opacity {
            enabled: editVisual.editAnimationEnded
            NumberAnimation {
                duration: 0.8 * animations.duration.proposed
                easing.type: Easing.OutCubic
            }
        }
    }

    MouseArea {
        id: editBackMouseArea
        anchors.fill: imageTiler
        visible: editModeVisual.editAnimationEnded && !root.inConfigureAppletsMode
        hoverEnabled: true

        property bool wheelIsBlocked: false;
        readonly property double opacityStep: 0.1
        readonly property string tooltip: i18nc("opacity for background under edit mode, %0% is opacity percentage",
                                                "You can use mouse wheel to change background opacity of %0%").arg(Math.round(plasmoid.configuration.editBackgroundOpacity * 100))

        onWheel: {
            processWheel(wheel);
        }


        function processWheel(wheel) {
            if (wheelIsBlocked) {
                return;
            }

            wheelIsBlocked = true;
            scrollDelayer.start();

            var angle = wheel.angleDelta.y / 8;

            if (angle > 10) {
                plasmoid.configuration.editBackgroundOpacity = Math.min(100, plasmoid.configuration.editBackgroundOpacity + opacityStep)
            } else if (angle < -10) {
                plasmoid.configuration.editBackgroundOpacity = Math.max(0, plasmoid.configuration.editBackgroundOpacity - opacityStep)
            }
        }

        Connections {
            target: root
            onEmptyAreasWheel: {
                if (root.editMode && !root.inConfigureAppletsMode) {
                    editBackMouseArea.processWheel(wheel);
                }
            }
        }

        //! A timer is needed in order to handle also touchpads that probably
        //! send too many signals very fast. This way the signals per sec are limited.
        //! The user needs to have a steady normal scroll in order to not
        //! notice a annoying delay
        Timer{
            id: scrollDelayer

            interval: 80
            onTriggered: editBackMouseArea.wheelIsBlocked = false;
        }
    }

    PlasmaComponents.Button {
        anchors.fill: editBackMouseArea
        opacity: 0
        tooltip: editBackMouseArea.tooltip
    }

    //! Settings Overlay
    SettingsOverlay {
        id: settingsOverlay
        anchors.fill: parent
        visible: root.editMode
    }

    Applet.TitleTooltipParent {
        id: titleTooltipParent
        metrics: root.metrics
        parabolic: root.parabolic
        minimumThickness: visibilityManager.thicknessEditMode
        maximumThickness: root.inConfigureAppletsMode ? visibilityManager.thicknessEditMode : 9999
    }

    Connections{
        target: root
        onThemeColorsChanged: imageTiler.opacity = editVisual.maxOpacity
    }

    Connections{
        target: plasmoid
        onLocationChanged: initializeEditPosition();
    }

    onInEditModeChanged: {
        if (inEditMode) {
            latteView.visibility.addBlockHidingEvent("EditVisual[qml]::inEditMode()");
        } else {
            latteView.visibility.removeBlockHidingEvent("EditVisual[qml]::inEditMode()");
            if (latteView.visibility.isHidden) {
                latteView.visibility.mustBeShown();
            }
        }
    }

    onRootThicknessChanged: {
        initializeEditPosition();
    }

    onThicknessChanged: {
        initializeEditPosition();
    }

    onXChanged: updateEffectsArea();
    onYChanged: updateEffectsArea();

    onWidthChanged: {
        /*if (root.isHorizontal) {
            initializeEditPosition();
        }*/

        updateEffectsArea();
    }

    onHeightChanged: {
       /* if (root.isVertical) {
            initializeEditPosition();
        }*/

        updateEffectsArea();
    }

    function updateEffectsArea(){
       if (LatteCore.WindowSystem.compositingActive ||
               !latteView || state !== "edit" || !editAnimationEnded) {
            return;
       }

        var rootGeometry = mapToItem(root, 0, 0);

        efGeometry.x = rootGeometry.x;
        efGeometry.y = rootGeometry.y;
        efGeometry.width = width;
        efGeometry.height = height;

        latteView.effects.rect = efGeometry;
    }


    function initializeNormalPosition() {
        if (plasmoid.location === PlasmaCore.Types.BottomEdge) {
            y = rootThickness;
            x = 0;
        } else if (plasmoid.location === PlasmaCore.Types.RightEdge) {
            x = rootThickness;
            y = 0;
        } else if (plasmoid.location === PlasmaCore.Types.LeftEdge) {
            x = -editVisual.thickness;
            y = 0;
        } else if (plasmoid.location === PlasmaCore.Types.TopEdge) {
            y = -editVisual.thickness;
            x = 0;
        }
    }

    function initializeEditPosition() {
        if (root.editMode) {
            if (plasmoid.location === PlasmaCore.Types.LeftEdge){
                x = 0;
                y = 0;
            } else if (plasmoid.location === PlasmaCore.Types.TopEdge) {
                x = 0;
                y = 0;
            } else if (plasmoid.location === PlasmaCore.Types.BottomEdge) {
                x = 0;
                y = rootThickness - thickness;
            } else if (plasmoid.location === PlasmaCore.Types.RightEdge) {
                x = rootThickness - thickness;
                y = 0;
            }
        }
    }

    //////////// States ////////////////////

    states: [
        State{
            name: "*"
            //! since qt 5.14 default state can not use "when" property
            //! it breaks restoring transitions otherwise
        },

        State{
            name: "edit"
            when: plasmaEditMode
        }
    ]

    transitions: [
        Transition{
            id: editTransition
            from: "*"
            to: "edit"

            SequentialAnimation{
                id:normalAnimationTransition
                ScriptAction{
                    script:{
                        editVisual.editAnimationRunning = true;
                        editVisual.inEditMode = true;
                        imageTiler.opacity = 0
                        editVisual.editAnimationEnded = false;

                        initializeNormalPosition();

                        animations.needLength.addEvent(editVisual);
                    }
                }

                PauseAnimation{
                    id: pauseAnimation
                    //! give the time to CREATE the settings windows and not break
                    //! the sliding in animation
                    duration: animations.active ? 100 : 0
                }

                ParallelAnimation{
                    PropertyAnimation {
                        target: imageTiler
                        property: "opacity"
                        to: plasmoid.configuration.inConfigureAppletsMode ? 1 : editVisual.maxOpacity
                        duration: Math.max(0, editVisual.speed - pauseAnimation.duration)
                        easing.type: Easing.InQuad
                    }

                    PropertyAnimation {
                        target: editVisual
                        property: root.isHorizontal ? "y" : "x"
                        to: editVisual.farEdge ? editVisual.rootThickness - editVisual.thickness : 0
                        duration: Math.max(0, editVisual.speed - pauseAnimation.duration)
                        easing.type: Easing.Linear
                    }
                }

                ScriptAction{
                    script:{
                        editVisual.editAnimationEnded = true;
                        editVisual.editAnimationInFullThickness = true;
                        updateEffectsArea();
                        autosize.updateIconSize();
                        visibilityManager.updateMaskArea();
                        editVisual.editAnimationRunning = false;
                    }
                }
            }
        },
        Transition{
            from: "edit"
            to: "*"
            SequentialAnimation{
                ScriptAction{
                    script: {
                        editVisual.editAnimationRunning = true;
                        editVisual.editAnimationInFullThickness = false;
                        //! remove kwin effects when starting the animation
                        latteView.effects.rect = Qt.rect(-1, -1, 0, 0);
                    }
                }

                PauseAnimation{
                    id: pauseAnimation2
                    //! give the time to DELETE the settings windows and not break
                    //! the sliding out animation
                    duration: animations.isActive ? 100 : 0
                }

                ParallelAnimation{
                    PropertyAnimation {
                        target: editVisual
                        property: root.isHorizontal ? "y" : "x"
                        to: editVisual.farEdge ? editVisual.rootThickness : -editVisual.thickness
                        duration: Math.max(0,editVisual.speed - pauseAnimation2.duration)
                        easing.type: Easing.Linear
                    }
                    PropertyAnimation {
                        target: imageTiler
                        property: "opacity"
                        to: 0
                        duration: Math.max(0,editVisual.speed - pauseAnimation2.duration)
                        easing.type: Easing.InQuad
                    }
                }

                ScriptAction{
                    script:{
                        editVisual.inEditMode = false;
                        editVisual.editAnimationEnded = false;
                        animations.needLength.removeEvent(editVisual);

                        //! That part was at the end of the Containers sliding-out animation
                        //! but it looks much better here

                        if (visibilityManager.inTempHiding) {
                            visibilityManager.sendSlidingOutAnimationEnded();
                        }
                        editVisual.editAnimationRunning = false;
                    }
                }
            }

        }
    ]
}
