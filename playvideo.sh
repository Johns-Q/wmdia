#!/bin/sh
#
#	Play video in wmdia example.
#

## name of our dia window
name=${wmdia:-"wmdia"}

##
##	Set the tooltip of wmdia
##
settooltip() {
    xprop -name $name -format TOOLTIP 8s -set TOOLTIP "$1"
}

##
##	Set the command of wmdia
##
setcommand() {
    xprop -name $name -format COMMAND 8s -set COMMAND "$1"
}

#
#	Find the window id of the wmdia window
#
wid=`xwininfo -name $name |
	sed -n -e '/xwininfo:/ s/.* \(0x[0-9a-fA-F]\+\) .*/\1/p'`

#
#	Play video with mplayer
#
settooltip "Playing video $*"
setcommand ""
# keep aspect ratio
mplayer -wid $wid -vf scale=62:-3 -vo x11 -ao null "$@"
# full sized
#mplayer -wid $wid -zoom -vo x11 -ao null "$@"
