#!/bin/bash

# From: https://github.com/felixqin/zbuild/blob/master/sh/listmenu.sh

# $1: title
# $2: height
# $3: width
# $4: menu-height


listmenu_text()
{
	echo "========================" >&2
	echo $1 >&2
	INDEX=0
	while read ITEM; do
		if [ x"${ITEM}" != x"" ]; then
			((INDEX++))
			echo "  ${INDEX}. ${ITEM}" >&2
			eval "CONTENT_${INDEX}=\"${ITEM}\""
		fi
	done
	echo "------------------------" >&2
	echo -n "Please select: " >&2
	exec < /dev/tty; read SELECT
	eval "echo \${CONTENT_${SELECT}}"
	echo "------------------------" >&2
}

listmenu_dialog()
{
	ITEMS=""
	INDEX=0
	while read ITEM; do
		if [ x"${ITEM}" != x"" ]; then
			INDEX=`expr ${INDEX} + 1`
			ITEMS="${ITEMS} \"${ITEM}\""
		fi
	done
	if [ x"$INDEX" = x"1" ]; then
		eval "echo $ITEMS"
	else
		#echo "ITEMS=${ITEMS}" >&2
		eval "dialog --output-fd 1 --title \"$1\" --no-items --menu \"Please Select:\" \"$2\" \"$3\" \"$4\" $ITEMS"
	fi
}


DIALOG=`which dialog`
if [ x"$DIALOG" = x"" ]; then
	listmenu_text "$1" "$2"
else
	listmenu_dialog "$1" "$2" "$3" "$4"
fi

