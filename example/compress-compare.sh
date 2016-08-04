#
# SYNOPSIS Compression benchmark
# DESCRIPTION
# Report file type, length, and compression performance for
# data received from the standard input.  The data never touches the
# disk.
# Demonstrates the use of stores.
#
#  Copyright 2012-2013 Diomidis Spinellis
#
#  Licensed under the Apache License, Version 2.0 (the "License");
#  you may not use this file except in compliance with the License.
#  You may obtain a copy of the License at
#
#      http:/www.apache.org/licenses/LICENSE-2.0
#
#  Unless required by applicable law or agreed to in writing, software
#  distributed under the License is distributed on an "AS IS" BASIS,
#  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#  See the License for the specific language governing permissions and
#  limitations under the License.
#

sgsh-wrap cat $1 |
sgsh-tee | {{
	sgsh-wrap echo -n 'File type:' &
	sgsh-wrap file - &

	sgsh-wrap echo -n 'Original size:' &
	sgsh-wrap wc -c &

	sgsh-wrap echo -n 'xz:' &
	sgsh-wrap xz -c | sgsh-wrap wc -c &

	sgsh-wrap echo -n 'bzip2:' &
	sgsh-wrap bzip2 -c | sgsh-wrap wc -c &

	sgsh-wrap echo -n 'gzip:' &
	sgsh-wrap gzip -c | sgsh-wrap wc -c &
}} | sgsh-tee
