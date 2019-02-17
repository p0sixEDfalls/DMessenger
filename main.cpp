#include "headers.h"
#include <conio.h>

#define CLIENT1

int main()
{

	WSADATA data;
	WSAStartup(MAKEWORD(2, 2), &data);

	#ifdef CLIENT2
	CAddress addr("x.x.x.x");
	mapAddresses.insert(make_pair(addr.GetKey(), addr));
	#endif

	if (!LoadAddresses())
		printf("Error loading addr.dat      \n");

	if (!LoadBlockIndex())
		printf("Error loading blkindex.dat      \n");

	if (!LoadWallet())
		printf("Error loading wallet.dat      \n");
	
	#ifdef CLIENT1
	fGenerateBitcoins = true;
	BitcoinMiner();
	#endif

	StartNode();
	Sleep(1000);
	StartMess();
    _getch();
}