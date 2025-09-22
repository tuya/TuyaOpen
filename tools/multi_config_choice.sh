#!/bin/bash

# $1: boards path

BOARDS_PATH=$1
CONFIG_LIST=$(find $BOARDS_PATH/*/config -name "*.config")

echo "========================" >&2
echo "Configs" >&2
INDEX=0
for ITEM in $CONFIG_LIST; do
	if [ x"${ITEM}" != x"" ]; then
		((INDEX++))
		echo "  ${INDEX}. $(basename ${ITEM})" >&2
		eval "CONTENT_${INDEX}=\"${ITEM}\""
	fi
done
echo "------------------------" >&2
echo -n "Please select: " >&2
exec < /dev/tty; read SELECT
eval "echo \${CONTENT_${SELECT}}"
echo "------------------------" >&2
