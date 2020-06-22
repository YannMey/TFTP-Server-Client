#!/bin/sh


cp ../mytftp_client client
cp ../mytftp_server server
cd server || exit
./mytftp_server 6200 & > /dev/null 2>&1
sleep 1
echo "SERVER NOW RUNNING IN BACKGROUND"
echo "================================"
cd ../client || exit
echo "TESTING GET test_server"
echo "get test_server">&1 | ./mytftp_client 127.0.0.1 6200
sleep 2
echo "================================"
echo "TESTING PUT test_client"
echo "put test_client">&1 | ./mytftp_client 127.0.0.1 6200
sleep 2
echo "================================"
echo "Killing server"
pkill -f mytftp_server