#!/bin/sh
#
#	Example how to set wmdia tooltip property
#
xprop -name ${wmdia:-"wmdia"} -format TOOLTIP 8s -set TOOLTIP "$*"
