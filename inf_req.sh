#!/bin/bash
while gcat "localhost/test" > /dev/null; do
    gcat "localhost/test/test.gmi" > /dev/null &
    gcat "localhost/" > /dev/null &
    gcat "localhost/index.gmi" > /dev/null &
    # echo "success" > /dev/null
done
