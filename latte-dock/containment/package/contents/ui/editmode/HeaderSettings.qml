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
import QtQuick.Layouts 1.1

import org.kde.plasma.plasmoid 2.0
import org.kde.plasma.core 2.0 as PlasmaCore
import org.kde.plasma.components 2.0 as PlasmaComponents

import org.kde.latte.core 0.2 as LatteCore

import "controls" as SettingsControls

Item {
    id: headerSettings
    width: plasmoid.formFactor === PlasmaCore.Types.Horizontal ? parent.width : parent.height
    height: thickness

    readonly property bool containsMouse: rearrangeBtn.containsMouse || stickOnBottomBtn.containsMouse || stickOnTopBtn.containsMouse
    readonly property int thickness: rearrangeBtn.implicitHeight

    readonly property bool inSettingsAdvancedMode: latteView && latteView.inSettingsAdvancedMode

    readonly property int headMargin: spacing * 2

    rotation: {
        if (plasmoid.formFactor === PlasmaCore.Types.Horizontal) {
            return  0;
        }

        if (plasmoid.location === PlasmaCore.Types.LeftEdge) {
            return 90;
        } else if (plasmoid.location === PlasmaCore.Types.RightEdge) {
            return -90;
        }
    }

    x: {
        if (plasmoid.formFactor === PlasmaCore.Types.Horizontal) {
            return 0;
        }

        if (plasmoid.location === PlasmaCore.Types.LeftEdge) {
            return visibilityManager.thicknessNormalOriginalValue + ruler.thickness + headMargin * 2 - width/2 + height/2;
        } else if (plasmoid.location === PlasmaCore.Types.RightEdge) {
            return headMargin - width/2 + height/2;
        }
    }

    y: {
        if (plasmoid.formFactor === PlasmaCore.Types.Vertical) {
            return width/2 - height/2;
        }

        if (plasmoid.location === PlasmaCore.Types.BottomEdge) {
            return headMargin;
        } else if (plasmoid.location === PlasmaCore.Types.TopEdge) {
            return parent.height - rearrangeBtn.height - headMargin;
        }
    }

    SettingsControls.Button{
        id: stickOnTopBtn
        visible: root.isVertical && inSettingsAdvancedMode

        text: i18n("Stick On Top");
        tooltip: i18n("Stick maximum available space at top screen edge and ignore any top docks or panels")
        checked: plasmoid.configuration.isStickedOnTopEdge
        iconPositionReversed: (plasmoid.location === PlasmaCore.Types.RightEdge)

        icon: SettingsControls.StickIcon{}

        onPressedChanged: {
            if (pressed) {
                plasmoid.configuration.isStickedOnTopEdge = !plasmoid.configuration.isStickedOnTopEdge;
            }
        }

        states: [
            State {
                name: "generalEdge"
                when: (plasmoid.location !== PlasmaCore.Types.RightEdge)

                AnchorChanges {
                    target: stickOnTopBtn
                    anchors{ top:parent.top; bottom:undefined; left:parent.left; right:undefined; horizontalCenter:undefined; verticalCenter:undefined}
                }
            },
            State {
                name: "rightEdge"
                when: (plasmoid.location === PlasmaCore.Types.RightEdge)

                AnchorChanges {
                    target: stickOnTopBtn
                    anchors{ top:parent.top; bottom:undefined; left:undefined; right:parent.right; horizontalCenter:undefined; verticalCenter:undefined}
                }
            }
        ]
    }


    SettingsControls.Button{
        id: rearrangeBtn
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top

        text: i18n("Rearrange and configure your widgets")
        tooltip: i18n("Feel free to move around your widgets and configure them from their tooltips")
        checked: root.inConfigureAppletsMode
        iconPositionReversed: plasmoid.location === PlasmaCore.Types.RightEdge

        icon: SettingsControls.RearrangeIcon{}

        onPressedChanged: {
            if (LatteCore.WindowSystem.compositingActive && pressed) {
                plasmoid.configuration.inConfigureAppletsMode = !plasmoid.configuration.inConfigureAppletsMode;
            }
        }
    }

    SettingsControls.Button{
        id: stickOnBottomBtn
        visible: root.isVertical && inSettingsAdvancedMode

        text: i18n("Stick On Bottom");
        tooltip: i18n("Stick maximum available space at bottom screen edge and ignore any bottom docks or panels")
        checked: plasmoid.configuration.isStickedOnBottomEdge
        iconPositionReversed: (plasmoid.location !== PlasmaCore.Types.RightEdge)

        icon: SettingsControls.StickIcon{}

        onPressedChanged: {
            if (pressed) {
                plasmoid.configuration.isStickedOnBottomEdge = !plasmoid.configuration.isStickedOnBottomEdge;
            }
        }

        states: [
            State {
                name: "generalEdge"
                when: (plasmoid.location !== PlasmaCore.Types.RightEdge)

                AnchorChanges {
                    target: stickOnBottomBtn
                    anchors{ top:parent.top; bottom:undefined; left:undefined; right:parent.right; horizontalCenter:undefined; verticalCenter:undefined}
                }
            },
            State {
                name: "rightEdge"
                when: (plasmoid.location === PlasmaCore.Types.RightEdge)

                AnchorChanges {
                    target: stickOnBottomBtn
                    anchors{ top:parent.top; bottom:undefined; left:parent.left; right:undefined; horizontalCenter:undefined; verticalCenter:undefined}
                }
            }
        ]
    }
}
