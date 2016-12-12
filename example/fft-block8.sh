#!/usr/bin/env dgsh
#
# SYNOPSIS FFT calculation
# DESCRIPTION
# Calculate the iterative FFT for n = 8 in parallel.
# Demonstrates combined use of permute and multipipe blocks.
#
#  Copyright 2016 Marios Fragkoulis
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

fft-input $1 |
dgsh-tee -p 1,5,3,7,2,6,4,8 |
{{
	{{
		w 1 0 &
		w 1 0 &
	}} |
	dgsh-tee -p 1,3,2,4 |
	{{
		w 2 0 &
		w 2 1 &
	}} &

	{{
		w 1 0 &
		w 1 0 &
	}} |
	dgsh-tee -p 1,3,2,4 |
	{{
		w 2 0 &
		w 2 1 &
	}} &
}} |
dgsh-tee -p 1,5,3,7,2,6,4,8 |
{{
	w 3 0 &

	w 3 1 &

	w 3 2 &

	w 3 3 &
}} |
dgsh-tee -p 1,5,2,6,3,7,4,8 |
cat
