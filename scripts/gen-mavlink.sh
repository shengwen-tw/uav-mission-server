#!/usr/bin/env bash

function ASSERT
{
    $*
    RES=$?
    if [ $RES -ne 0 ]; then
        echo 'Assert failed: "' $* '"'
        exit $RES
    fi
}

PASS_COLOR='\e[32;01m'
NO_COLOR='\e[0m'
function OK
{
    printf " [ ${PASS_COLOR} OK ${NO_COLOR} ]\n"
}

function gen_mavlink
{
    rm -rf /tmp/mavlink
    pushd /tmp/
    ASSERT git clone https://github.com/mavlink/mavlink --recursive
    cd mavlink/
    python3 pymavlink/tools/mavgen.py \
            --lang=C --wire-protocol=2.0 \
            --output=generated/include/mavlink/v2.0 \
            message_definitions/v1.0/common.xml
    popd
    mkdir -p lib/mavlink/
    cp -r /tmp/mavlink/generated/include/mavlink/v2.0/* lib/mavlink/
}

if [ ! -d "lib/mavlink" ]; then
    gen_mavlink && OK
fi
