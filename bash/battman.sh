#!/bin/bash

for bat in $(upower -e | grep "battery"); do
    n=$(upower -i $bat|grep "native-path"|awk '{print $2}')
    s=$(upower -i $bat|grep "state"|awk '{print $2}')
    e=$(upower -i $bat|grep "time to empty"|awk '{print $4 " " $5}')
    f=$(upower -i $bat|grep "time to full"|awk '{print $4 " " $5}')
    p=$(upower -i $bat|grep "percentage"|awk '{print $2}')
    echo "$n: $s ($p)"
    if [[ $s == "discharging" ]]; then
        if [[ -n $e ]]; then
            echo "    time to empty: $e"
        fi
    elif [[ $s == "charging" ]]; then
        if [[ -n $f ]]; then
            echo "    time to full: $f"
        fi
    fi
done
