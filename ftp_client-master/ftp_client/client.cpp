#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "client.h"
FTP_MODE MODE = passive;

int main(int argc, char *argv[])
{
	// Kiem tra cu phap
	if (argc != 2) {
		printf("Using: ftp_client ftp.example.com\n");
		exit(1);
	}
	int status;
	char host[INET_ADDRSTRLEN];

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
	for (p_addrinfo = result; p_addrinfo != NULL;
		p_addrinfo = p_addrinfo->ai_next) {
		connect_SOCKET =
			socket(p_addrinfo->ai_family, p_addrinfo->ai_socktype,
			p_addrinfo->ai_protocol);
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
		if (p_addrinfo->ai_family == AF_INET) {
			struct sockaddr_in *ipv4 =
				(struct sockaddr_in *)p_addrinfo->ai_addr;
			void *addr = &(ipv4->sin_addr);
			inet_ntop(p_addrinfo->ai_family, addr, host,
				INET_ADDRSTRLEN);
		}
		break;
	}

	// done with result
	freeaddrinfo(result);

	// Get socket info
	struct sockaddr_in client_addr;
	socklen_t addr_len = sizeof(client_addr);
	if (getsockname(connect_SOCKET, (struct sockaddr *)&client_addr,
		&addr_len) < 0) {
		printf("getsockname error\n");
	}
	// Get client IP
	char ip_client_str[INET_ADDRSTRLEN];
	if (inet_ntop(client_addr.sin_family, &(client_addr.sin_addr),
		ip_client_str, sizeof(ip_client_str)) == NULL) {
		printf("inet_ntop error\n");
	}

	// Login
	status = ftp_login(connect_SOCKET);
	// error
	if (status == FTP_FAIL) {
		closesocket(connect_SOCKET);
		return 0;
	}
	pwd(connect_SOCKET, "PWD\r\n");
	TYPE(connect_SOCKET, "TYPE I\r\n");

	command *cmdstruct = new command;
	// test passive mode
	char *whole_cmd = new char[BUFLEN];
	memset(whole_cmd, '\0', BUFLEN);

	while (1) {
		if (read_ftpclien_cmd(whole_cmd, cmdstruct) == FTP_FAIL) {
			printf("Invalid Commmand\n");
			continue;
		}
		else {
			if ((strcmp(cmdstruct->_cmd, "NLST") == 0) ||
				(strcmp(cmdstruct->_cmd, "LIST") == 0)) {
				ls(connect_SOCKET, host, whole_cmd, cmdstruct,
					ip_client_str);
				continue;
			}
			else if (strcmp(cmdstruct->_cmd, "STOR") == 0) {
				put(connect_SOCKET, host, whole_cmd, cmdstruct,
					ip_client_str);
				continue;
			}
			else if (strcmp(cmdstruct->_cmd, "RETR") == 0) {
				get(connect_SOCKET, host, whole_cmd, cmdstruct,
					ip_client_str);
				continue;
			}
			else if (strcmp(cmdstruct->_cmd, "MRETR") == 0) {
				mget(connect_SOCKET, host, whole_cmd, cmdstruct,
					ip_client_str);
				continue;
			}
			else if (strcmp(cmdstruct->_cmd, "MSTOR") == 0) {
				mput(connect_SOCKET, host, whole_cmd, cmdstruct,
					ip_client_str);
				continue;
			}
			else if (strcmp(cmdstruct->_cmd, "QUIT") == 0) {
				handle_ftp_exit(connect_SOCKET, whole_cmd);
				break;
			}
			else if (strcmp(cmdstruct->_cmd, "PASV") == 0) {
				MODE = passive;
				printf("Switched to passive mode\n");
				continue;
			}
			else if (strcmp(cmdstruct->_cmd, "ACTV") == 0) {
				MODE = active;
				printf("Switched to active mode\n");
				continue;
			}
			else if (strcmp(cmdstruct->_cmd, "MKD") == 0) {
				mkdir(connect_SOCKET, whole_cmd);
				continue;
			}
			else if (strcmp(cmdstruct->_cmd, "RMD") == 0) {
				rmdir(connect_SOCKET, whole_cmd);
				continue;
			}
			else if (strcmp(cmdstruct->_cmd, "PWD") == 0) {
				pwd(connect_SOCKET, whole_cmd);
				continue;
			}
			else if (strcmp(cmdstruct->_cmd, "DELE") == 0) {
				delete_file(connect_SOCKET, whole_cmd);
				continue;
			}
			else if (strcmp(cmdstruct->_cmd, "MDELE") == 0) {
				mdelete(connect_SOCKET, whole_cmd, cmdstruct);
				continue;
			}
			else if (strcmp(cmdstruct->_cmd, "CHDIR") == 0) {
				lcd(cmdstruct->_args);
				continue;
			}
			else if (strcmp(cmdstruct->_cmd, "CWD") == 0) {
				cd(connect_SOCKET, whole_cmd);
				continue;
			}
			else {
				break;
			}
		}
	}
	delete cmdstruct;
	delete[] whole_cmd;
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
		buf[len_temp] = '\0';
		printf("%s", buf);
		reply_code = strtol(buf, &p_end, 10);

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
	vector<int> arr_expect_user = { 230, 331 };
	status = recv_reply(connect_SOCKET, arr_expect_user);
	if (status == FTP_FAIL)
		return FTP_FAIL;

	// Get password
	// PASS <SP> <password> <CRLF>
	printf("Password: ");
	char password[BUFLEN - 10];

	// hide password input
	int temp_i = 0, temp_ch;
	while ((temp_ch = _getch()) != Enter) {
		if (temp_ch == BACKSPACE) {
			if (temp_i > 0) {
				--temp_i;
			}
		}
		else {
			password[temp_i++] = temp_ch;
		}
	}
	password[temp_i] = '\0';
	fflush(stdin);
	printf("\n");

	sprintf_s(buf, "PASS %s\r\n", password);

	// Send password
	status = send(connect_SOCKET, buf, strlen(buf), 0);
	if (status == SOCKET_ERROR) {
		printf("send() error %d\n", WSAGetLastError());
	}
	memset(buf, 0, BUFLEN);

	// Check server recieving password
	vector<int> arr_expect_pass = { 230 };
	status = recv_reply(connect_SOCKET, arr_expect_pass);

	return status;
}

