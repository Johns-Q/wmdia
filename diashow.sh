#!/bin/sh
##
##	Dia show script example for wmdia.
##

## delay between pictures
delay=30
## name of our dia window
name="wmdia"
## picture viewer
viewer="/usr/bin/feh"
## or set background
viewer="/usr/bin/fbsetbg -a"

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

##
##	Show picture in wmdia and update tooltip/command
##
showdia() {
    #/usr/bin/display -backdrop -background darkgrey -gravity center -size 62x62
    /usr/bin/display -resize 62x62 -bordercolor darkgray -border 31 \
    	-gravity center -crop 62x62+0+0 -window $name "$1"
    settooltip "$1"
    setcommand "$viewer '$1'"
}

##
##	Read stdin line by line and show the input as dia's.
##
showinput() {
    while true; do
	read n
	[ -z "$n" ] && return
	showdia "$n"
	sleep $delay
    done
}

##
##	Show all files in the directory
##
showdir() {
    for i in "$1/"*; do
	showdia "$i"
	sleep $delay
    done
}

##
##	print all supported image types for find
##
images() {
    for i in jpg jpeg png; do
	echo -iname "*."$i -o;
    done
    echo -iname "*.gif"
}

##
##	Show recursive all images of directory
##
recursive() {
    find "$1" -xdev -type f -readable \( `images` \) -print 2>/dev/null | showinput
}

#	Show all pictures found on root hard disk.
#recursive /
#	Show all pictures in background directory
#showdir	~/.fluxbox/backgrounds

showdia wmdia.xpm
