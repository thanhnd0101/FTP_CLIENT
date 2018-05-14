// TODO hoan thanh print_reply_code (con mot so code chua xong)
// TODO nhap password ma khong hien len terminal
#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "client.h"


//Global arguments for passive mode
SOCKET DataSocket=INVALID_SOCKET;
int ip1, ip2, ip3, ip4, p1, p2;


int main(int argc, char *argv[])
{
	// Kiem tra cu phap
	if (argc != 2) {
		printf("Using: ftp_client ftp.example.com\n");
		exit(1);
	}
	int status;
	char* host = argv[1];

	// Khoi tao Winsock phien ban 2.2
	WSADATA var_WSADATA;
	status = WSAStartup(MAKEWORD(2, 2), &var_WSADATA);
	if (status != 0) {
		printf("WSAStartup() failed with error: %d\n", status);
		WSACleanup();
		return 1;
	}

	// Tao addrinfo object
	addrinfo hints, *result, *p_addrinfo;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;


	// Resolve server address and port
	status = getaddrinfo(argv[1], FTP_PORT, &hints, &result);
	if (status != 0) {
		printf("getaddrinfo() failed with error %d\n", status);
		WSACleanup();
		return 1;
	}
	
	SOCKET connect_SOCKET = INVALID_SOCKET;
	// Loop through all the resuls and connect the first
	for (p_addrinfo = result; p_addrinfo != NULL;   p_addrinfo = p_addrinfo->ai_next) {
		connect_SOCKET =   socket(p_addrinfo->ai_family, p_addrinfo->ai_socktype,  p_addrinfo->ai_protocol);
		// invalid_socket -> use next p_addrinfo
		if (connect_SOCKET == INVALID_SOCKET) {
			continue;
		}
		status = connect(connect_SOCKET, p_addrinfo->ai_addr,
				 (int)p_addrinfo->ai_addrlen);
		// error_socket -> close then use next p_addrinfo
		if (status == SOCKET_ERROR) {
			closesocket(connect_SOCKET);
			continue;
		}
		break;
	}

	// done with result
	freeaddrinfo(result);
	

	// Login
	status = ftp_login(connect_SOCKET);
	//error
	if (status == -1){
		printf("Can not login!!");
		closesocket(connect_SOCKET);
		return 0;
	}
	pwd(connect_SOCKET, "PWD\r\n");
	TYPE_I(connect_SOCKET, "TYPE I\r\n");

	command *cmdstruct=new command;
	char *whole_cmd=new char[BUFLEN];
	

	while (1){
		if (read_ftpclien_cmd(whole_cmd, cmdstruct) == -1){
			printf("Ivalid Commmand");
			continue;
		}
		else
		{
			if ((strcmp(cmdstruct->_cmd, "NLST") == 0) || (strcmp(cmdstruct->_cmd, "LIST") == 0)){
				ls(connect_SOCKET, host,whole_cmd);
				continue;
			}
			else if (strcmp(cmdstruct->_cmd, "STOR") == 0){
				put(connect_SOCKET, host,whole_cmd, cmdstruct);
				continue;
				
			}
			else if (strcmp(cmdstruct->_cmd, "RETR") == 0){
				get(connect_SOCKET,host, whole_cmd, cmdstruct);
				continue;
			}
		}
	}
	// Cleanup
	closesocket(connect_SOCKET);
	WSACleanup();
	return 0;
}

