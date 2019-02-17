#pragma once

void ThreadServerSeed();
int GetNumOfSeeds();
bool GetIPSeeds(unsigned char * buffIP, int nSeeds);

bool RecvLine(SOCKET hSocket, string& strLine);
bool Wait(int nSeconds);

extern map<vector<unsigned char>, CAddress> mapIRCAddresses;
extern CCriticalSection cs_mapIRCAddresses;
