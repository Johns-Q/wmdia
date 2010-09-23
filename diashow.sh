#!/bin/sh
##
##	Dia show script example for wmdia.
##

## delay between pictures
delay=60
## name of our dia window
name=${wmdia:-"wmdia"}
## picture viewer
viewer="/usr/bin/feh"
## or set background
#viewer="/usr/bin/fbsetbg -a"

##
##	Set the tooltip of wmdia
##
settooltip() {
    xprop -name $name -format TOOLTIP 8s -set TOOLTIP "$1" || exit -1
}

##
##	Set the command of wmdia
##
setcommand() {
    xprop -name $name -format COMMAND 8s -set COMMAND "$1" || exit -1
}

##
##	Show picture in wmdia and update tooltip/command
##
showdia() {
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

##
##	Show  recursive all images of directory in random order
##
random_recursive() {
    find "$1" -xdev -type f -readable \( `images` \) -print 2>/dev/null \
    	| sort -R | showinput
}

#	Show only one picture
#showdia wmdia.xpm
#	Show all pictures in background directory
#showdir ~/.fluxbox/backgrounds
#	Show all pictures found on root hard disk.
#recursive /

#	Loop for ever 
viewer="/usr/bin/fbsetbg -a"

while true; do random_recursive ${1:-~/.fluxbox/backgrounds}; done;
