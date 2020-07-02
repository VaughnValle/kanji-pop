/*
*  Copyright 2020  Michail Vourlakos <mvourlakos@gmail.com>
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

import org.kde.plasma.core 2.0 as PlasmaCore

PlasmaCore.FrameSvgItem {    id: frame
    property string basePrefix: "normal"

    imagePath: "widgets/tabbar"
    rotation: 0

    opacity: 1

    prefix: {
        if (!indicator.isActive) {
            return "";
        }

        if (plasmoid.location === PlasmaCore.Types.LeftEdge) {
            return "west-active-tab";
        }

        if (plasmoid.location === PlasmaCore.Types.TopEdge) {
            return "north-active-tab";
        }

        if (plasmoid.location === PlasmaCore.Types.RightEdge) {
            return "east-active-tab";
        }

        if (plasmoid.location === PlasmaCore.Types.BottomEdge) {
            return "south-active-tab";
        }

        return  "south-active-tab";
    }

    states: [
        State {
            name: "launcher"
            when: indicator.isLauncher || (indicator.isApplet && !indicator.isActive)

            PropertyChanges {
                target: frame
                basePrefix: ""
            }
        },
        State {
            name: "hovered"
            when: indicator.isHovered && frame.hasElementPrefix("hover")

            PropertyChanges {
                target: frame
                basePrefix: "hover"
            }
        },
        State {
            name: "attention"
            when: indicator.inAttention

            PropertyChanges {
                target: frame
                basePrefix: "attention"
            }
        },
        State {
            name: "minimized"
            when: indicator.isMinimized

            PropertyChanges {
                target: frame
                basePrefix: "minimized"
            }
        },
        State {
            name: "active"
            when: indicator.isActive

            PropertyChanges {
                target: frame
                basePrefix: "focus"
            }
        }
    ]
}
