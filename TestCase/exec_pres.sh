#!/bin/sh
###

function run_test_with()
{
    # server_ip_port -> $1
    # expected-writes -> $2
    # sample -> $3

	nohup ./pressure -server $1 -enable-concurrent -routines 20 -expected-writes=$2 -sample $3 > /dev/null &
}

function run_test_on()
{
    # server_ip_port -> $1

	run_test_with $1 1G S100k
	run_test_with $1 1G S1M
	run_test_with $1 1G S5M
	run_test_with $1 1G S10M
	run_test_with $1 1G S100M
}

#
#run_test_on 10.0.1.186:9092 &
#run_test_on 10.0.1.186:9093 &

# AWS Elastic Load Balancing
run_test_on 10.0.1.119:30000 &

#