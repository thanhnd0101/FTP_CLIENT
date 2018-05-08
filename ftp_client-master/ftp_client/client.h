#ifndef _CLIENT_H
#define _CLIENT_H

#include <stdio.h>
#include <stdlib.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <string.h>
#include <iostream>
#include <string>

#pragma comment(lib, "Ws2_32.lib")


#define FTP_PORT "21"
#define BUFLEN 256

/* _cmd store command such as CWD, LIST,... in server
   _args store arguments such as /home/task /home/bt
*/
struct command{
	char _cmd[6];
	char _args[BUFLEN];
	bool pasv_mode;
};


/* Login to socket with username and password
* return reply_code (code send from server)
*/
int ftp_login(SOCKET &connect_SOCKET);

/* Print full message from server
* by reply_code, follow RFC 959
*/
void print_reply_code(int reply_code);

/* Read client command and tranfer to server command
	return 0 if success, return -1 if client command is invalid
*/
int read_ftpclien_cmd(char* buff, command *cmdstruct);

SOCKET pasv(SOCKET &connect_SOCKET, char* host);

int ls(SOCKET &connect_SOCKET, char *_cmd);

void pwd(SOCKET &connect_SOCKET, char *pwd);

void TYPE_I(SOCKET &connect_SOCKET, char *typei);

int put(SOCKET &connect_SOCKET, char *_cmd, command* cmdstruct);

int get(SOCKET &connect_SOCKET, char *_cmd, command* cmdstruct);

int feedback(SOCKET &connect_SOCKET, char* feedback, int size);

int get226_Successfully_transferred(SOCKET &connect_SOCKET, char* pre_feedback);

#endif