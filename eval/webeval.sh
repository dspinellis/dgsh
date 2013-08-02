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

. 'eval-lib.sh'

download ftp://ita.ee.lbl.gov/traces/clarknet_access_log_Aug28.gz
download ftp://ita.ee.lbl.gov/traces/clarknet_access_log_Sep4.gz

# Compile Java code if needed
if [ ! -r WebStats.class -o WebStats.java -nt WebStats.java ]
then
	if ! javac WebStats.java
	then
		echo "Unable to compile WebStats.java" 1>&2
		exit 1
	fi
fi

# Set machine type
if ! [ -d /opt/aws ] ||
   ! IID=`curl -s http://169.254.169.254/2011-01-01/meta-data/instance-id` ||
   ! TYPE=`ec2-describe-instance-attribute $IID --instance-type | awk '{print $3}'`
then
	TYPE=`hostname`
fi

mkdir -p time out err

# Log grow factor
GROW=1
while :
do
	for PROG in sgsh perl java
	do
		DESC=web-$TYPE-$PROG-$GROW
		if [ -r err/$DESC ]
		then
			continue
		fi
		for i in clarknet*.gz
		do
			gzip -dc $i
		done |
		/usr/bin/perl log-grow.pl $GROW |
		case $PROG in
		perl)
			timerun $DESC /usr/bin/perl web-log-report.pl
			;;
		sgsh)
			timerun $DESC ../sgsh -p .. ../example/web-log-report.sh
			;;
		java)
			timerun $DESC java WebStats
			;;
		esac
	done
	GROW=`expr $GROW \* 2`
	if [ $GROW -gt 1024 ]
	then
		break
	fi
done
