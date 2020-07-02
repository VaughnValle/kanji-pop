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

import QtQuick 2.1

import org.kde.plasma.core 2.0 as PlasmaCore
import QtGraphicalEffects 1.0

Item{
    id: shadowRoot

    property int shadowDirection: PlasmaCore.Types.BottomEdge
    property int shadowSize: 7
    property real shadowOpacity: 1
    property color shadowColor: "#040404"

    readonly property bool isHorizontal : (shadowDirection !== PlasmaCore.Types.LeftEdge) && (shadowDirection !== PlasmaCore.Types.RightEdge)

    readonly property int implicitWidth: shadow.width
    readonly property int implicitHeight: shadow.height

    Item{
        id: shadow
        width: isHorizontal ? shadowRoot.width + 2*shadowSize : shadowSize
        height: isHorizontal ? shadowSize: shadowRoot.height + 2*shadowSize
        opacity: shadowOpacity

        clip: true

        Rectangle{
            id: editShadow
            width: shadowRoot.width
            height: shadowRoot.height
            color: "white"

            layer.enabled: true
            layer.effect: DropShadow {
                radius: shadowSize
                fast: true
                samples: 2 * radius
                color: shadowRoot.shadowColor
            }
        }

        states: [
            ///topShadow
            State {
                name: "topShadow"
                when: (shadowDirection === PlasmaCore.Types.BottomEdge)

                AnchorChanges {
                    target: editShadow
                    anchors{ top:parent.top; bottom:undefined; left:parent.left; right:undefined}
                }
                PropertyChanges{
                    target: editShadow
                    anchors{ leftMargin: 0; rightMargin:0; topMargin:shadowSize; bottomMargin:0}
                }
            },
            ///bottomShadow
            State {
                name: "bottomShadow"
                when: (shadowDirection === PlasmaCore.Types.TopEdge)

                AnchorChanges {
                    target: editShadow
                    anchors{ top:undefined; bottom:parent.bottom; left:parent.left; right:undefined}
                }
                PropertyChanges{
                    target: editShadow
                    anchors{ leftMargin: 0; rightMargin:0; topMargin:0; bottomMargin:shadowSize}
                }
            },
            ///leftShadow
            State {
                name: "leftShadow"
                when: (shadowDirection === PlasmaCore.Types.RightEdge)

                AnchorChanges {
                    target: editShadow
                    anchors{ top:parent.top; bottom: undefined; left:parent.left; right:undefined}
                }
                PropertyChanges{
                    target: editShadow
                    anchors{ leftMargin: shadowSize; rightMargin:0; topMargin:0; bottomMargin:0}
                }
            },
            ///rightShadow
            State {
                name: "rightShadow"
                when: (shadowDirection === PlasmaCore.Types.LeftEdge)

                AnchorChanges {
                    target: editShadow
                    anchors{top:parent.top; bottom:undefined; left:undefined; right:parent.right}
                }
                PropertyChanges{
                    target: editShadow
                    anchors{ leftMargin: 0; rightMargin:shadowSize; topMargin:0; bottomMargin:0}
                }
            }
        ]
    }
}
