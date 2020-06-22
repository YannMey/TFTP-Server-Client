# Projet_RSA_Kessler_Meyer

This is the RSA project of 2020: MyTFTP

To make it you can simply type: `make`

It will make both tftp client and server you can then execute both with those arguments:

`tftp_client <address> <port>` (address can be IPV4 or IPV6 and port between 1500 and 65000)

`tftp_server <port>` (address will be the passive one and port between 1500 and 65000)

The command to pass to tftp_client are GET/PUT <filename> (GET and PUT commands are not case sensitive)

To enable random dropping of ack you can simply turn on the RANDOM macro in the Makefile (line 25)
Example of test:

```shell script
cp mytftp_client test/client
cp mytftp_server test/server
# Open a new terminal and type 
cd test/server
./mytftp_server 1500
# Go back to the first terminal and type 
cd test/client
./mytftp_client 127.0.0.1 1500
# enter
get test_server
# you will get test_server from the test/server/ folder
# restart the client (it closes by default after one operation but the server stay alive)
./mytftp_client 127.0.0.1 1500
put test_client
# in test/server/ you will have the test_client file
```