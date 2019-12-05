/*
 * client.c
 */
#include <stdio.h>
#include <stdlib.h>
#include "csapp.h"

int main(int argc, char **argv) 
{
    int clientfd;
    char *num1, *num2;
    char *host, *port;

    if (argc != 3) {
        fprintf(stderr, "usage: %s <num1> <num2>\n", argv[0]);
        exit(0);
    }

    num1 = argv[1];
    num2 = argv[2];

    host = "localhost";
    port = "8080";

    clientfd = Open_clientfd(host, port);

    /* 
	Write your code here.
	Recommend to use the Robust I/O package.
    */
    
    Close(clientfd);
    exit(0);
}
