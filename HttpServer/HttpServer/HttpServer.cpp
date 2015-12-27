// HttpServer.cpp : �������̨Ӧ�ó������ڵ㡣
//

#include "stdafx.h"
#include <stdio.h>
#include <Windows.h>
#include <process.h>
#include <string.h>
#include <assert.h> 

#pragma comment(lib, "Ws2_32.lib")

#define MAXSIZE 65507              //�������ݱ��ĵ���󳤶�
#define HTTP_PORT 80               //HTTP�������˿�

//HTTP��Ҫͷ������
struct HttpHeader
{
	char method[4];                //POST��GET��ע����ЩΪCONNECT����ʵ���ݲ�����
	char url[1024];                //�����url
	char host[1024];               //Ŀ������
	char cookie[1024 * 10];        //cookie
	HttpHeader()                   //ZeroMemory��ָ�����ڴ�����㣬�ṹ������
	{
		ZeroMemory(this, sizeof(HttpHeader));
	}
};

BOOL InitSocket();
void ParseHttpHeader(char *buffer, HttpHeader *httpHeader);
BOOL ConnectToServer(SOCKET *serverSocket, char *host);
unsigned int __stdcall ProxyThread(LPVOID lpParameter);

//������ز���
SOCKET ProxyServer;
sockaddr_in ProxyServerAddr;
const int ProxyPort = 10240;

//�����µ����Ӷ�ʹ���µ��߳̽��д������̵߳�Ƶ���Ĵ����������ر��˷���Դ
//����ʹ���̳߳ؼ�����߷�����Ч��
/*
const int ProxyThreadMaxNum = 20;
HANDLE ProxyThreadHandle[ProxyThreadMaxNum] = { 0 };
DWORD ProxyThreadDW[ProxyThreadMaxNum] = { 0 };
*/

struct ProxyParam
{
	SOCKET clientSocket;
	SOCKET serverSocket;
};

int _tmain(int argc, _TCHAR* argv[])
{
	printf("�����������������\n");
	printf("��ʼ��...\n");
	if (!InitSocket())
	{
		printf("socket��ʼ��ʧ��\n");
		return -1;
	}
	printf("����������������У������˿� %d\n", ProxyPort);

	SOCKET acceptSocket = INVALID_SOCKET;
	ProxyParam *lpProxyParam;
	//HANDLE hThread;
	//DWORD dwThreadID;

	//������������ϼ���
	while (true)
	{
		acceptSocket = accept(ProxyServer, NULL, NULL);
		lpProxyParam = new ProxyParam;
		if (lpProxyParam == NULL)
		{
			continue;
		}
		lpProxyParam->clientSocket = acceptSocket;

		//hThread = (HANDLE)_beginthreadex(NULL, 0, &ProxyThread, (LPVOID)lpProxyParam, 0, 0);

		QueueUserWorkItem((LPTHREAD_START_ROUTINE)ProxyThread, (LPVOID)lpProxyParam, WT_EXECUTEINLONGTHREAD);

		//CloseHandle(hThread);

		Sleep(200);
	}

	closesocket(ProxyServer);
	WSACleanup();
	//system("pause");
	return 0;
}

//***************************************
//Method:    InitSocket
//FullName:  InitSocket
//Access:    public
//Returns:   BOOL
//Qualifier: ��ʼ���׽���
//****************************************
BOOL InitSocket()
{
	//�����׽��ֿ⣨���룩
	WORD wVersionRequested;
	//WSADATA�ṹ�������������AfxSocketInitȫ�ֺ������ص�Windows Sockets��ʼ����Ϣ��
	WSADATA wsaData;
	//�׽��ּ��ش���ʱ��ʾ
	int err;
	//WINSOCK�汾2.2
	wVersionRequested = MAKEWORD(2, 2);
	//����dll�ļ�Socket��
	err = WSAStartup(wVersionRequested, &wsaData);

	if (err != 0)
	{
		//�Ҳ���winsock.dll
		printf("����winsockʧ�ܣ��������Ϊ��%d\n", WSAGetLastError());
		return FALSE;
	}

	if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2)
	{
		printf("�����ҵ���ȷ��winsock�汾\n");
		WSACleanup();
		return FALSE;
	}
	//ָ�����ֵ�ַ����PF_INET, AF_INET�� Ipv4����Э��
	//type����������������ͨ�ŵ�Э�����ͣ�SOCK_STREAM�� �ṩ�������ӵ��ȶ����ݴ��䣬��TCPЭ��
	//����protocol����ָ��socket��ʹ�õĴ���Э���š���һ����ͨ�����������ã�һ������Ϊ0����
	ProxyServer = socket(AF_INET, SOCK_STREAM, 0);

	if (INVALID_SOCKET == ProxyServer)
	{
		printf("�����׽���ʧ�ܣ��������Ϊ��%d\n", WSAGetLastError());
		return FALSE;
	}

	//sin_family��ʾЭ��أ�һ����AF_INET��ʾTCP/IPЭ��
	ProxyServerAddr.sin_family = AF_INET;
	ProxyServerAddr.sin_port = htons(ProxyPort);
	//�����ַ
	ProxyServerAddr.sin_addr.S_un.S_addr = INADDR_ANY;

	if ( bind(ProxyServer, (SOCKADDR*)&ProxyServerAddr, sizeof(SOCKADDR)) == SOCKET_ERROR )
	{
		printf("���׽���ʧ��\n");
		return FALSE;
	}

	if (listen(ProxyServer, SOMAXCONN) == SOCKET_ERROR)
	{
		printf("�����˿�%dʧ��\n", ProxyPort);
		return FALSE;
	}

	return TRUE;
}