int read_ftpclien_cmd(char *buff, command *cmdstruct)
{
	memset(cmdstruct->_cmd, 0, sizeof(cmdstruct->_cmd));
	memset(cmdstruct->_args, 0, sizeof(cmdstruct->_args));

	printf("ftpclient> ");
	fflush(stdin);

	// read_ftpclient_input
	if (fgets(buff, BUFLEN, stdin)) {
		char *enter = strchr(buff, '\n');
		*enter = '\0';
	}

	char *args = NULL;
	char *next_token;
	args = strtok_s(buff, " ", &next_token);
	// get arguments
	args = strtok_s(NULL, "\0", &next_token);

	// store arguments
	if (args) {
		strncpy_s(cmdstruct->_args, args, strlen(args));
	}

	// now buff = command
	if (strcmp(buff, "ls") == 0) {
		strcpy_s(cmdstruct->_cmd, "NLST");
	}
	else if (strcmp(buff, "dir") == 0) {
		strcpy_s(cmdstruct->_cmd, "LIST");
	}
	else if (strcmp(buff, "put") == 0) {
		strcpy_s(cmdstruct->_cmd, "STOR");
	}
	else if (strcmp(buff, "get") == 0) {
		strcpy_s(cmdstruct->_cmd, "RETR");
	}
	else if (strcmp(buff, "cd") == 0) {
		strcpy_s(cmdstruct->_cmd, "CWD");
	}
	else if (strcmp(buff, "delete") == 0) {
		strcpy_s(cmdstruct->_cmd, "DELE");
	}
	else if (strcmp(buff, "mdelete") == 0) {
		strcpy_s(cmdstruct->_cmd, "MDELE");
	}
	else if (strcmp(buff, "mkdir") == 0) {
		strcpy_s(cmdstruct->_cmd, "MKD");
	}
	else if (strcmp(buff, "rmdir") == 0) {
		strcpy_s(cmdstruct->_cmd, "RMD");
	}
	else if (strcmp(buff, "pwd") == 0) {
		strcpy_s(cmdstruct->_cmd, "PWD");
	}
	else if (strcmp(buff, "passive") == 0) {
		strcpy_s(cmdstruct->_cmd, "PASV");
	}
	else if (strcmp(buff, "active") == 0) {
		strcpy_s(cmdstruct->_cmd, "ACTV");
	}
	else if (strcmp(buff, "quit") == 0 || strcmp(buff, "exit") == 0) {
		strcpy_s(cmdstruct->_cmd, "QUIT");
	}
	else if (strcmp(buff, "mget") == 0) {
		strcpy_s(cmdstruct->_cmd, "MRETR");
	}
	else if (strcmp(buff, "mput") == 0) {
		strcpy_s(cmdstruct->_cmd, "MSTOR");
	}
	else if (strcmp(buff, "lcd") == 0) {
		strcpy_s(cmdstruct->_cmd, "CHDIR");
	}
	else {
		return FTP_FAIL;
	}

	memset(buff, '\0', BUFLEN);
	strncpy(buff, cmdstruct->_cmd, sizeof(cmdstruct->_cmd));
	if (args != NULL) {
		strcat(buff, " ");
		strncat(buff, cmdstruct->_args, sizeof(cmdstruct->_args));
	}
	strcat(buff, "\r\n\0");
	return FTP_WIN;
}