int ftp_login(SOCKET &connect_SOCKET)
{
	char buf[BUFLEN]; // buffer receice from or send to ftp serve
	int len_temp;     // length of buffer receive from or send to ftp server
	int reply_code;   // reply code from ftp server
	int status;       // for get Winsock error
	// Check connection
	while ((len_temp = recv(connect_SOCKET, buf, BUFLEN, 0)) > 0) {
		char *p_end = NULL;
		reply_code = strtol(buf, &p_end, 10);
		print_reply_code(reply_code);

		// 220 Service ready for new user
		if (reply_code == 220) {
			break;
		}
		// error happen
		return reply_code;
	}
	memset(buf, 0, BUFLEN);

	// Get username
	// USER <SP> <username> <CRLF>
	printf("Username: ");
	char username[BUFLEN - 10];
	fgets(username, BUFLEN - 10, stdin);
	username[strcspn(username, "\n")] = 0; // remove trailing '\n'
	sprintf_s(buf, "USER %s\r\n", username);

	// Send username
	status = send(connect_SOCKET, buf, strlen(buf), 0);
	if (status == SOCKET_ERROR) {
		printf("send() error %d\n", WSAGetLastError());
	}
	memset(buf, 0, BUFLEN);

	// Check server recieving username
	while ((len_temp = recv(connect_SOCKET, buf, BUFLEN, 0)) > 0) {
		char *p_end = NULL;
		reply_code = strtol(buf, &p_end, 10);
		print_reply_code(reply_code);

		// 230 User logged in, proceed
		// 331 User name okay, need password
		if (reply_code == 230 || reply_code == 331) {
			break;
		}
		// error happen
		return -1;
	}
	memset(buf, 0, BUFLEN);

	// If logged in, exit
	if (reply_code == 230) {
		return 0;
	}

	// username looks good, need password
	// Get password
	// PASS <SP> <password> <CRLF>
	printf("Password: ");
	char password[BUFLEN - 10];
	fgets(password, BUFLEN - 10, stdin);
	password[strcspn(password, "\n")] = 0; // remove trailing '\n'
	sprintf_s(buf, "PASS %s\r\n", password);

	// Send password
	status = send(connect_SOCKET, buf, strlen(buf), 0);
	if (status == SOCKET_ERROR) {
		printf("send() error %d\n", WSAGetLastError());
	}
	memset(buf, 0, BUFLEN);

	// Check server recieving password
	while ((len_temp = recv(connect_SOCKET, buf, BUFLEN, 0)) > 0) {
		char *p_end = NULL;
		reply_code = strtol(buf, &p_end, 10);
		print_reply_code(reply_code);

		// 230 User logged in, proceed
		if (reply_code == 230) {
			break;
		}
		// error happen
		return -1;
	}
	memset(buf, 0, BUFLEN);

	return 0;
}

void print_reply_code(int reply_code)
{
	if (reply_code == 110)
		printf(
		    "%d Restart marker reply.\n"
		    "    In this case, the text is exact and not left to the\n"
		    "    particular implementation; it must read :\n"
		    "    \tMARK yyyy = mmmm\n"
		    "    Where yyyy is User - process data stream marker, and "
		    "mmmm\n"
		    "    server's equivalent marker (note the spaces between "
		    "markers\n"
		    "    and \"=\").\n",
		    reply_code);

	else if (reply_code == 120)
		printf("%d Service ready in nnn minutes.\n", reply_code);
	else if (reply_code == 125)
		printf("%d Data connection already open; transfer starting.\n",
		       reply_code);
	else if (reply_code == 150)
		printf("%d File status okay; about to open data connection.\n",
		       reply_code);
	else if (reply_code == 200)
		printf("%d Command okay.\n", reply_code);
	else if (reply_code == 202)
		printf(
		    "%d Command not implemented, superfluous at this site.\n",
		    reply_code);
	else if (reply_code == 211)
		printf("%d System status, or system help reply.\n", reply_code);
	else if (reply_code == 212)
		printf("%d Directory status.\n", reply_code);
	else if (reply_code == 213)
		printf("%d File status.\n", reply_code);
	else if (reply_code == 214)
		printf("%d Help message.\n"
		       "    On how to use the server or the meaning of a "
		       "particular\n"
		       "    non - standard command.This reply is useful only "
		       "to the\n"
		       "    human user.\n",
		       reply_code);
	else if (reply_code == 215)
		printf("%d NAME system type.\n"
		       "    Where NAME is an official system name from the "
		       "list in the\n"
		       "    Assigned Numbers document.\n",
		       reply_code);
	else if (reply_code == 220)
		printf("%d Service ready for new user.\n", reply_code);
	else if (reply_code == 221)
		;
	else if (reply_code == 225)
		;
	else if (reply_code == 226)
		;
	else if (reply_code == 227)
		;
	else if (reply_code == 230)
		printf("%d User logged in, proceed.\n", reply_code);
	else if (reply_code == 250)
		printf("%d Requested file action okay, completed.\n",reply_code);
	else if (reply_code == 257)
		printf("%d 'PATHNAME' created.",reply_code);
	else if (reply_code == 331)
		printf("%d User name okay, need password.\n", reply_code);
	else if (reply_code == 332)
		printf("%d Need account for login.\n", reply_code);
	else if (reply_code == 350)
		;
	else if (reply_code == 421)
		printf("%d Service not available, closing control connection.\n"
		       "    This may be a reply to any command if the service "
		       "knows it\n"
		       "    must shut down.\n",
		       reply_code);
	else if (reply_code == 425)
		;
	else if (reply_code == 426)
		;
	else if (reply_code == 450)
		;
	else if (reply_code == 451)
		;
	else if (reply_code == 451)
		;
	else if (reply_code == 500)
		printf("%d Syntax error, command unrecognized.\n"
		       "    This may include errors such as command line too "
		       "long.\n",
		       reply_code);
	else if (reply_code == 500){
		printf("%d Syntax error, command unrecognized.\nThis may include errors such as command line too long.", reply_code);
	}
	else if (reply_code == 501)
		printf("%d Syntax error in parameters or arguments.\n",
		       reply_code);
	else if (reply_code == 502){}
		
	else if (reply_code == 503)
	{ }
	else if (reply_code == 504)
	{ }
	else if (reply_code == 530)
		printf("%d Not logged in.\n", reply_code);
	else if (reply_code == 532)
	{ }
	else if (reply_code == 550)
	{ }
	else if (reply_code == 551)
	{ }
	else if (reply_code == 552)
	{ }
	else if (reply_code == 553)
	{ }
}

