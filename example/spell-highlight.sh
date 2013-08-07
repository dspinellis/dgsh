#!/usr/local/bin/sgsh
#
# SYNOPSIS Highlight misspelled words
# DESCRIPTION
# Highlight the words that are misspelled in the command's standard
# input.
# Demonstrates stream buffering and the avoidance of pass-through
# constructs to avoid deadlocks.
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

export LC_ALL=C

scatter |{

	# Ensure dictionary is sorted consistently with our settings
	.| sort /usr/share/dict/words |>/stream/dict

	-| tr -cs A-Za-z \\n |
	tr A-Z a-z |
	sort -u |
	comm -23 - /stream/dict |>/stream/errors

	# Using a pass-through construct would (rightly) result in a
	# deadlock warning, because fgrep needs to read the file in its
	# /stream/errors argument before opening /stream/text, thereby
	# causing the scatter operation to block.
	-| cat |>/stream/text


|} gather |{
	fgrep -f /stream/errors -i --color -w -C 2 /stream/text
|}
