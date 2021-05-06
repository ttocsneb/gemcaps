#!/bin/bash
while gcat "localhost/test" > /dev/null; do
    echo "success" > /dev/null
done