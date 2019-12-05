The python xmlrpc library is described here: https://docs.python.org/2/library/xmlrpclib.html

You may need to install python.  If so, you can do so below on Linux

sudo apt-get update
sudo apt-get install python

To run the python server, first make sure you have python installed and that server.py has executable permissions. 
Then simply type ./server.py

To run the python client, first make sure you have python installed and that client.py has executable permissions.
Then simply type ./client.py <arg1> <arg2>, where arg1 and arg2 are numbers.  The output of client.py is then 
the addition of the two numbers and the multiplication of the two numbers separated by a space. An example is below. 

$ ./server.py 
Listening on port 8080...
127.0.0.1 - - [22/Nov/2019 11:51:39] "POST /RPC2 HTTP/1.1" 200 -

$ ./client.py 5.5 9.2
14.7   50.6

Make sure you start the server first, or the client will return an error.  The client is configured to connect to localhoost
on port 8080. 

To build client.c, type "make".  This will build the "client" executable.  
This executable should take the same inputs as client.py above, and produce the same output.  
However, this executable can only communicate over sockets. 