SOCKET pasv(SOCKET &connect_SOCKET, SOCKET &DataSocket, char *host)
{
	int ip1, ip2, ip3, ip4, p1, p2;

	char pasv[BUFLEN];
	memset(pasv, '\0', BUFLEN);
	sprintf_s(pasv, "PASV\r\n");

	int status = send(connect_SOCKET, pasv, strlen(pasv), 0);
	if (status == SOCKET_ERROR) {
		printf("send() error %d\n", WSAGetLastError());
		return FTP_FAIL;
	}

	char temp[BUFLEN];
	memset(temp, '\0', BUFLEN);
	if (recv(connect_SOCKET, temp, BUFLEN, 0) < 0) {
		printf("recv() error %d\n", WSAGetLastError());
		return FTP_FAIL;
	}
	else {
		fputs(temp, stdout);
	}

	// scan SERVER IP,PORT
	sscanf(temp, "227 Entering Passive Mode (%d,%d,%d,%d,%d,%d)", &ip1,
		&ip2, &ip3, &ip4, &p1, &p2);
	// set ip
	char ip[BUFLEN];
	memset(ip, '\0', BUFLEN);

	// set port
	int serverport = p1 * 256 + p2;
	string serverport_str = to_string(serverport);

	// connect to server by port given
	// Tao addrinfo object
	addrinfo hints, *result, *p_addrinfo;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	// Resolve server address and port
	status = getaddrinfo(host, serverport_str.c_str(), &hints, &result);
	if (status != 0) {
		printf("getaddrinfo() failed with error %d\n", status);
		return INVALID_SOCKET;
	}

	// Loop through all the resuls and connect the first
	DataSocket = INVALID_SOCKET;
	for (p_addrinfo = result; p_addrinfo != NULL;
		p_addrinfo = p_addrinfo->ai_next) {
		DataSocket =
			socket(p_addrinfo->ai_family, p_addrinfo->ai_socktype,
			p_addrinfo->ai_protocol);
		// invalid_socket -> use next p_addrinfo
		if (DataSocket == INVALID_SOCKET) {
			continue;
		}
		status = connect(DataSocket, p_addrinfo->ai_addr,
			(int)p_addrinfo->ai_addrlen);
		// error_socket -> close then use next p_addrinfo
		if (status == SOCKET_ERROR) {
			closesocket(DataSocket);
			continue;
		}
		break;
	}

	freeaddrinfo(result);
	return FTP_WIN;
}

