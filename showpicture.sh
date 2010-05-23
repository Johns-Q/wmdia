#!/bin/sh
##
##	Show picture script example for wmdia.
##
/usr/bin/display -resize 62x62 -bordercolor darkgray -border 31 \
    -gravity center -crop 62x62+0+0 -window ${wmdia:-wmdia} "$1"
