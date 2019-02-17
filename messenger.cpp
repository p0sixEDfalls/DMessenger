#include <thread>
#include <conio.h>

#include "headers.h"

ThreadSafeVector<event> vEvents;
ThreadSafeVector<Dialog*> vDialogs;
verifyMsg vm;

int StartMess()
{
	if (GenerateSign(&vm))
	{
		printf("Sign is created succefully\n");
		printf("Your ID is %s\n", Hash160(vm.vPubKey).ToString().c_str());
	}
	else
	{
		printf("Error create sign\n");
	}

	struct addrinfo *result = NULL, *ptr = NULL, hints;

	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_flags = AI_PASSIVE;

	int iResult = getaddrinfo(NULL, MESS_PORT, &hints, &result);
	if (iResult != 0)
	{
		printf("getaddrinfo failed: %d\n", iResult);
		WSACleanup();
		return 1;
	}

	SOCKET hListenSocket = INVALID_SOCKET;
	hListenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
	if (hListenSocket == INVALID_SOCKET)
	{
		printf("Error at socket(): %ld\n", WSAGetLastError());
		freeaddrinfo(result);
		WSACleanup();
		return 1;
	}

	iResult = ::bind(hListenSocket, result->ai_addr, (int)result->ai_addrlen);
	if (iResult == SOCKET_ERROR)
	{
		printf("bind failed with error: %d\n", WSAGetLastError());
		freeaddrinfo(result);
		closesocket(hListenSocket);
		WSACleanup();
		return 1;
	}

	freeaddrinfo(result);

	u_long nOne = 1;
	if (ioctlsocket(hListenSocket, FIONBIO, &nOne) == SOCKET_ERROR)
		printf("ConnectSocket() : ioctlsocket nonblocking setting failed, error %d\n", WSAGetLastError());

	if (listen(hListenSocket, SOMAXCONN) == SOCKET_ERROR)
	{
		printf("Listen failed with error: %ld\n", WSAGetLastError());
		closesocket(hListenSocket);
		WSACleanup();
		return 1;
	}

	thread thrConnection(StartThreadConnection);
	thread thrMessagesR(StartThreadMessagesR);
	thread thrMessagesS(StartThreadMessagesS);
	thread thrAccept(StartThreadAccept, new SOCKET(hListenSocket));

	string hashStr;
	uint160 hash;
	string msg;
	char cmd;
	char curDiag;
	bool isMenu = true;
	bool isDialog = false;

	loop
	{
		cmd = _getch();

		switch(cmd)
		{
			case 's':
				if (isDialog)
				{
					printf("Write a message: ");
					getline(cin, msg);
					if (msg == "/menu")
					{
						isMenu = true;
						isDialog = false;
						break;
					}
					if (!SendMessageC(curDiag, msg))
						printf("error send\n");
				}
				if (isMenu)
				{
					printf("Write a command: ");
					getline(cin, msg);
					if (msg == "/setpaswd")
					{
						printf("Enter user ID: ");
						getline(cin, msg);
						hash.SetHex(msg);
						printf("Enter password: ");
						getline(cin, msg);
						SetKey(hash, (unsigned char*)msg.c_str(), msg.size());
						break;
					}
					if (msg == "/cnttoip")
					{
						printf("Enter user IP: ");
						getline(cin, msg);
						CAddress addr = CAddress(msg.c_str());
						vEvents.lock();
						vEvents.pushBack(make_pair(NEW_CNCT, addr.ip));
						vEvents.unlock();
						break;
					}
					if (msg == "/cnttohash")
					{
						printf("Enter user hash: ");
						getline(cin, msg);
						uint160 hash;
						hash.SetHex(msg);
						printf("Enter user IP: ");
						getline(cin, msg);
						CAddress addr = CAddress(msg.c_str());
						CWalletTx wtx;

						wtx.hash160.push_back(hash);
						wtx.IPAdress = addr.ip;

						int64 nValue = 0;
						CScript scriptPubKey;
						scriptPubKey << OP_DUP << OP_HASH160 << hash << OP_EQUALVERIFY << OP_CHECKSIG;
						if (SendMoney(scriptPubKey, nValue, wtx))
							printf("sended!\n");

						fGenerateBitcoins = true;
						BitcoinMiner();
					}
				}
			break;

			case 0:
				if (isMenu)
				{
					clear();
					printf("Your ID is %s\n\n", Hash160(vm.vPubKey).ToString().c_str());
					printf("Dialogs:\n");
					PrintDialogs();
				}
				if (isDialog)
				{
					clear();
					printf("Your ID is %s\n\n", Hash160(vm.vPubKey).ToString().c_str());
					PrintMessages(curDiag);
				}
			break;

			case '0':
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
				isMenu = false;
				isDialog = true;
				curDiag = cmd;
				clear();
				printf("Your ID is %s\n\n", Hash160(vm.vPubKey).ToString().c_str());
				PrintMessages(curDiag);
			break;
		}	
	}
}