int ls(SOCKET &connect_SOCKET, char *host, char *_cmd, command *cmdstruct,
	char *ip_client_str)
{
	SOCKET DataSocket = INVALID_SOCKET;
	SOCKET listen_Socket = INVALID_SOCKET;
	if (MODE == active) {
		listen_Socket = mode_active(connect_SOCKET, ip_client_str);
	}
	else {
		pasv(connect_SOCKET, DataSocket, host);
	}

	if (listen_Socket == INVALID_SOCKET && DataSocket == INVALID_SOCKET) {
		return FTP_FAIL;
	}

	char retrive[BUFLEN];

	// send NLST
	int status = send(connect_SOCKET, _cmd, strlen(_cmd), 0);

	if (status < 0) {
		printf("send() error %d\n", WSAGetLastError());
		closesocket(DataSocket);
		return FTP_FAIL;
	}
	// get feedback
	vector<int> arr_expect_pass = { 150 };
	status = recv_reply(connect_SOCKET, arr_expect_pass);
	if (status == FTP_FAIL) {
		closesocket(DataSocket);
		return FTP_FAIL;
	}

	if (MODE == active) {
		DataSocket = accept(listen_Socket, NULL, NULL);
		closesocket(listen_Socket);
	}

	// get list
	memset(retrive, '\0', BUFLEN);
	int len = 0;
	while ((len = recv(DataSocket, retrive, BUFLEN, 0)) > 0) {
		retrive[len] = '\0';
		fputs(retrive, stdout);
	}
	// get226_Successfully_transferred
	arr_expect_pass = { 226 };
	status = recv_reply(connect_SOCKET, arr_expect_pass);
	if (status == FTP_FAIL) {
		closesocket(DataSocket);
		return FTP_FAIL;
	}
	closesocket(DataSocket);

	return FTP_WIN;
}

int pwd(SOCKET &connect_SOCKET, char *pwd)
{
	char buff[BUFLEN];

	if (send(connect_SOCKET, pwd, strlen(pwd), 0) < 0) {
		printf("send() error %d\n", WSAGetLastError());
		return FTP_FAIL;
	}
	memset(buff, '\0', BUFLEN);

	if (recv(connect_SOCKET, buff, BUFLEN, 0) < 0) {
		printf("recv() error %d\n", WSAGetLastError());
		return FTP_FAIL;
	}
	else {
		fputs(buff, stdout);
	}
	return FTP_WIN;
}

void TYPE(SOCKET &connect_SOCKET, char *typei)
{
	char buff[BUFLEN];

	if (send(connect_SOCKET, typei, strlen(typei), 0) < 0) {
		printf("send() error %d\n", WSAGetLastError());
		return;
	}
	memset(buff, '\0', BUFLEN);

	if (recv(connect_SOCKET, buff, BUFLEN, 0) < 0) {
		printf("recv() error %d\n", WSAGetLastError());
		return;
	}
	else {
		fputs(buff, stdout);
	}
}

