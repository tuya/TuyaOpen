#!/usr/bin/env bash
##
# @file util_tools.sh
# @brief
# @author Tuya
# @version 1.0.0
# @date 2023-03-26
#/


declare -A fore
declare -A back

fore=([reset]=0 [black]=30 [red]=31 [green]=32 [yellow]=33 [blue]=34 [purple]=35 [cyan]=36 [white]=37)
back=([reset]=0 [black]=40 [red]=41 [green]=42 [yellow]=43 [blue]=44 [purple]=45 [cyan]=46 [white]=47)
##/
#$1: color
#$2: message
#/
function show()
{
  printf "\033[$1m$2\033[0m\n"
}

##/
#$1: assert object
#$2: error message
#/
function assert_non_null()
{
    if [ x"$1" = x"" ]; then
        show "${fore[red]};${back[black]}" "$2"
    fi
}

##/
# $1: cmd
# $2: minimum version
# $3: how to get the version
#/
function check_command_version()
{
    NOTE_COMMAND="Please install [$1], and version > [$2]."
    HAS=`which $1`
    if [ -z ${HAS} ]; then
        show "${fore[red]};${back[black]}" "$NOTE_COMMAND"
        return 1
    fi
    VER=`$3 | grep -o "[0-9]\+\.[0-9]\+\(\.[0-9]\+\)\?"`
    VER_LT=$(echo "$VER $2" | tr " " "\n" | sort -V | head -n 1)
    if [ "$2" != "$VER_LT" ]; then
        NOTE_VERSION="Please update [$1]($VER) > [$2]."
        show "${fore[yellow]};${back[black]}" "$NOTE_VERSION"
        return 1
    fi

    NOTE_OK="Check [$1]($VER) > [$2]: OK."
    show "${fore[green]}" "$NOTE_OK"
    return 0
}
