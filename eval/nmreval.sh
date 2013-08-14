#!/bin/sh
#
# Run the NMRPipe sequential vs parallel performance evaluation
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

# Obtain file from:
# http://www.bmrb.wisc.edu/ftp/pub/bmrb/timedomain/bmr6443/timedomain_data/c13-hsqc/june11-se-6426-CA.fid/fid
/usr/bin/time -v -o par-`hostname`-sgsh-nmrpipe ~/src/sgsh/sgsh -p ~/src/sgsh  NMRPipe.sh june11-se-6426-CA.fid/fid
/usr/bin/time -v -o par-`hostname`-file-nmrpipe ~/src/sgsh/sgsh -p ~/src/sgsh  -S NMRPipe.sh june11-se-6426-CA.fid/fid