int put(SOCKET &connect_SOCKET, char *host, char *_cmd, command *cmdstruct,
	char *ip_client_str)
{
	SOCKET DataSocket;
	SOCKET listen_Socket = INVALID_SOCKET;
	if (MODE == active) {
		listen_Socket = mode_active(connect_SOCKET, ip_client_str);
	}
	else {
		pasv(connect_SOCKET, DataSocket, host);
	}

	if (listen_Socket == INVALID_SOCKET && DataSocket == INVALID_SOCKET) {
		return FTP_FAIL;
	}

	// Check whether file is valid?

	FILE *file_out = fopen(cmdstruct->_args, "rb");
	if (file_out == NULL) {
		printf("%s is invalid", cmdstruct->_args);
		closesocket(DataSocket);
		return FTP_FAIL;
	}
	// send STOR command
	int status = send(connect_SOCKET, _cmd, strlen(_cmd), 0);

	if (status < 0) {
		printf("send() error %d\n", WSAGetLastError());
		closesocket(DataSocket);
		return FTP_FAIL;
	}
	// get feedback
	vector<int> arr_expect_pass = { 150 };
	status = recv_reply(connect_SOCKET, arr_expect_pass);
	if (status == FTP_FAIL) {
		closesocket(DataSocket);
		return FTP_FAIL;
	}
	if (MODE == active) {
		DataSocket = accept(listen_Socket, NULL, NULL);
		closesocket(listen_Socket);
	}
	// upload file
	int len;
	char buf[BUFLEN];
	memset(buf, '\0', BUFLEN);

	while ((len = fread(buf, sizeof(char), BUFLEN, file_out)) > 0) {
		status = send(DataSocket, buf, len, 0);
		if (status < 0) {
			printf("send() error %d\n", WSAGetLastError());
			closesocket(DataSocket);
			return FTP_FAIL;
		}
	}

	fclose(file_out);
	closesocket(DataSocket);

	// get226_Successfully_transferred
	arr_expect_pass = { 226 };
	status = recv_reply(connect_SOCKET, arr_expect_pass);
	if (status == FTP_FAIL) {
		return FTP_FAIL;
	}

	return FTP_WIN;
}

int get(SOCKET &connect_SOCKET, char *host, char *_cmd, command *cmdstruct,
	char *ip_client_str)
{
	SOCKET DataSocket;
	SOCKET listen_Socket = INVALID_SOCKET;
	if (MODE == active) {
		listen_Socket = mode_active(connect_SOCKET, ip_client_str);
	}
	else {
		pasv(connect_SOCKET, DataSocket, host);
	}

	if (listen_Socket == INVALID_SOCKET && DataSocket == INVALID_SOCKET) {
		return FTP_FAIL;
	}

	// send RETR command
	int status = send(connect_SOCKET, _cmd, strlen(_cmd), 0);

	if (status < 0) {
		printf("send() error %d\n", WSAGetLastError());
		closesocket(DataSocket);
		return FTP_FAIL;
	}
	// get feedback
	vector<int> arr_expect_pass = { 150 };
	status = recv_reply(connect_SOCKET, arr_expect_pass);
	if (status == FTP_FAIL) {
		closesocket(DataSocket);
		return FTP_FAIL;
	}
	if (MODE == active) {
		DataSocket = accept(listen_Socket, NULL, NULL);
		closesocket(listen_Socket);
	}
	// create a new file
	FILE *file_in = fopen(cmdstruct->_args, "wb");
	if (file_in == NULL) {
		printf("Can not get file %s", cmdstruct->_args);
		closesocket(DataSocket);
		return FTP_FAIL;
	}
	// download file
	int len;
	char buf[BUFLEN];
	memset(buf, '\0', BUFLEN);

	while (len = recv(DataSocket, buf, BUFLEN, 0) > 0) {
		fwrite(buf, sizeof(char), sizeof(buf), file_in);
	}

	// get226_Successfully_transferred
	arr_expect_pass = { 226 };
	status = recv_reply(connect_SOCKET, arr_expect_pass);
	if (status == FTP_FAIL) {
		closesocket(DataSocket);
		return FTP_FAIL;
	}

	fclose(file_in);
	closesocket(DataSocket);

	return FTP_WIN;
}

