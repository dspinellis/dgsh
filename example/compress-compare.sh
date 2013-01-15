#!/usr/local/bin/sgsh
#
# Report file type, length, and compression performance for a
# URL retrieved from the web.  The web file never touched the
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

scatter |{
	curl $1 |{
		-| wc -c |= NBYTES
		-| file - |= FILETYPE
		-| compress -c | wc -c |= COMPRESS
		-| bzip2 -c | wc -c |= BZIP2
		-| gzip -c | wc -c |= GZIP
	|}
|} gather |{
	cat <<EOF
File URL:      $1
File type:     $FILETYPE
Original size: $NBYTES bytes
compress:      $COMPRESS bytes
gzip:          $GZIP bytes
bzip2:         $BZIP2 bytes
EOF
|}
