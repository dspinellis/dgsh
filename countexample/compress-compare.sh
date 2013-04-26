#!/bin/sh
#
# Plain sh(1) version of example/compress-compare.sh
#
# SYNOPSIS Compression benchmark
# DESCRIPTION
# Report file type, length, and compression performance for a
# URL retrieved from the web.  The web file never touches the
# disk.
# Demonstrates the use of stores.
#
#  Copyright 2012-2013 Diomidis Spinellis
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

export SGDIR=/tmp/sg-$$

rm -rf $SGDIR

trap '
# Remove temporary directory
rm -rf "$SGDIR"' 0

trap 'exit $?' 1 2 3 15

mkdir $SGDIR

curl -s "$1" >$SGDIR/npi-0.0.0
NBYTES=` <$SGDIR/npi-0.0.0 wc -c`
FILETYPE=` <$SGDIR/npi-0.0.0 file - `
COMPRESS=` <$SGDIR/npi-0.0.0 compress -c | wc -c `
BZIP2=` <$SGDIR/npi-0.0.0 bzip2 -c | wc -c `
GZIP=` <$SGDIR/npi-0.0.0 gzip -c | wc -c `

# Gather the results
	cat <<EOF
File URL:      $1
File type:     $FILETYPE
Original size: $NBYTES bytes
compress:      $COMPRESS bytes
gzip:          $GZIP bytes
bzip2:         $BZIP2 bytes
EOF
