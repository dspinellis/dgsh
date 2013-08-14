#!/bin/sh
#
# Run the f2d sequential vs parallel performance evaluation
#
#  Copyright 2013 Diomidis Spinellis
#
#  Licensed under the Apache License, Version 2.0 (the "License");
#  you may not use this file except in compliance with the License.
#  You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
#  Unless required by applicable law or agreed to in writing, software
#  distributed under the License is distributed on an "AS IS" BASIS,
#  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#  See the License for the specific language governing permissions and
#  limitations under the License.
#

# Compare sgsh with file-based
/usr/bin/time -v -o time/par-`hostname`-sgsh-ft2d ../sgsh -p .. ft2d.sh
/usr/bin/time -v -o time/par-`hostname`-file-ft2d ../sgsh -p .. -S ft2d.sh

# Compare with native tool
cp time/par-`hostname`-sgsh-ft2d time/ft2d-`hostname`-sgsh
rm -rf Fig
/usr/bin/time -v -o time/ft2d-`hostname`-scons scons
rm -rf *.rsf *.vpl Fig *.rsf\@ .sconsign.dbhash
