#!/bin/sh
#
# Run sequential vs parallel performance evaluations
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

if ! [ -d usr ]
then
	curl http://ftp-archive.freebsd.org/pub/FreeBSD-Archive/old-releases/i386/9.0-RELEASE/src.txz |
	xz -dc |
	tar xf -
fi

if [ -d /opt/aws ]
then
	IID=`curl -s http://169.254.169.254/2011-01-01/meta-data/instance-id`
	TYPE=`ec2-describe-instance-attribute $IID --instance-type | awk '{print $3}'`
else
	TYPE=`hostname`
fi

mkdir -p time out err

for flags in '' -S
do
	if [ "$flags" = -S ]
	then
		PREF=par-$TYPE-file
	else
		PREF=par-$TYPE-sgsh
	fi

	CODE=usr

	DESC=par-$PREF-metrics
	/usr/bin/time -v -o time/$DESC ../sgsh $flags -p .. ../example/code-metrics.sh $CODE >out/$DESC 2>err/$DESC

	DESC=$PREF-dup
	/usr/bin/time -v -o time/$DESC ../sgsh $flags -p .. ../example/duplicate-files.sh $CODE >out/$DESC 2>err/$DESC

	DESC=$PREF-compress
	tar cf - $CODE |
	/usr/bin/time -v -o time/$DESC ../sgsh $flags -p .. ../example/compress-compare.sh >out/$DESC 2>err/$DESC
done
