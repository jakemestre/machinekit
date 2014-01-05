#!/bin/sh
CONFIG=configs/ARM/BeagleBone/BeBoPr/BeBoPr.ini
sed -i "s/^HOME =  .*$/HOME =  $1/g" $CONFIG
sed -i "s/^HOME_OFFSET =  .*$/HOME_OFFSET =  $1/g" $CONFIG
