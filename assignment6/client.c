/*
 * client.c
 */
#include <stdio.h>
#include <stdlib.h>
#include "csapp.h"
#include <regex.h>


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

    char * buffer1 = malloc(100);
    char * buffer2= malloc(100);
    sprintf(buffer1, "<param><value><double>%s</double></value></param>\r\n", num1);
    sprintf(buffer2, "<param><value><double>%s</double></value></param>\r\n", num2);
    char * body= malloc(400);
    strcat(body, "<?xml version=\"1.0\"?>\r\n");
    strcat(body, "<methodCall>\r\n");
    strcat(body, "<methodName>sample.addmultiply</methodName>\r\n");
    strcat(body, "<params>\r\n");
    strcat(body, buffer1);
    strcat(body, buffer2);
    strcat(body, "</params>\r\n");
    strcat(body, "</methodCall>\r\n");

    char * header=malloc(400);
    strcat(header, "POST /RPC2 HTTP/1.1\r\n");
    strcat(header, "Content-Type: text/xml\r\n");
    strcat(header, "User-Agent: XML-RPC.NET\r\n");
    char * buffer3 = malloc(100);
    sprintf(buffer3, "Content-Length: %lu\r\n", strlen(body));
    strcat(header, buffer3);
    strcat(header, "Expect: 100-continue\r\n");
    strcat(header, "Connection: Keep-Alive\r\n");
    strcat(header, "Host: localhost:8080\r\n\r\n");

    char * message=malloc(900);
    strcat(message, header);
    strcat(message, body);


    char * serverreply[500];
    clientfd = Open_clientfd(host, port);
    send(clientfd, message, strlen(message), 0);
    recv(clientfd, serverreply, 500, 0);
    printf(serverreply);
    strcat(serverreply, "\0");

    regex_t regex;
    int reti;
    regmatch_t rm[2];
    reti = regcomp(&regex, "<double>(.*)</double>",REG_EXTENDED);
    reti = regexec(&regex, serverreply, 2, rm, 0);
    printf("%i", reti);
    printf("Text: %.*s", (int)(rm[1].rm_eo - rm[1].rm_so), serverreply + rm[1].rm_so);
    //char msgbuf[100];
    //regerror(reti, &regex, msgbuf, sizeof(msgbuf));
    //fprintf(stderr, "Regex match failed: %s\n", msgbuf);
    /* 
	Write your code here.
	Recommend to use the Robust I/O package.
    */
    
    Close(clientfd);
    exit(0);
}