int mget(SOCKET &connect_SOCKET, char *host, char *_cmd, command *cmdstruct,
	char *ip_client_str)
{
	strcpy_s(cmdstruct->_cmd, "RETR");

	int count = 0;

	char _cmd1[BUFLEN];
	char *arg1 = NULL;
	command *cmdstruct1 = new command;
	strcpy(cmdstruct1->_cmd, cmdstruct->_cmd);

	char *next_token;
	// get argument 1
	arg1 = strtok_s(cmdstruct->_args, " ", &next_token);
	++count;
	strcpy(cmdstruct1->_args, arg1);

	if (next_token == NULL) {
		*cmdstruct->_args = *arg1;
		return get(connect_SOCKET, host, _cmd, cmdstruct,
			ip_client_str);
	}

	memset(_cmd1, '\0', BUFLEN);

	strcat(_cmd1, cmdstruct1->_cmd);
	strcat(_cmd1, " ");
	strcat(_cmd1, arg1);
	strcat(_cmd1, "\r\n\0");

	while (count > 0) {
		if (get(connect_SOCKET, host, _cmd1, cmdstruct1,
			ip_client_str) == FTP_FAIL) {
			return FTP_FAIL;
		}
		--count;
		// If still have files need to be downloaded
		if (*next_token != '\0') {
			arg1 = strtok_s(NULL, " ", &next_token);
			++count;
			memset(cmdstruct1->_args, '\0',
				sizeof(cmdstruct1->_args));
			strcpy(cmdstruct1->_args, arg1);
			memset(_cmd1, '\0', BUFLEN);
			strcat(_cmd1, cmdstruct1->_cmd);
			strcat(_cmd1, " ");
			strcat(_cmd1, arg1);
			strcat(_cmd1, "\r\n\0");
		}
	}

	delete cmdstruct1;
	return FTP_WIN;
}

int mput(SOCKET &connect_SOCKET, char *host, char *_cmd, command *cmdstruct,
	char *ip_client_str)
{
	strcpy_s(cmdstruct->_cmd, "STOR");

	int count = 0;

	char _cmd1[BUFLEN];
	char *arg1 = NULL;
	command *cmdstruct1 = new command;

	strcpy(cmdstruct1->_cmd, cmdstruct->_cmd);

	char *next_token;
	// get argument 1
	arg1 = strtok_s(cmdstruct->_args, " ", &next_token);
	++count;
	strcpy(cmdstruct1->_args, arg1);

	if (next_token == NULL) {
		*cmdstruct->_args = *arg1;
		return get(connect_SOCKET, host, _cmd, cmdstruct,
			ip_client_str);
	}

	memset(_cmd1, '\0', BUFLEN);

	strcat(_cmd1, cmdstruct1->_cmd);
	strcat(_cmd1, " ");
	strcat(_cmd1, arg1);
	strcat(_cmd1, "\r\n\0");

	while (count > 0) {
		if (put(connect_SOCKET, host, _cmd1, cmdstruct1,
			ip_client_str) == FTP_FAIL) {
			return FTP_FAIL;
		}
		--count;
		// If still have files need to be downloaded
		if (*next_token != '\0') {
			arg1 = strtok_s(NULL, " ", &next_token);
			++count;
			memset(cmdstruct1->_args, '\0',
				sizeof(cmdstruct1->_args));
			strcpy(cmdstruct1->_args, arg1);
			memset(_cmd1, '\0', BUFLEN);
			strcat(_cmd1, cmdstruct1->_cmd);
			strcat(_cmd1, " ");
			strcat(_cmd1, arg1);
			strcat(_cmd1, "\r\n\0");
		}
	}

	delete cmdstruct1;
	return FTP_WIN;
}

int handle_ftp_exit(SOCKET connect_SOCKET, char *quit_cmd)
{
	char buf[BUFLEN];
	int status;

	// QUIT <CRLF>
	status = send(connect_SOCKET, quit_cmd, strlen(quit_cmd), 0);

	if (status == SOCKET_ERROR) {
		printf("send() error %d\n", WSAGetLastError());
		return FTP_FAIL;
	}

	memset(buf, '\0', BUFLEN);

	// get feedback
	vector<int> arr_expect_pass = { 221 };
	status = recv_reply(connect_SOCKET, arr_expect_pass);

	return status;
}

int mkdir(SOCKET &connect_SOCKET, char *_cmd)
{
	if (send(connect_SOCKET, _cmd, strlen(_cmd), 0) < 0) {
		printf("send() error %d\n", WSAGetLastError());
		return FTP_FAIL;
	}

	vector<int> arr_expect_pass = { 257 };
	// status may be equal FTP_FAIL or FTP_WIN
	int status = recv_reply(connect_SOCKET, arr_expect_pass);
	return status;
}

