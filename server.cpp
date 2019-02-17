#include "headers.h"

CAddress * server = new CAddress("18.188.254.109");

map<vector<unsigned char>, CAddress> mapIRCAddresses;
CCriticalSection cs_mapIRCAddresses;


void ThreadServerSeed()
{
    //loop
    //{
        int nSeeds = GetNumOfSeeds();
        if (nSeeds)
        {
            unsigned char * BuffIP = (unsigned char *)malloc(nSeeds * 4);
            bool flag = GetIPSeeds(BuffIP, nSeeds);
            if (flag)
                printf("Seeds are saved!\n");
            free(BuffIP);
        }
        //Wait(120);
    //}
}

int GetNumOfSeeds()
{
    int nSeeds = 0;
    SOCKET sock = -1;
    sockaddr_in addrRemote = {-1};
    int res = 0;

    sock = socket(AF_INET, SOCK_STREAM, 0);

    if (sock != -1)
    {
        addrRemote = (*server).GetSockAddr();
        res = connect(sock, (sockaddr*)&addrRemote, sizeof(addrRemote));
        if (res != -1)
        {
            unsigned int ip = addrLocalHost.ip;
            unsigned char sendMsg[9] = { 0x42 };
            memcpy(&sendMsg[1], &ip, 4);
            int msgLen = 9;
            res = send(sock, (char *)sendMsg, msgLen, 0);
            if (res != -1)
            {
                unsigned char szBuffer[4];
                res = recv(sock, (char *)szBuffer, 4, 0);
                if (res > 0)
                {
                    memcpy(&nSeeds, szBuffer, 4);
                    printf("Number of seeds: %d\n", nSeeds);
                }
                else
                {
                    printf("RECIEVE ERROR in GetNumOfSeeds()\n");
                    closesocket(sock);
                    return 0;
                }
            }
            else
            {
                printf("SEND ERROR in GetNumOfSeeds()\n");
                closesocket(sock);
                return 0;
            }
        }
        else
        {
            printf("Connect ERROR in GetNumOfSeeds()\n");
            return 0;
        }

    }
    else
    {
        printf("SOCKET ERROR in GetNumOfSeeds()\n");
        return 0;
    }                    
    closesocket(sock);
    return nSeeds;
}

bool GetIPSeeds(unsigned char * buffIP, int nSeeds)
{
    SOCKET sock = -1;
    sockaddr_in addrRemote = {-1};
    int res = 0;

    sock = socket(AF_INET, SOCK_STREAM, 0);

    if (sock != -1)
    {
        addrRemote = (*server).GetSockAddr();
        res = connect(sock, (sockaddr*)&addrRemote, sizeof(addrRemote));
        if (res != -1)
        {
            unsigned int ip = addrLocalHost.ip;
            unsigned char sendMsg[9] = { 0x43 };
            memcpy(&sendMsg[1], &ip, 4);
            memcpy(&sendMsg[5], &nSeeds, 4);
            int msgLen = 9;
            res = send(sock, (char *)sendMsg, msgLen, 0);
            if (res != -1)
            {
                res = recv(sock, (char *)buffIP, 4 * nSeeds, 0);
                if (res > 0)
                {
					CAddrDB * addrdb = new CAddrDB();

                    for (int i = 0; i < nSeeds; i++)
                    {
                        unsigned int ip = 0;

                        ///debug
                        buffIP[0]--;

                        memcpy(&ip, buffIP + (i * 4), 4);
                        CAddress * CIP = new CAddress(ip);
                        (*CIP).print();

                        
                        if (!AddAddress(*addrdb, *CIP))
                        {
                            printf("ERROR AddAdress\n");
                            closesocket(sock);
                            return false;
                        }

                        CRITICAL_BLOCK(cs_mapIRCAddresses)
                        {
                            map<vector<unsigned char>, CAddress>::iterator it = mapIRCAddresses.find((*CIP).GetKey());
                            if(it == mapIRCAddresses.end())
                                mapIRCAddresses.insert(make_pair((*CIP).GetKey(), *CIP));
                        }
						delete CIP;
                    }
					delete addrdb;
                }
                else
                {
                    printf("RECIEVE ERROR in GerIPSeeds()\n");
                    _close(sock);
                    return false;
                }
            }
            else
            {
                printf("SEND ERROR in GerIPSeeds()\n");
                _close(sock);
                return false;
            }
        }
        else
        {
            printf("Connect ERROR in GerIPSeeds()\n");
            _close(sock);
            return false;
        }

    }
    else
    {
        printf("SOCKET ERROR in GerIPSeeds()\n");
        return false;
    }

    closesocket(sock);
    return true;
}

bool RecvLine(SOCKET hSocket, string& strLine)
{
    strLine = "";
    loop
    {
        char c;
        int nBytes = recv(hSocket, &c, 1, 0);
        if (nBytes > 0)
        {
            if (c == '\n')
                continue;
            if (c == '\r')
                return true;
            strLine += c;
        }
        else if (nBytes <= 0)
        {
            if (!strLine.empty())
                return true;
            // socket closed
            printf("Server socket closed\n");
            return false;
        }
        else
        {
            // socket error
            int nErr = WSAGetLastError();
            if (nErr != WSAEMSGSIZE && nErr != WSAEINTR && nErr != WSAEINPROGRESS)
            {
                printf("Server recv failed: %d\n", nErr);
                return false;
            }
        }
    }
}

bool Wait(int nSeconds)
{
    if (fShutdown)
        return false;
    printf("Waiting %d seconds to reconnect to server\n", nSeconds);
    for (int i = 0; i < nSeconds; i++)
    {
        if (fShutdown)
            return false;
        Sleep(1000);
    }
    return true;
}