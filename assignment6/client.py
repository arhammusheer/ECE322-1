#!/usr/bin/python
import xmlrpclib
import sys

proxy = xmlrpclib.ServerProxy("http://localhost:8080/RPC2")
return_list = proxy.sample.addmultiply(float(sys.argv[1]),float(sys.argv[2]))
print return_list[0], " ", return_list[1]

