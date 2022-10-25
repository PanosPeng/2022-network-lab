//支持ipv6协议的Web Server
//学校的DNS不支持ipv6访问，连上运营商的ipv6即可，支持ipv6访问,ipv6地址：cmd->ipconfig/all->临时 IPv6 地址
#pragma once
#include "winsock2.h"
#include <stdio.h>
#include <iostream>
#include<sstream>        //istringstream 必须包含这个头文件
#include<string>  
#include <regex>
#include <fstream>
#include <ws2tcpip.h>//sockaddr_in6，支持ipv6的struct头文件
#include <thread>//多线程头文件

using namespace std;
#pragma comment(lib,"ws2_32.lib")


//Server IP:10.21.192.80(ipconfig/all)
string fileaddress = "your server path";
u_short listen_port = 80;//default 80
u_long listen_addr = INADDR_ANY;//default all address

void Session_process(SOCKET sessionSocket);
void printIpv6(sockaddr_in6 clientaddr);

int main()
{
	//configure listen IP/port and home road
	char configure = 'n';
	cout << "Do you want to configure IP?:(y/n)" << endl;
	cin >> configure;
	if (configure == 'y' || configure == 'Y')
	{
		cout << "Listen IP: ";
		cin >> listen_addr;
		cout << "Listen Port: ";
		cin >> listen_port;
		cout << "Home road: ";
		cin >> fileaddress;
		cout << "configure successfully!\n";
	}


	//1.initialize Winsock
	WSADATA wsaData;
	int nRc = WSAStartup(0x0202, &wsaData);	//完成一系列的初始化工作
	if (nRc) {
		printf("Winsock  startup failed with error!\n");
	}

	//wrong version
	if (wsaData.wVersion != 0x0202) {
		printf("Winsock version is not correct!\n");
	}
	//initialize successfully
	printf("Winsock  startup Ok!\n");

	//2.create listen socket:socket srv_listenSocket
	SOCKET srv_listenSocket;
	srv_listenSocket = socket(AF_INET6, SOCK_STREAM, 0);//指定使用SOCK_STREAM（流类型套接字——TCP protocol）0表示根据地址格式和SOCKET类型自动选择这里为IPPROTO_TCP
	if (srv_listenSocket != INVALID_SOCKET)
		printf("Socket create Ok!\n");
	int a = WSAGetLastError();

	//3.binding
	sockaddr_in6 addr, clientaddr;
	memset(&addr, 0, sizeof(sockaddr_in6)); //重点，否则就10049错误
	memset(&clientaddr, 0, sizeof(sockaddr_in6)); //重点，否则就10049错误

	int addrLen = sizeof(clientaddr);
	//set port and ip 绑定socket IP 和端口号80
	addr.sin6_family = AF_INET6;
	addr.sin6_port = htons(listen_port);//htons 和 htonl 函数把主机字节顺序转换为网络字节顺序，分别用于短整型16 和长整型数据32 reasons:网络是大端存储， pc是小端存储，需要转换
	addr.sin6_addr = IN6ADDR_ANY_INIT;//listen all TCP requests

	int rtn = bind(srv_listenSocket, (LPSOCKADDR)&addr, sizeof(addr));
	a = WSAGetLastError();
	if (rtn != SOCKET_ERROR)
		printf("Socket bind Ok!\n");
	
	//4.listen
	rtn = listen(srv_listenSocket, SOMAXCONN);//backlog=SOMAXCONN 等待连接的请求队列长度
	if (rtn != SOCKET_ERROR)
		printf("Socket listen Ok!\n\n");


	//5.accept and create a session socket

	while (1)
	{
		//产生会话Socket
		int sockerrornum;
		SOCKET sessionSocket = accept(srv_listenSocket, (sockaddr*)&clientaddr, &addrLen);
		//如果执行成功，accept()建立一个新的SOCKET与对方通信，与srv_listenSOCKET
		//有相同的特性，包括端口号，新生成的SOCKET-sessionSocket才是与client通信的实际SOCKET，前者为 监听SOCKET 后者为 会话SOCKET
		if (sessionSocket == INVALID_SOCKET) {
			printf("accept error %d\n", WSAGetLastError());
			return 0;
		}
		else
		{
			printf("Socket accept one client request!\n");
			thread talkthread(Session_process, sessionSocket);//开启一个新的线程来处理这一个client的请求
			printIpv6(clientaddr);
			cout << "client Port:" << clientaddr.sin6_port << "\n";
			//Session_process(sessionSocket);//session processing
			//print client IP b1.b2.b3.b4
			talkthread.detach();
		}
	}
	return 0;
}