//***************************************
//Method:    ProxyThread
//FullName:  ProxyThread
//Access:    public
//Returns:   unsigned int __stdcall
//Qualifier: �߳�ִ�к���
//Parameter: LPVOID lpParameter
//****************************************
unsigned int __stdcall ProxyThread(LPVOID lpParameter)
{
	char Buffer[MAXSIZE];
	char *CacheBuffer;
	ZeroMemory(Buffer, MAXSIZE);

	SOCKADDR_IN clientAddr;
	int length = sizeof(SOCKADDR_IN);
	int recvSize;
	int ret;

	recvSize = recv(((ProxyParam*)lpParameter)->clientSocket, Buffer, MAXSIZE, 0);
	if (recvSize <= 0)
	{
		goto error;
	}

	HttpHeader* httpHeader = new HttpHeader();
	CacheBuffer = new char[recvSize + 1];
	ZeroMemory(CacheBuffer, recvSize + 1);
	memcpy(CacheBuffer, Buffer, recvSize);
	ParseHttpHeader(CacheBuffer, httpHeader);
	delete CacheBuffer;
	// ��ַ����
	
	if (!strcmp(httpHeader->host, "donghua.dmzj.com"))
	{
		printf("��վ����\n");
		goto error;
	}
	
	/*
	if (!strcmp(httpHeader->host, "www.sohu.com"))
	{
		memcpy(httpHeader->host, "donghua.dmzj.com", MAXSIZE);
	}
	*/
	if (!ConnectToServer(&((ProxyParam*)lpParameter)->serverSocket, httpHeader->host))
	{
		goto error;
	}

	printf("������������ %s �ɹ�\n", httpHeader->host);

	//���ͻ��˷��͵�HTTP���ݱ���ֱ��ת����Ŀ�������
	ret = send(((ProxyParam*)lpParameter)->serverSocket, Buffer, strlen(Buffer) + 1, 0);
	//�ȴ�Ŀ���������������
	recvSize = recv(((ProxyParam*)lpParameter)->serverSocket, Buffer, MAXSIZE, 0);
	
	FILE *stream;
	fopen_s(&stream, "cache.txt", "w+");
	fprintf(stream, "%s", Buffer);
	fclose(stream);
	
	if (recvSize <= 0)
	{
		goto error;
	}

	//��Ŀ����������ص�����ֱ��ת�����ͻ���
	ret = send(((ProxyParam*)lpParameter)->clientSocket, Buffer, sizeof(Buffer), 0);

	//������
error:
	printf("�ر��׽���\n");
	Sleep(200);
	closesocket(((ProxyParam*)lpParameter)->clientSocket);
	closesocket(((ProxyParam*)lpParameter)->serverSocket);
	delete lpParameter;
	//_endthreadex(0);
	return 0;
}

//***************************************
//Method:    ParseHttpHead
//FullName:  ParseHttpHead
//Access:    public
//Returns:   void
//Qualifier: ����TCP�����е�HTTPͷ��
//Parameter: char *buffer
//Parameter: HttpHeader *httpHeader
//****************************************
void ParseHttpHeader(char *buffer, HttpHeader *httpHeader)
{
	char *p;
	char *ptr;
	const char* delim = "\r\n";

	//��ȡ��һ��
	p = strtok_s(buffer, delim, &ptr);
	printf("%s\n", p);

	//GET��ʽ
	if (p[0] == 'G')
	{
		memcpy(httpHeader->method, "GET", 3);
		memcpy(httpHeader->url, &p[4], strlen(p) - 13);
	}
	//POST��ʽ
	else if (p[0] == 'P')
	{
		memcpy(httpHeader->method, "POST", 4);
		memcpy(httpHeader->url, &p[5], strlen(p) - 14);
	}
	printf("%s\n", httpHeader->url);
	//ʵ��Ϊ�ڶ���ָ�룬ָ��ض�λ��
	p = strtok_s(NULL, delim, &ptr);
	while (p)
	{
		switch (p[0])
		{
		case 'H'://Host
			memcpy(httpHeader->host, &p[6], strlen(p) - 6);
			break;
		case 'C'://Cookie
			if (strlen(p) > 8)
			{
				char header[8];
				ZeroMemory(header, sizeof(header));
				memcpy(header, p, 6);
				if (!strcmp(header, "Cookie"))
				{
					memcpy(httpHeader->cookie, &p[8], strlen(p) - 8);
				}
			}
			break;
		default:
			break;
		}
		p = strtok_s(NULL, delim, &ptr);
	}
}

//***************************************
//Method:    ConnectToServer
//FullName:  ConnectToServer
//Access:    public
//Returns:   BOOL
//Qualifier: ������������Ŀ��������׽��ֲ�����
//Parameter: SOCKET *serverSocket
//Parameter: char *host
//****************************************
BOOL ConnectToServer(SOCKET *serverSocket, char *host)
{
	sockaddr_in serverAddr;
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(HTTP_PORT);
	HOSTENT *hostent = gethostbyname(host);
	if (!hostent)
	{
		return FALSE;
	}

	in_addr Inaddr = *((in_addr*)*hostent->h_addr_list);
	serverAddr.sin_addr.s_addr = inet_addr(inet_ntoa(Inaddr));
	*serverSocket = socket(AF_INET, SOCK_STREAM, 0);
	if (*serverSocket == INVALID_SOCKET)
	{
		return FALSE;
	}

	if (connect(*serverSocket, (SOCKADDR *)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR)
	{
		closesocket(*serverSocket);
		return FALSE;
	}
	return TRUE;
}