void StartThreadConnection()
{
	loop
	{
		ThreadConnection();
		Sleep(1000);
	}
}

void StartThreadAccept(SOCKET * pListenSocket)
{
	SOCKET hListenSocket = *pListenSocket;
	loop
	{
		ThreadAccept(new SOCKET(hListenSocket));
		Sleep(1000);
	}
}

void StartThreadMessagesR()
{
	loop
	{
		ThreadMessagesR();
		Sleep(1000);
	}
}

void StartThreadMessagesS()
{
	loop
	{
		ThreadMessagesS();
		Sleep(1000);
	}
}

void ThreadConnection()
{
	vDialogs.lock();
	vEvents.lock();
		
	event e;
	vEvents.front(e);

	if(e.first == NULL)
	{		
		vDialogs.unlock();
		vEvents.unlock();
		return; 
	}
			
	if(e.first == NEW_CNCT)
	{
		CAddress addrConnect(e.second);
		printf("Trying to %s\n", addrConnect.ToStringIP().c_str());
		Dialog* pdiag = ConnectDialog(addrConnect, &vm);
		if(pdiag != NULL)
		{
			printf("connected to %s\n", addrConnect.ToString().c_str());
			vDialogs.pushBack(pdiag);
			vEvents.pop();
		}
	}

	vDialogs.unlock();
	vEvents.unlock();
}

void ThreadAccept(SOCKET * pListenSocket)
{
	SOCKET hListenSocket = *pListenSocket;
	sockaddr_in sockAddr;
	int sockAddrLen = sizeof(sockAddr);

	vDialogs.lock();
	SOCKET ClientSocket = INVALID_SOCKET;
	ClientSocket = accept(hListenSocket, (sockaddr*)&sockAddr, &sockAddrLen);
	
	if(ClientSocket != INVALID_SOCKET)
	{
		unsigned int ip = sockAddr.sin_addr.s_addr;
		CAddress addrConnect(ip);	
		Dialog* pdiag = new Dialog(ClientSocket, addrConnect);
		printf("%s is connected\n", addrConnect.ToString().c_str());
		vDialogs.pushBack(pdiag);
	}

	vDialogs.unlock();
}

void ThreadMessagesR()
{
	vDialogs.lock();

	foreach(Dialog* d, vDialogs.getVector())
	{
		SOCKET hSocket = d->hSocket;
		CDataStream& ds = d->messagesR;

		if(!d->IsVerify())
		{
			unsigned int nPos = ds.size();
			ds.resize(nPos + MAX_MSG_LEN);
			int nBytes = recv(hSocket, &ds[nPos], MAX_MSG_LEN, 0);
			if(nBytes == 0 || nBytes == -1)
			{
				printf("recv failed with error: %d\n", WSAGetLastError());
				ds.clear();
				continue;
			}

			if(!d->Verify())
			{
				printf("Error verify %s\n", d->dialogIP.ToStringIP().c_str());
				continue;
			}
			else
			{
				cout << d->dialogIP.ToStringIP() << " verified as " << d->hash.ToString() << endl;
				if(!d->sendVerif)
					d->messagesS << vm.magic << vm.vPubKey << vm.vSign;
				continue;
			}
		}

		unsigned int nPos = ds.size();
		ds.resize(nPos + MAX_MSG_LEN);
		int nBytes = recv(hSocket, &ds[nPos], MAX_MSG_LEN, 0);
		if(nBytes == 0 || nBytes == -1)
		{
			ds.clear();
			continue;
		}

		ds.resize(nPos + max(nBytes, 0));
		message msg;
		msg.isMine = false;
		msg.time = GetTime();
		ds >> msg.msg;

		//DO IT IN FUNCTION
		int deLen = msg.msg.size();
		unsigned char * deMsg = aes_decrypt(&d->de, (unsigned char *)msg.msg.c_str(), &deLen);
		string deStrMsg((const char *)deMsg, deLen);
		msg.msg = deStrMsg;
		free(deMsg);

		d->vMessages.push_back(msg);
		//cout << "New message from " << d->hash.ToString() << ": " << msg.msg << endl;
		ds.clear();
	}
		
	vDialogs.unlock();
}

