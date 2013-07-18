#!/bin/sh
#
# Run performance evaluations
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

test -f clarknet_access_log_Aug28.gz || wget ftp://ita.ee.lbl.gov/traces/clarknet_access_log_Aug28.gz
test -f clarknet_access_log_Sep4.gz || wget ftp://ita.ee.lbl.gov/traces/clarknet_access_log_Sep4.gz

if [ -d /opt/aws ]
then
	IID=`curl -s http://169.254.169.254/2011-01-01/meta-data/instance-id`
	TYPE=`ec2-describe-instance-attribute $IID --instance-type | awk '{print $3}'`
else
	TYPE=`hostname`
fi

mkdir -p time out err

# Log grow factor
GROW=1
while :
do
	for PROG in sgsh perl
	do
		DESC=web-$TYPE-$PROG-$GROW
		for i in clarknet*.gz
		do
			gzip -dc $i
		done |
		/usr/bin/perl log-grow.pl $GROW |
		case $PROG in
		perl)
			/usr/bin/time -v -o time/$DESC /usr/bin/perl wwwstats
			;;
		sgsh)
			/usr/bin/time -v -o time/$DESC ../sgsh -p .. ../example/web-log-report.sh
			;;
		esac >out/$DESC 2>err/$DESC
	done
	GROW=`expr $GROW \* 2`
	if [ $GROW -gt 1024 ]
	then
		break
	fi
done
