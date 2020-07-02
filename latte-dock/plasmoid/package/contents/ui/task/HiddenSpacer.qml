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

import org.kde.latte.core 0.2 as LatteCore

Item{
    id: hiddenSpacer
    //we add one missing pixel from calculations
    width: root.vertical ? wrapper.width : nHiddenSize
    height: root.vertical ? nHiddenSize : wrapper.height

    visible: (rightSpacer ? index === taskItem.indexer.lastVisibleItemIndex : index === taskItem.indexer.firstVisibleItemIndex)
             || (separatorSpace > 0) || taskItem.inAttentionAnimation
             || taskItem.inFastRestoreAnimation || taskItem.inMimicParabolicAnimation

    property bool neighbourSeparator: rightSpacer ? taskItem.headItemIsSeparator : taskItem.tailItemIsSeparator
    //in case there is a neighbour separator, lastValidIndex is used in order to protect from false
    //when the task is removed
    property int indexUsed: index === -1 ? lastValidIndex : index

    //fix #846,empty tasks after activity changes
    //in some cases after activity changes some tasks
    //are shown empty because some ghost tasks are created.
    //This was tracked down to hidden TaskItems spacers.
    //the flag !root.inActivityChange protects from this
    //and it is used later on Behaviors in order to not break
    //the activity change animations from removal/additions of tasks
    //! && !root.inActivityChange (deprecated) in order to check if it is fixed
    property int separatorSpace: neighbourSeparator && !isSeparator && root.parabolicEffectEnabled
                                 && !(taskItem.indexer.separators.length>0 && root.dragSource) ?
                                     ((LatteCore.Environment.separatorLength/2)+taskItem.metrics.margin.length) : 0

    property bool rightSpacer: false

    property real nScale: 0
    property real nHiddenSize: 0

    Binding{
        target: hiddenSpacer
        property: "nHiddenSize"
        value: {
            if (isForcedHidden) {
                return 0;
            } else if (!inAttentionAnimation && !inMimicParabolicAnimation && !inFastRestoreAnimation) {
                return (nScale > 0) ? (taskItem.spacersMaxSize * nScale) + separatorSpace : separatorSpace;
            } else {
                return (nScale > 0) ? (taskItem.metrics.iconSize * nScale) + separatorSpace : separatorSpace;
            }
        }
    }

    Connections{
        target: taskItem
        onContainsMouseChanged: {
            if (!taskItem.containsMouse && !inAttentionAnimation && !inFastRestoreAnimation && !inMimicParabolicAnimation) {
                hiddenSpacer.nScale = 0;
            }
        }
    }

    Behavior on nHiddenSize {
        id: animatedBehavior
        enabled: (taskItem.inFastRestoreAnimation || showWindowAnimation.running || restoreAnimation.running
                  || root.inActivityChange || taskItem.inRemoveStage)
                 || (taskItem.containsMouse && inAttentionAnimation && wrapper.mScale!==taskItem.parabolic.factor.zoom)
        NumberAnimation{ duration: 3 * taskItem.animationTime }
    }

    Behavior on nHiddenSize {
        id: directBehavior
        enabled: !animatedBehavior.enabled
        NumberAnimation { duration: 0 }
    }

    Loader{
        active: latteView && latteView.debugModeSpacers

        sourceComponent: Rectangle{
            width: !root.vertical ? hiddenSpacer.width : 1
            height: !root.vertical ? 1 : hiddenSpacer.height
            x: root.vertical ? hiddenSpacer.width/2 : 0
            y: !root.vertical ? hiddenSpacer.height/2 : 0

            border.width: 1
            border.color: "red"
            color: "transparent"
        }
    }
}
