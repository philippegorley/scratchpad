#!/bin/bash

# first test
as="some_string:another_string:gas-preprocessor.pl"
test "${as#*gas-preprocessor.pl}" != "$as" || echo "woop woop 1"

as="some_string:another_string"
test "${as#*gas-preprocessor.pl}" != "$as" || echo "woop woop 2"

