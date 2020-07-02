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

import org.kde.plasma.plasmoid 2.0

///////Activate animation/////
SequentialAnimation{
    id: clickedAnimation

    property bool pressed: taskItem.pressed
    property int speed: taskItem.animations.speedFactor.current * taskItem.animations.duration.large
    property real maxMScale: Math.max(1,taskItem.parabolic.factor.zoom - (taskItem.parabolic.factor.zoom - 1) / 2)

    ParallelAnimation{
        PropertyAnimation {
            target: brightnessTaskEffect
            property: "brightness"
            to: -0.5
            duration: clickedAnimation.speed
            easing.type: Easing.OutQuad
        }
       /* PropertyAnimation {
            target: wrapper
            property: "mScale"
            to: root.taskInAnimation ? 1 : Math.max(clickedAnimation.maxMScale, wrapper.mScale - (taskItem.parabolic.factor.zoom - 1) / 2)
            duration: clickedAnimation.speed
            easing.type: Easing.OutQuad
        }*/
    }

    ParallelAnimation{
        PropertyAnimation {
            target: brightnessTaskEffect
            property: "brightness"
            to: 0
            duration: clickedAnimation.speed
            easing.type: Easing.OutQuad
        }
      /*  PropertyAnimation {
            target: wrapper
            property: "mScale"
            to: root.taskInAnimation ? 1 : taskItem.parabolic.factor.zoom
            duration: clickedAnimation.speed
            easing.type: Easing.OutQuad
        }*/
    }


    onPressedChanged: {
        if(!running && pressed && !indicators.info.providesClickedAnimation &&
                ((taskItem.lastButtonClicked == Qt.LeftButton)||(taskItem.lastButtonClicked == Qt.MidButton)) ){
            //taskItem.animationStarted();
            start();
        }
    }

    onStopped: {
        if( !taskItem.isDragged){
            //taskItem.animationEnded();
            if(!root.latteView)
                checkListHovered.startDuration(6 * taskItem.animations.duration.large);
        }
    }
}