int read_ftpclien_cmd(char* buff,command *cmdstruct){
	memset(cmdstruct->_cmd, 0, sizeof(cmdstruct->_cmd));
	memset(cmdstruct->_args, 0, sizeof(cmdstruct->_args));

	printf("ftpclient> ");
	fflush(stdin);

	//read_ftpclient_input
	if (fgets(buff, BUFLEN, stdin)){
		char *enter = strchr(buff, '\n');
		*enter = '\0';
	}

	char *args = NULL;
	char *next_token;
	args = strtok_s(buff," ",&next_token);
	//get arguments
	args = strtok_s(NULL, "\0",&next_token);

	//store arguments
	if (args){
		strncpy_s(cmdstruct->_args, args, strlen(args));
	}

	//now buff = command
	if (strcmp(buff, "ls")==0){
		strcpy_s(cmdstruct->_cmd, "NLST");
	}
	else if (strcmp(buff, "dir")==0){
		strcpy_s(cmdstruct->_cmd, "LIST");
	}
	else if (strcmp(buff, "put")==0){
		strcpy_s(cmdstruct->_cmd, "STOR");
	}
	else if (strcmp(buff, "get")==0){
		strcpy_s(cmdstruct->_cmd, "RETR");
	}
	else if (strcmp(buff, "cd")==0){
		strcpy_s(cmdstruct->_cmd, "CWD");
	}
	else if (strcmp(buff, "delete")==0){
		strcpy_s(cmdstruct->_cmd, "DELE");
	}
	else if (strcmp(buff, "mkdir")==0){
		strcpy_s(cmdstruct->_cmd, "MKD");
	}
	else if (strcmp(buff, "rmdir")==0){
		strcpy_s(cmdstruct->_cmd, "RMD");
	}
	else if (strcmp(buff, "pwd")==0){
		strcpy_s(cmdstruct->_cmd, "PWD");
	}
	else if (strcmp(buff, "passive")==0){
		strcpy_s(cmdstruct->_cmd, "PASV");
	}
	else if (strcmp(buff, "quit")==0 || strcmp(buff, "exit")==0){
		strcpy_s(cmdstruct->_cmd, "QUIT");
	}
	else
	{
		return -1;
	}

	memset(buff, '\0', BUFLEN);
	strncpy(buff, cmdstruct->_cmd,sizeof(cmdstruct->_cmd));
	if (args != NULL){
		strcat(buff, " ");
		strncat(buff, cmdstruct->_args, sizeof(cmdstruct->_args));
	}
	strcat(buff, "\r\n\0");
	return 0;
}

