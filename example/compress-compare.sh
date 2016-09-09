#
# SYNOPSIS Compression benchmark
# DESCRIPTION
# Report file type, length, and compression performance for
# data received from the standard input.  The data never touches the
# disk.
# Demonstrates the use of an output multipipe to source many commands
# from one followed by an input multipipe to sink to one command
# the output of many and the use of sgsh-tee that is used both to
# propagate the same input to many commands and collect output from
# many commands orderly in a way that is transparent to users.
#
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

sgsh-tee |
{{
	sgsh-wrap -s echo -n 'File type:' &
	sgsh-wrap file - &

	sgsh-wrap -s echo -n 'Original size:' &
	sgsh-wrap wc -c &

	sgsh-wrap -s echo -n 'xz:' &
	sgsh-wrap xz -c | sgsh-wrap wc -c &

	sgsh-wrap -s echo -n 'bzip2:' &
	sgsh-wrap bzip2 -c | sgsh-wrap wc -c &

	sgsh-wrap -s echo -n 'gzip:' &
	sgsh-wrap gzip -c | sgsh-wrap wc -c &
}} |
sgsh-tee
