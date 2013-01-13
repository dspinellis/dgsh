#!/bin/sh
# Prevent blocked pipes
(tac | tac || tail -r | tail -r) 2>/dev/null
