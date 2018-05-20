#ifndef _CLIENT_H
#define _CLIENT_H

#include <conio.h>
#include <vector>
#include <stdio.h>
#include <stdlib.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <string.h>
#include <iostream>
#include <direct.h>
#include <string>
#include <random>

#pragma comment(lib, "Ws2_32.lib")
using namespace std;

#define FTP_PORT "21"
#define BUFLEN 1024
#define FTP_WIN 0
#define FTP_FAIL -1
#define Enter 13
#define BACKSPACE 8
enum FTP_MODE{ active, passive };

/* _cmd store command such as CWD, LIST,... in server
   _args store arguments such as /home/task /home/bt
   */
struct command{
	char _cmd[6];
	char _args[BUFLEN];
};

/* Login to socket with username and password
* return reply_code (code send from server)
*/
int ftp_login(SOCKET &connect_SOCKET);

/* Read client command and tranfer to server command.
	return FTP_WIN if success, return FTP_FAIL if client command is invalid.
	*/
int read_ftpclien_cmd(char* buff, command *cmdstruct);

SOCKET pasv(SOCKET &connect_SOCKET, SOCKET &DataSocket, char* host);

/*
	This command causes a directory listing to be sent from server to user site.
	return FTP_WIN if success, return FTP_FAIL if any error.
	*/
int ls(SOCKET &connect_SOCKET, char* host, char *_cmd, command* cmdstruct);

/*
	This command causes the name of the current working directory to be returned in the reply.
	return FTP_WIN if success, return FTP_FAIL if any error.
	*/
int pwd(SOCKET &connect_SOCKET, char *pwd);

/*
	This command tells the server that the transfer is going to involve a file with
	a binary data type.
	Image mode and Binary mode mean the same thing in FTP.
	*/
void TYPE(SOCKET &connect_SOCKET, char *typei);

/*
	This command causes the server-DTP to accept the data transferred via the data connection
	and to store the data as a file at the server site.
	return FTP_WIN if success, return FTP_FAIL if any error.
	*/
int put(SOCKET &connect_SOCKET, char* host, char *_cmd, command* cmdstruct);

/*
	This command causes the server-[Data Tranfer Process] to transfer a copy of the file, specified in the pathname,
	to user-DTP at the other end of the data connection.
	return FTP_WIN if success, return FTP_FAIL if any error.
	*/
int get(SOCKET &connect_SOCKET, char* host, char *_cmd, command* cmdstruct);

/*
	This command causes the server-[Data Tranfer Process] to transfer multi copies of the files,
	to user-DTP at the other end of the data connection.
	return FTP_WIN if success, return FTP_FAIL if any error.
	*/
int mget(SOCKET &connect_SOCKET, char* host, char *_cmd, command* cmdstruct);

/*
	This command causes the server-DTP to accept the data transferred via the data connection
	and to store the data as multi files at the server site.
	return FTP_WIN if success, return FTP_FAIL if any error.
	*/
int mput(SOCKET &connect_SOCKET, char* host, char *_cmd, command* cmdstruct);

/*
	This command causes the client-DTP to logout of server.
	return FTP_WIN if success, return FTP_FAIL if any error.
	*/
int handle_ftp_exit(SOCKET connect_SOCKET, char * quit_cmd);

/*
	This command causes the directory specified in the pathname to be created as a .
	return FTP_WIN if success, return FTP_FAIL if any error.
	*/
int mkdir(SOCKET &connect_SOCKET, char *_cmd);

/*
	This command causes the directory specified in the pathname to be removed as a directory.
	return FTP_WIN if success, return FTP_FAIL if any error.
	*/
int rmdir(SOCKET &connect_SOCKET, char *_cmd);

/*
	Check if k in arr
	return FTP_WIN if success, return FTP_FAIL if any error.
	*/
int exists_in_arr(vector<int> arr, int k);

/*
	Receive reply from server
	return FTP_WIN if success, return FTP_FAIL if any error.
	*/
int recv_reply(SOCKET connect_SOCKET, vector<int> arr_expect);

int delete_file(SOCKET &connect_SOCKET, char *_cmd);

int mdelete(SOCKET &connect_SOCKET, char *_cmd, command* cmdstruct);

void lcd(char * arg);

int cd(SOCKET &connect_SOCKET, char *_cmd);

SOCKET mode_active(SOCKET connect_SOCKET);
#endif