/*
 *   Copyright 2016 Marco Martin <mart@kde.org>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Library General Public License as
 *   published by the Free Software Foundation; either version 2, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU Library General Public License for more details
 *
 *   You should have received a copy of the GNU Library General Public
 *   License along with this program; if not, write to the
 *   Free Software Foundation, Inc.,
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

import QtQuick 2.5
import QtQuick.Layouts 1.3
import QtQuick.Templates 2.2 as T
import org.kde.plasma.core 2.0 as PlasmaCore

import org.kde.latte.components 1.0 as LatteComponents

import "private" as Private

T.CheckDelegate {
    id: control

    implicitWidth: contentItem.implicitWidth + leftPadding + rightPadding
    implicitHeight: Math.max(contentItem.implicitHeight,
                             indicator ? indicator.implicitHeight : 0) + topPadding + bottomPadding
    hoverEnabled: true

    topPadding: margin
    bottomPadding: margin
    leftPadding: margin
    rightPadding: margin
    spacing: units.smallSpacing

    property bool blankSpaceForEmptyIcons: false
    property string icon
    property string iconToolTip
    property bool iconOnlyWhenHovered

    property int textHorizontalAlignment: Text.AlignLeft

    readonly property bool isHovered: hovered || iconMouseArea.containsMouse
    readonly property int margin: 4

    contentItem: RowLayout {
        Layout.leftMargin: control.mirrored ? (control.indicator ? control.indicator.width : 0) + control.spacing : 0
        Layout.rightMargin: !control.mirrored ? (control.indicator ? control.indicator.width : 0) + control.spacing : 0
        spacing: units.smallSpacing
        enabled: control.enabled

        Rectangle {
            Layout.minimumWidth: parent.height
            Layout.maximumWidth: parent.height
            Layout.minimumHeight: parent.height
            Layout.maximumHeight: parent.height
            visible: icon && (!control.iconOnlyWhenHovered || (control.iconOnlyWhenHovered && control.isHovered))
            color: control.iconToolTip && iconMouseArea.containsMouse ? theme.highlightColor : "transparent"

            PlasmaCore.IconItem {
                id: iconElement
                anchors.fill: parent
                colorGroup: PlasmaCore.Theme.ButtonColorGroup
                source: control.icon
            }

            LatteComponents.ToolTip{
                parent: iconElement
                text: iconToolTip
                visible: iconMouseArea.containsMouse
                delay: 6 * units.longDuration
            }

            MouseArea {
                id: iconMouseArea
                anchors.fill: parent
                hoverEnabled: true
                visible: control.iconToolTip

                onClicked: control.ListView.view.iconClicked(index);
            }
        }

        Rectangle {
            //blank space when no icon is shown
            Layout.minimumHeight: parent.height
            Layout.minimumWidth: parent.height
            visible: control.blankSpaceForEmptyIcons && (!icon || (control.iconOnlyWhenHovered && !control.isHovered) )
            color: "transparent"
        }

        Label {
            Layout.fillWidth: true
            text: control.text
            font: control.font
            color: theme.viewTextColor
            elide: Text.ElideRight
            visible: control.text
            horizontalAlignment: control.textHorizontalAlignment
            verticalAlignment: Text.AlignVCenter
        }
    }

    //background: Private.DefaultListItemBackground {}
    background: Rectangle {
        visible: control.ListView.view ? control.ListView.view.highlight === null : true
        enabled: control.enabled
        opacity: {
            if (control.highlighted || control.pressed) {
                return 0.6;
            } else if (control.isHovered && !control.pressed) {
                return 0.3;
            }

            return 0;
        }

        color: theme.highlightColor
    }
}