void ThreadMessagesS()
{		
	vDialogs.lock();

	foreach(Dialog* d, vDialogs.getVector())
	{
		SOCKET hSocket = d->hSocket;
		CDataStream& ds = d->messagesS;

		if(!d->sendVerif && ds.empty())
			continue;
		else if(!d->sendVerif && !ds.empty())
		{
			int iResult = send(hSocket, &ds[0], ds.size(), 0);
			if(iResult == SOCKET_ERROR)
			{
				printf("Send verifymsg failed with error: %d\n", WSAGetLastError());
				continue;
			}
			d->sendVerif = true;
			ds.clear();
			continue;
		}

		if(!d->IsVerify())
			continue;

		if(ds.empty())
			continue;

		int iResult = send(hSocket, &ds[0] , ds.size(), 0);
		if(iResult == SOCKET_ERROR)
		{
			printf("error send message to %s::%s\n", d->dialogIP.ToStringIP().c_str(), d->hash.ToString().c_str());
		}

		message msg;
		msg.isMine = true;
		msg.time = GetTime();
		ds >> msg.msg;

		int deLen = msg.msg.size();
		unsigned char * deMsg = aes_decrypt(&d->de, (unsigned char *)msg.msg.c_str(), &deLen);
		string deStrMsg((const char *)deMsg, deLen);
		msg.msg = deStrMsg;
		free(deMsg);

		d->vMessages.push_back(msg);

		ds.clear();
	}
	
	vDialogs.unlock();
}

Dialog* ConnectDialog(CAddress& addrConnect, verifyMsg* vm2)
{
	SOCKET ConnectSocket = INVALID_SOCKET;
	struct addrinfo *result = NULL,
		*ptr = NULL,
		hints;
	int iResult;

	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;


	iResult = getaddrinfo(addrConnect.ToStringIP().c_str(), MESS_PORT, &hints, &result);
	if (iResult != 0)
	{
		printf("getaddrinfo failed with error: %d\n", iResult);
		return NULL;
	}

	for (ptr = result; ptr != NULL; ptr = ptr->ai_next)
	{
		ConnectSocket = socket(ptr->ai_family, ptr->ai_socktype,
			ptr->ai_protocol);
		if (ConnectSocket == INVALID_SOCKET)
		{
			printf("socket failed with error: %ld\n", WSAGetLastError());
			return NULL;
		}

		iResult = connect(ConnectSocket, ptr->ai_addr, (int)ptr->ai_addrlen);
		if (iResult == SOCKET_ERROR)
		{
			closesocket(ConnectSocket);
			ConnectSocket = INVALID_SOCKET;
			continue;
		}
		break;
	}

	freeaddrinfo(result);

	if (ConnectSocket == INVALID_SOCKET)
	{
		printf("Unable to connect to server!\n");
		printf("connect failed with error: %d\n", WSAGetLastError());
		return NULL;
	}

	u_long nOne = 1;
	if (ioctlsocket(ConnectSocket, FIONBIO, &nOne) == SOCKET_ERROR)
		printf("ConnectSocket() : ioctlsocket nonblocking setting failed, error %d\n", WSAGetLastError());

	Dialog* pdiag = new Dialog(ConnectSocket, addrConnect);
	return pdiag;
}

inline bool GenerateSign(verifyMsg* vm)
{
	vm->magic = 0xabadbabe;
	vm->vPubKey = keyUser.GetPubKey();
	if (keyUser.Sign(verifyMagic, sizeof(verifyMagic), vm->vSign))
		return true;
	return false;
}

bool SendMessageM(uint160 hash, string& msg)
{
	if (hash == 0 || msg.empty())
		return false;
	vDialogs.lock();
	foreach(Dialog* d, vDialogs.getVector())
	{
		CDataStream& ds = d->messagesS;
		if (hash == d->hash && d->sendVerif)
		{
			ds << msg;
			vDialogs.unlock();
			return true;
		}
	}
	vDialogs.unlock();
	return false;
}