//print ipv6 address of client XXXX.XXXX.XXXX.XXXX.XXXX.XXXX.XXXX.XXXX 4*32=128bits
void printIpv6(sockaddr_in6 clientaddr)
{
	printf("client IP: ");
	for (int i = 0; i < 8; ++i)
	{
		unsigned char b1 = (clientaddr.sin6_addr.u.Byte[2 * i] & 0xf0) >> 4;//高位
		unsigned char b2 = (clientaddr.sin6_addr.u.Byte[2 * i] & 0x0f);//低位
		unsigned char b3 = (clientaddr.sin6_addr.u.Byte[2 * i + 1] & 0xf0) >> 4;//高位
		unsigned char b4 = (clientaddr.sin6_addr.u.Byte[2 * i + 1] & 0x0f);//低位
		printf("%x%x%x%x", (unsigned int)b1, (unsigned int)b2, (unsigned int)b3, (unsigned int)b4);
		if (i <= 6)
			printf(".");
	}
	printf("\n");


}


//Session_process function
//receive HTTP message from transport layer and send the required file to client(browser)
void Session_process(SOCKET sessionSocket)
{
	//error type
	const char* NOTFOUND = "HTTP/1.1 404 Not Found\r\n";
	const char* BADREQUEST = "HTTP/1.1 400 Bad Request\r\n";

	char recvBuf[4096];
	//receiving data from client
	memset(recvBuf, '\0', 4096);
	int rtn = recv(sessionSocket, recvBuf, 2048, 0);//接收请求报文至recvBuf,最大2048bits
	if (rtn == SOCKET_ERROR) {
		std::cout << "receive falied!" << endl;
		return;
	}
	//receive successfully
	else {
		//解析请求报文请求行,并判断是否有error出现
		int sendre;//record return from send()
		string method, URL, http_version;//请求行三个word分别为：方法 URL 版本
		std::string str(recvBuf);//取出recvBuf第一行
		istringstream is(str);//输入输出控制类
		string s;
		//separate method URL http_version
		is >> s;
		method = s;
		is >> s;
		URL = s;
		is >> s;
		http_version = s;

		cout << "\nmethod: " << method << "\nURL: " << URL << "\nversion: " << http_version << "\n";
		//判断请求行各个对象是否合法
		//1.method check
		if (method != (string)"GET" && method != (string)"POST" && method != (string)"HEAD" && method != (string)"PUT" && method != (string)"DELETE")
		{	//bad request
			printf("method ERROR\n");
			sendre = send(sessionSocket, BADREQUEST, strlen(BADREQUEST), 0);//send 400 error
			closesocket(sessionSocket);
			return;
		}

		//2.URL check(file system)

		//if (URL == (string)"/favicon.ico")//直接返回
		//{
		//	//cout << " favicon.ico detected " << endl;
		//	closesocket(sessionSocket);
		//	return;
		//}

		//3.http_version check正则判定
		string http_v_reg = "HTTP/[0-9][.][0-9]";
		regex rule(http_v_reg);
		bool judge = regex_match(http_version, rule);//0-mismatch 1-match
		if (!judge)
		{	//bad request
			printf("http version ERROR\n");
			sendre = send(sessionSocket, BADREQUEST, strlen(BADREQUEST), 0);//send 400 error
			closesocket(sessionSocket);
			return;
		}

		//请求报文正确，无400 error
		else {
			ifstream f;
			f.open(fileaddress + URL, std::ios::binary);//
			//404 not found send 404.html
			if (!f) {//这里浏览器会多发送一次/favicon.ico请求，遇到该请求直接结束会话即可
				f.close();
				cout << URL + "  NOT FOUND 404\n";
				URL = (string)"/404.html";//修改URL为404.html
				f.open(fileaddress + URL, std::ios::binary);
			}

			//请求报文正确且找到对应的html文件(include 404.html)发送响应报文
			filebuf* tmp = f.rdbuf();
			int size = tmp->pubseekoff(0, f.end, f.in);
			tmp->pubseekpos(0, f.in);
			if (size <= 0) {
				std::cout << "load file into memory failed!\n";
				sendre = send(sessionSocket, BADREQUEST, strlen(BADREQUEST), 0);
				closesocket(sessionSocket);
				return;
			}

			else {
				string Content_Type = "image/jpg";//也可以输出文本格式text/html
				char* buffer = new char[size];//数据头指针
				char* tail = buffer + size;//数据尾指针
				tmp->sgetn(buffer, size);//
				f.close();
				cout << "successfully response file: " + URL <<"\n"<< endl;
				stringstream remsg;
				remsg << "HTTP/1.1 200 OK\r\n" << "Connection: close\r\n" << "Server: Macyrate\r\n" << "Content Length: " << size
					<< "\r\n" << "Content Type: " + Content_Type << "\r\n\r\n";
				string remsgstr = remsg.str();//转化为string
				const char* remsgchar = remsgstr.c_str();//转化为char*数组
				int final_size = strlen(remsgchar);
				sendre = send(sessionSocket, remsgchar, final_size, 0);//先发送
				while (buffer < tail) {
					sendre = send(sessionSocket, buffer, size, 0);
					buffer = buffer + sendre;
					size = size - sendre;
				}
				closesocket(sessionSocket);
				return;
			}



		}




	}


}