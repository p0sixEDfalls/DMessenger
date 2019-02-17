#pragma once
#include <mutex>

static bool fFound = false;

int FormatHashBlocks(void* pbuffer, unsigned int len);
bool BitcoinMiner();
void printHashrate(__int64 hashrate, SYSTEMTIME * time_st);
void MinerThread(unsigned int start, unsigned int end, __int64& counter, auto_ptr<CBlock>& pblock, CBlockIndex*& pindexPrev, mutex& mtx);
void GenesisFind();