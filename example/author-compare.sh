#!/usr/bin/env dgsh
#
# SYNOPSIS Compare IEEE Software with ICSE authors
# DESCRIPTION
# Obtain the computer science bibliography from the specied compressed
# DBLP URL (e.g. http://dblp.uni-trier.de/xml/dblp.xml.gz) and output the
# number of authors who have published only in IEEE Software, the number
# who have published only in the International Conference on Software
# Engineering, and authors who have published in both.
# Demonstrates the use of dgsh-wrap -e to have sed(1) create two output
# streams and the use of tee to copy a pair of streams into four ones.
#
#  Copyright 2017 Diomidis Spinellis
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

# Extract and sort author names
sorted_authors()
{
  sed -n 's/<author>\([^<]*\)<\/author>/\1/p' |
  sort
}

export -f sorted_authors

if [ -z "$1" ] ; then
  echo "usage: $0 URL" 1>&2
  exit 1
fi

curl -s "$1" |
gzip -dc |
# Output ICSE and IEEE Software authors as two output streams
dgsh-wrap -eO sed -n '
/<inproceedings.*key="conf\/icse\//,/<title>/ w >|
/<article.*key="journals\/software\//,/<title>/ w >|' |
# 2 streams in 4 streams out: ICSE, Software, ICSE, Software
tee |
{{
  {{
    echo -n 'ICSE papers: '
    grep -c '<inproceedings'
    echo -n 'IEEE Software articles: '
    grep -c '<article'
  }}

  {{
    call sorted_authors
    call sorted_authors
  }} |
  comm |
  {{
    echo -n 'Authors only in ICSE: '
    wc -l
    echo -n 'Authors only in IEEE Software: '
    wc -l
    echo -n 'Authors common in both venues: '
    wc -l
  }}
}} |
cat
