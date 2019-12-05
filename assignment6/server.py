#!/usr/bin/python
import xmlrpclib
from SimpleXMLRPCServer import SimpleXMLRPCServer

# Input arguments: two floats var1 and var2
# Output: a single dimensional array with two elements
#         the first element is a float representing the addition of the two input arguments
#         the second element is a float representing the multiplication of the two input arguments
def sample_addmultiply(var1,var2):
	return_list = [(var1+var2),(var1*var2)];	
	return return_list

server = SimpleXMLRPCServer(("localhost", 8080))
print "Listening on port 8080..."
server.register_function(sample_addmultiply, "sample.addmultiply")
server.serve_forever()