int rmdir(SOCKET &connect_SOCKET, char *_cmd)
{
	if (send(connect_SOCKET, _cmd, strlen(_cmd), 0) < 0) {
		printf("send() error %d\n", WSAGetLastError());
		return FTP_FAIL;
	}

	vector<int> arr_expect_pass = { 250 };
	// status may be equal FTP_FAIL or FTP_WIN
	int status = recv_reply(connect_SOCKET, arr_expect_pass);
	return status;
}

int exists_in_arr(vector<int> arr, int k)
{
	for (size_t i = 0; i < arr.size(); ++i) {
		if (arr[i] == k)
			return FTP_WIN;
	}
	return FTP_FAIL;
}

int recv_reply(SOCKET connect_SOCKET, vector<int> arr_expect)
{
	char buf[BUFLEN];
	int len_temp;
	if ((len_temp = recv(connect_SOCKET, buf, BUFLEN, 0)) > 0) {
		char *p_end = NULL;
		buf[len_temp] = '\0';
		printf("%s", buf);
		int reply_code = strtol(buf, &p_end, 10);

		if (exists_in_arr(arr_expect, reply_code) == FTP_FAIL) {
			return FTP_FAIL;
		}
	}
	return FTP_WIN;
}

int delete_file(SOCKET &connect_SOCKET, char *_cmd)
{
	if (send(connect_SOCKET, _cmd, strlen(_cmd), 0) <
		0) { // Lệnh gửi chuỗi dele
		printf(
			"error: cannot send"); // Xuất ra lỗi không gửi được yêu cầu
		return FTP_FAIL;
	}

	vector<int> arr_expect_pass = { 250 };
	// status may be equal FTP_FAIL or FTP_WIN
	int status = recv_reply(connect_SOCKET, arr_expect_pass);
	return status;
}

int mdelete(SOCKET &connect_SOCKET, char *_cmd, command *cmdstruct)
{
	strcpy_s(cmdstruct->_cmd, "DELE");

	int count = 0;

	char _cmd1[BUFLEN];
	char *arg1 = NULL;
	command *cmdstruct1 = new command;

	strcpy(cmdstruct1->_cmd, cmdstruct->_cmd);

	char *next_token;
	// get argument 1
	arg1 = strtok_s(cmdstruct->_args, " ", &next_token);
	++count;
	strcpy(cmdstruct1->_args, arg1);

	if (next_token == NULL) {
		*cmdstruct->_args = *arg1;
		return delete_file(connect_SOCKET, _cmd);
	}

	memset(_cmd1, '\0', BUFLEN);

	strcat(_cmd1, cmdstruct1->_cmd);
	strcat(_cmd1, " ");
	strcat(_cmd1, arg1);
	strcat(_cmd1, "\r\n\0");

	while (count > 0) {
		if (delete_file(connect_SOCKET, _cmd1) == FTP_FAIL) {
			return FTP_FAIL;
		}
		--count;
		// If still have files need to be downloaded
		if (*next_token != '\0') {
			arg1 = strtok_s(NULL, " ", &next_token);
			++count;
			memset(cmdstruct1->_args, '\0',
				sizeof(cmdstruct1->_args));
			strcpy(cmdstruct1->_args, arg1);
			memset(_cmd1, '\0', BUFLEN);
			strcat(_cmd1, cmdstruct1->_cmd);
			strcat(_cmd1, " ");
			strcat(_cmd1, arg1);
			strcat(_cmd1, "\r\n\0");
		}
	}

	delete cmdstruct1;
	return FTP_WIN;
}

void lcd(char *arg)
{
	char buf[BUFLEN];

	if (_chdir(arg) == -1) {
		// change cwd to path (thay đổi thư mục hiện thời sang path)
		fprintf(stderr, "error: could not change to dir %s\n",
			arg); // Xuất lỗi: Không thể thay đổi
		return;
	}

	_getcwd(buf, BUFLEN); // Lấy tên thư mục chủ
	printf("Local current directory: %s\n",
		buf); // print cwd as obtained from getcwd()
}