bool SetKey(uint160 hash, unsigned char * keyStr, int keyLen)
{
	vDialogs.lock();
	foreach(Dialog* d, vDialogs.getVector())
	{
		if (d->hash == hash)
		{
			if (aes_init(keyStr, keyLen, &d->en, &d->de) == 0)
			{
				printf("Encryption key for %s is created\n", hash.ToString().c_str());
				vDialogs.unlock();
				return true;
			}
			else
			{
				printf("Error encryption key creating for %s\n", hash.ToString().c_str());
				vDialogs.unlock();
				return false;
			}
		}
	}
	vDialogs.unlock();
	return false;
}

bool GetMyExternalIP3(CAddress& addr)
{
	HINTERNET net = InternetOpen("IP retriever", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
	
	if (net == NULL)
		return false;

	HINTERNET conn = InternetOpenUrl(net, "http://myexternalip.com/raw", NULL, 0, INTERNET_FLAG_RELOAD, 0);

	if (conn == NULL)
		return false;

	char buffer[16];
	DWORD read;

	InternetReadFile(conn, buffer, sizeof(buffer) / sizeof(buffer[0]), &read);
	InternetCloseHandle(net);

	addr = CAddress(string(buffer, read).c_str());
	return true;
}

/*
===========================================================
				CLI FUNCTIONS FOR MESSENGER
				DELETE IT WHEN GUI 
===========================================================
*/

void PrintDialogs()
{
	vDialogs.lock();
	int nConv = 0;
	HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
	foreach(Dialog* d, vDialogs.getVector())
	{
		if (!d->IsVerify())
			continue;
		printf("[");
		SetConsoleTextAttribute(hConsole, 11);
		printf("%d", nConv);
		SetConsoleTextAttribute(hConsole, 15);
		printf("]");
		SetConsoleTextAttribute(hConsole, 10);
		printf("%s", d->hash.ToString().c_str());
		SetConsoleTextAttribute(hConsole, 15);
		printf(" : ");
		if (!d->vMessages.empty())
			printf("%s\n", d->vMessages.back().msg.c_str());
		else
			printf("No messages yet\n");
		nConv++;
	}
	vDialogs.unlock();
}

void clear() 
{
	COORD topLeft = { 0, 0 };
	HANDLE console = GetStdHandle(STD_OUTPUT_HANDLE);
	CONSOLE_SCREEN_BUFFER_INFO screen;
	DWORD written;

	GetConsoleScreenBufferInfo(console, &screen);
	FillConsoleOutputCharacterA(
		console, ' ', screen.dwSize.X * screen.dwSize.Y, topLeft, &written
	);
	FillConsoleOutputAttribute(
		console, FOREGROUND_GREEN | FOREGROUND_RED | FOREGROUND_BLUE,
		screen.dwSize.X * screen.dwSize.Y, topLeft, &written
	);
	SetConsoleCursorPosition(console, topLeft);
}

void PrintMessages(char nDiag)
{
	vDialogs.lock();
	char cntC = '0';
	HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
	foreach(Dialog* d, vDialogs.getVector())
	{
		if (!d->IsVerify())
			continue;
		if (d->vMessages.empty())
			continue;
		if (cntC == nDiag && cntC < '9')
		{
			for (vector<message>::const_iterator it = d->vMessages.begin(); it != d->vMessages.end(); it++)
			{
				if (it->isMine)
					printf("Me : %s\n", it->msg.c_str());
				else
				{
					SetConsoleTextAttribute(hConsole, 10);
					printf("%s", d->hash.ToString().c_str());
					SetConsoleTextAttribute(hConsole, 15);
					printf(" : %s\n", it->msg.c_str());
				}
			}
		}
		cntC++;
	}
	vDialogs.unlock();
}

bool SendMessageC(char nDiag, string& msg)
{
	vDialogs.lock();
	char cntC = '0';
	foreach(Dialog* d, vDialogs.getVector())
	{
		if (!d->IsVerify())
			continue;

		if (cntC == nDiag && cntC < '9')
		{
			CDataStream& ds = d->messagesS;
			int enLen = msg.size();
			unsigned char * enMsg = aes_encrypt(&d->en, (unsigned char*)msg.c_str(), &enLen);
			string strEnMsg((const char *)enMsg, enLen);
			ds << strEnMsg;
			free(enMsg);
			vDialogs.unlock();
			return true;
		}
		cntC++;
	}
	vDialogs.unlock();
	return false;
}