int feedback(SOCKET &connect_SOCKET,char* feedback,int size){
	memset(feedback, '\0', size);

	int status = recv(connect_SOCKET, feedback, size, 0);


	if (status < 0){
		printf("recv() error %d\n", WSAGetLastError());
		return -1;
	}
	return 0;
}

SOCKET pasv(SOCKET &connect_SOCKET, char* host){
	char pasv[BUFLEN];
	memset(pasv, '\0', BUFLEN);
	sprintf_s(pasv, "PASV\r\n");

	int status = send(connect_SOCKET, pasv, sizeof(pasv), 0);
	if ( status==SOCKET_ERROR){
		printf("send() error %d\n", WSAGetLastError());
		return -1;
	}

	char temp[BUFLEN];
	memset(temp, '\0', BUFLEN);
	if (recv(connect_SOCKET, temp, BUFLEN, 0) < 0){
		printf("recv() error %d\n", WSAGetLastError());
		return -1;
	}
	else{
		fputs(temp, stdout);
	}

	//scan SERVER IP,PORT
	sscanf(temp, "227 Entering Passive Mode (%d,%d,%d,%d,%d,%d)", &ip1, &ip2, &ip3, &ip4, &p1, &p2);
	//set ip
	char ip[BUFLEN];
	memset(ip, '\0', BUFLEN);
	
	//set port
	int serverport = p1 * 256 + p2;
	
	DataSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (DataSocket < 0){
		printf("ERROR opening socket");
	}
	struct sockaddr_in servAddr2;
	memset(&servAddr2, '0', sizeof(servAddr2));

	servAddr2.sin_family = AF_INET;
	servAddr2.sin_port = htons(serverport);
	
	if (inet_pton(AF_INET, host, &servAddr2.sin_addr) <= 0)
	{
		printf("\n inet_pton error occured\n");
		exit(1);
	}
	if (connect(DataSocket, (struct sockaddr *)&servAddr2, sizeof(struct sockaddr)) < 0)
	{
		printf("\n[!]Fail connect to server \n");
		closesocket(DataSocket);
		return -1;
	}

	return DataSocket;
}

int ls(SOCKET &connect_SOCKET,char* host,char *_cmd){
	if (pasv(connect_SOCKET, host) != -1){
		char retrive[BUFLEN * 2];
		char f_ls_list[BUFLEN * 5];

		//send NLST
		int status = send(connect_SOCKET, _cmd, BUFLEN, 0);

		if (status < 0){
			printf("send() error %d\n", WSAGetLastError());
			return -1;
		}
		//get feedback
		char feedb[BUFLEN];
		status = feedback(connect_SOCKET, feedb, BUFLEN);
		if (status < 0){
			return -1;
		}
		else{
			fputs(feedb, stdout);
		}

		//get list
		memset(retrive, '\0', BUFLEN);
		memset(f_ls_list, '\0', sizeof(f_ls_list));
		status = recv(DataSocket, retrive, sizeof(retrive), 0);
		while (status > 0){
			strcat(f_ls_list, retrive);
			status = recv(DataSocket, retrive, sizeof(retrive), 0);
		}
		get226_Successfully_transferred(connect_SOCKET, feedb);
		fputs(f_ls_list, stdout);

		closesocket(DataSocket);
		return 0;
	}
}

void pwd(SOCKET &connect_SOCKET,char *pwd){
	char buff[BUFLEN];

	if (send(connect_SOCKET, pwd, strlen(pwd), 0) < 0){
		printf("send() error %d\n", WSAGetLastError());
		return;
	}
	memset(buff, '\0', BUFLEN);

	if (recv(connect_SOCKET, buff, BUFLEN, 0) < 0){
		printf("recv() error %d\n", WSAGetLastError());
		return;
	}
	else{
		int code;
		fputs(buff, stdout);
		sprintf(buff, "%d", &code);
		print_reply_code(code);
	}
}