int cd(SOCKET &connect_SOCKET, char *_cmd)
{
	int status;

	status = send(connect_SOCKET, _cmd, strlen(_cmd), 0); // Gửi yêu cầu

	vector<int> arr_expect_pass = { 250 };
	// status may be equal FTP_FAIL or FTP_WIN
	status = recv_reply(connect_SOCKET, arr_expect_pass);
	return status;
}

SOCKET mode_active(SOCKET connect_SOCKET, char *ip_client_str)
{
	// random port (code from stackoverflow)
	random_device rd;  // obtain a random number from hardware
	mt19937 eng(rd()); // seed the generator
	uniform_int_distribution<> distr(50000, 60000); // define the range
	int ACT_PORT = distr(eng);
	string port_str = to_string(ACT_PORT);

	// Tao listen port
	// Tao addrinfo object
	addrinfo hints, *result, *p_addrinfo;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_flags = AI_PASSIVE;

	// Resolve server address and port
	int status =
		getaddrinfo(ip_client_str, port_str.c_str(), &hints, &result);
	if (status != 0) {
		printf("getaddrinfo() failed with error %d\n", status);
		return INVALID_SOCKET;
	}

	// Loop through all the resuls
	char ip_str[INET_ADDRSTRLEN];
	SOCKET listen_SOCKET = INVALID_SOCKET;
	for (p_addrinfo = result; p_addrinfo != NULL;
		p_addrinfo = p_addrinfo->ai_next) {
		listen_SOCKET =
			socket(p_addrinfo->ai_family, p_addrinfo->ai_socktype,
			p_addrinfo->ai_protocol);
		if (listen_SOCKET == INVALID_SOCKET) {
			continue;
		}
		status = bind(listen_SOCKET, p_addrinfo->ai_addr,
			(int)p_addrinfo->ai_addrlen);
		if (status == SOCKET_ERROR) {
			closesocket(listen_SOCKET);
			continue;
		}
		if (p_addrinfo->ai_family == AF_INET) {
			struct sockaddr_in *ipv4 =
				(struct sockaddr_in *)p_addrinfo->ai_addr;
			void *addr = &(ipv4->sin_addr);
			inet_ntop(p_addrinfo->ai_family, addr, ip_str,
				INET_ADDRSTRLEN);
		}
		break;
	}
	freeaddrinfo(result);

	// Listen on a socket
	status = listen(listen_SOCKET, SOMAXCONN);
	if (status == SOCKET_ERROR) {
		printf("listen() failed with error: %d\n", WSAGetLastError());
		closesocket(listen_SOCKET);
		return INVALID_SOCKET;
	}

	// PORT <SP> <host - port> <CRLF>
	// <host - port> :: = <host - number>, <port - number>
	// <host - number> :: = <number>, <number>, <number>, <number>
	// <port - number> :: = <number>, <number>
	// <number> :: = any decimal integer 1 through 255
	// 127.0.0.1 -> 127,0,0,1
	for (size_t i = 0; i < strlen(ip_str); ++i) {
		if (ip_str[i] == '.')
			ip_str[i] = ',';
	}
	int port_num_1 = ACT_PORT / 256;
	int port_num_2 = ACT_PORT % 256;
	string msg = "PORT " + string(ip_str) + string(",") +
		to_string(port_num_1) + string(",") +
		to_string(port_num_2) + string("\r\n");
	char buf[BUFLEN];
	strcpy_s(buf, msg.c_str());

	// send PORT to server
	status = send(connect_SOCKET, buf, strlen(buf), 0);
	if (status == SOCKET_ERROR) {
		printf("send() error %d\n", WSAGetLastError());
		closesocket(listen_SOCKET);
		return INVALID_SOCKET;
	}

	// Get reply from server
	vector<int> arr_expect = { 200 };
	if (recv_reply(connect_SOCKET, arr_expect) == FTP_FAIL)
		return INVALID_SOCKET;

	return listen_SOCKET;
}