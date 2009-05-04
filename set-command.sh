#!/bin/sh
#
#	Example how to set wmdia command property
#
xprop -name wmdia -format COMMAND 8s -set COMMAND "$*"