void TYPE_I(SOCKET &connect_SOCKET, char *typei){
	char buff[BUFLEN];

	if (send(connect_SOCKET, typei, strlen(typei), 0) < 0){
		printf("send() error %d\n", WSAGetLastError());
		return;
	}
	memset(buff, '\0', BUFLEN);

	if (recv(connect_SOCKET, buff, BUFLEN, 0) < 0){
		printf("recv() error %d\n", WSAGetLastError());
		return;
	}
	else{
		int code;
		fputs(buff, stdout);
		sprintf(buff, "%d", &code);
		print_reply_code(code);
	}
}

int put(SOCKET &connect_SOCKET, char* host,char *_cmd,command* cmdstruct){
	if (pasv(connect_SOCKET, host) != -1){
		//Check whether file is valid?

		FILE* file_out = fopen(cmdstruct->_args, "r");
		if (file_out == NULL){
			printf("%s is invalid", cmdstruct->_args);
			return -1;
		}
		//send STOR command
		int status = send(connect_SOCKET, _cmd, BUFLEN, 0);

		if (status < 0){
			printf("send() error %d\n", WSAGetLastError());
			return -1;
		}
		//get feedback
		char feedb[BUFLEN];
		status = feedback(connect_SOCKET, feedb, BUFLEN);

		if (status < 0){
			return -1;
		}
		else{
			fputs(feedb, stdout);
		}
		//upload file
		int len;
		char buf[BUFLEN];
		memset(buf, '\0', BUFLEN);
		while ((len = fread(buf, 1, BUFLEN, file_out)) > 0){
			status = send(DataSocket, buf, len, 0);
			if (status < 0){
				printf("send() error %d\n", WSAGetLastError());
				return -1;
			}
		}
		fclose(file_out);
		
		closesocket(DataSocket);
		return 0;
	}
}

int get(SOCKET &connect_SOCKET, char* host,char *_cmd, command* cmdstruct){
	if (pasv(connect_SOCKET, host) != -1){
		//send RETR command
		int status = send(connect_SOCKET, _cmd, BUFLEN, 0);

		if (status < 0){
			printf("send() error %d\n", WSAGetLastError());
			return -1;
		}
		//get feedback
		char feedb[BUFLEN];

		status = feedback(connect_SOCKET, feedb, BUFLEN);

		if (status < 0){
			return -1;
		}
		else{
			int code;
			fputs(feedb, stdout);
			sscanf(feedb, "%d", &code);
			if (code == 550){
				return -1;
			}
		}

		//create a new file
		FILE* file_in = fopen(cmdstruct->_args, "w");
		if (file_in == NULL){
			printf("Can not get file %s", cmdstruct->_args);
			return -1;
		}
		//download file
		int len;
		char buf[BUFLEN];
		memset(buf, '\0', BUFLEN);

		while (len = recv(DataSocket, buf, BUFLEN, 0) > 0){
			fwrite(buf, 1, BUFLEN, file_in);
		}

		get226_Successfully_transferred(connect_SOCKET, feedb);
		fclose(file_in);

		closesocket(DataSocket);

		return 0;
	}
}

int get226_Successfully_transferred(SOCKET &connect_SOCKET, char* pre_feedback){
	int code_226=0;
	//if pre_feedback has 226 code
	char* temp=NULL;
	temp = strtok(pre_feedback, "\n");
	while (temp!=NULL)
	{
		sscanf(temp, "%d", &code_226);
		temp = strtok(NULL, "\n");
		if (code_226 == 226){
			return 226;
		}
	}
	//get 226 from server
	int status;
	char feedb[BUFLEN];
	status = feedback(connect_SOCKET, feedb, BUFLEN);
	while (code_226 != 226 && status != -1)
	{
		sscanf(feedb, "%d", &code_226);
		if (code_226 == 226){
			return 226;
		}
		status = feedback(connect_SOCKET, feedb, BUFLEN);
	}
	return -1;
}

int mget(SOCKET &connect_SOCKET, char* host, char *_cmd, command* cmdstruct){

}