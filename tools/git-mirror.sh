#!/usr/bin/env bash

set -e
set -u
## repos group
REPOS_ARRAY=(
    armink/FlashDB
    littlefs-project/littlefs
    DaveGamble/cJSON
    FreeRTOS/backoffAlgorithm
    fukuchi/libqrencode
    tuya/TuyaOpen-T2
    tuya/TuyaOpen-T3
    tuya/TuyaOpen-ubuntu
    tuya/TuyaOpen-T5AI
    tuya/TuyaOpen-esp32
    tuya/TuyaOpen-ln882h
    tuya/TuyaOpen-bk7231x
    tuya/arduino-tuyaopen
)

len=${#REPOS_ARRAY[@]}

if [[ "$@" == "" ]]; then
    echo "Usage:"
    echo "    Set the mirror:   ./git-mirror.sh set"
    echo "    Unset the mirror: ./git-mirror.sh unset"
fi

for ((i = 0; i < len; i++))
do
    REPO=${REPOS_ARRAY[i]}
    REPO_NAME=$(echo "$REPO" | cut -d'/' -f2)
    if [[ "$@" == "set" ]]; then
        git config --global url.https://gitee.com/tuya-open/$REPO_NAME.insteadOf https://github.com/$REPO
        git config --global url.https://gitee.com/tuya-open/$REPO_NAME.git.insteadOf https://github.com/$REPO
    elif [[ "$@" == "unset" ]]; then
        git config --global --unset url.https://gitee.com/tuya-open/$REPO_NAME.insteadof
        git config --global --unset url.https://gitee.com/tuya-open/$REPO_NAME.git.insteadof
    fi
done