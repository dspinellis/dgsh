#!/usr/local/bin/sgsh
#
# Report file type, length, and compression performance for a
# URL retrieved from the web.  The web file never touches the
# disk.
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

curl -s "$1" |
scatter |{
	-| wc -c |store:NBYTES
	-| file - |store:FILETYPE
	-| compress -c | wc -c |store:COMPRESS
	-| bzip2 -c | wc -c |store:BZIP2
	-| gzip -c | wc -c |store:GZIP
|} gather |{
	cat <<EOF
File URL:      $1
File type:     `store:FILETYPE`
Original size: `store:NBYTES` bytes
compress:      `store:COMPRESS` bytes
gzip:          `store:GZIP` bytes
bzip2:         `store:BZIP2` bytes
EOF
|}
