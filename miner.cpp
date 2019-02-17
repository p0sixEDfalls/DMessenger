#include <thread>

#include "sha.h"
#include "headers.h"

using CryptoPP::ByteReverse;
static int detectlittleendian = 1;

void BlockSHA256(const void* pin, unsigned int nBlocks, void* pout)
{
	unsigned int* pinput = (unsigned int*)pin;
	unsigned int* pstate = (unsigned int*)pout;

	CryptoPP::SHA256::InitState(pstate);

	if (*(char*)&detectlittleendian != 0)
	{
		for (int n = 0; n < nBlocks; n++)
		{
			unsigned int pbuf[16];
			for (int i = 0; i < 16; i++)
				pbuf[i] = ByteReverse(pinput[n * 16 + i]);
			CryptoPP::SHA256::Transform(pstate, pbuf);
		}
		for (int i = 0; i < 8; i++)
			pstate[i] = ByteReverse(pstate[i]);
	}
	else
	{
		for (int n = 0; n < nBlocks; n++)
			CryptoPP::SHA256::Transform(pstate, pinput + n * 16);
	}
}

int FormatHashBlocks(void* pbuffer, unsigned int len)
{
    unsigned char* pdata = (unsigned char*)pbuffer;
    unsigned int blocks = 1 + ((len + 8) / 64);
    unsigned char* pend = pdata + 64 * blocks;
    memset(pdata + len, 0, 64 * blocks - len);
    pdata[len] = 0x80;
    unsigned int bits = len * 8;
    pend[-1] = (bits >> 0) & 0xff;
    pend[-2] = (bits >> 8) & 0xff;
    pend[-3] = (bits >> 16) & 0xff;
    pend[-4] = (bits >> 24) & 0xff;
    return blocks;
}

bool BitcoinMiner()
{
    printf("BitcoinMiner started\n");

    CBigNum bnExtraNonce = 0;

    thread minerThreads[8];
    __int64 count[8] = { 0,0,0,0,0,0,0,0 };
    mutex mtx;

    while (fGenerateBitcoins)
    {
        Sleep(50);
        //CheckForShutdown(3);
       /*while (vNodes.empty())
        {
            Sleep(1000);
            //CheckForShutdown(3);
        }*/

        unsigned int nTransactionsUpdatedLast = nTransactionsUpdated;
        CBlockIndex* pindexPrev = pindexBest;
        unsigned int nBits = GetNextWorkRequired(pindexPrev);


        //
        // Create coinbase tx
        //
        CTransaction txNew;
        txNew.vin.resize(1);
        txNew.vin[0].prevout.SetNull();
        txNew.vin[0].scriptSig << nBits << ++bnExtraNonce;
        txNew.vout.resize(1);
        txNew.vout[0].scriptPubKey << keyUser.GetPubKey() << OP_CHECKSIG;

        cout << txNew.GetHash().ToString() << endl;

        //
        // Create new block
        //
        auto_ptr<CBlock> pblock(new CBlock());
        if (!pblock.get())
            return false;

        // Add our coinbase tx as first transaction
        pblock->vtx.push_back(txNew);

        // Collect the latest transactions into the block
        int64 nFees = 0;
        CRITICAL_BLOCK(cs_main)
            CRITICAL_BLOCK(cs_mapTransactions)
        {
            CTxDB * txdb = new CTxDB("r");
            map<uint256, CTxIndex> mapTestPool;
            vector<char> vfAlreadyAdded(mapTransactions.size());
            bool fFoundSomething = true;
            unsigned int nBlockSize = 0;
            while (fFoundSomething && nBlockSize < MAX_SIZE / 2)
            {
                fFoundSomething = false;
                unsigned int n = 0;
                for (map<uint256, CTransaction>::iterator mi = mapTransactions.begin(); mi != mapTransactions.end(); ++mi, ++n)
                {
                    if (vfAlreadyAdded[n])
                        continue;
                    CTransaction& tx = (*mi).second;
                    if (tx.IsCoinBase() || !tx.IsFinal())
                        continue;

                    // Transaction fee requirements, mainly only needed for flood control
                    // Under 10K (about 80 inputs) is free for first 100 transactions
                    // Base rate is 0.01 per KB
                    int64 nMinFee = tx.GetMinFee(pblock->vtx.size() < 100);

                    map<uint256, CTxIndex> mapTestPoolTmp(mapTestPool);
                    if (!tx.ConnectInputs(*txdb, mapTestPoolTmp, CDiskTxPos(1, 1, 1), 0, nFees, false, true, nMinFee))
                        continue;
                    swap(mapTestPool, mapTestPoolTmp);

                    pblock->vtx.push_back(tx);
                    nBlockSize += ::GetSerializeSize(tx, SER_NETWORK);
                    vfAlreadyAdded[n] = true;
                    fFoundSomething = true;
                }
            }
        }
        pblock->nBits = nBits;
        pblock->vtx[0].vout[0].nValue = pblock->GetBlockValue(nFees);
        pblock->nVersion = 1;
        pblock->nTime = max((pindexPrev ? pindexPrev->GetMedianTimePast() + 1 : 0), GetAdjustedTime());
        pblock->nNonce = 0;
        pblock->hashMerkleRoot = pblock->BuildMerkleTree();
        pblock->hashPrevBlock = (pindexPrev ? pindexPrev->GetBlockHash() : 0);
        printf("\n\nRunning StreebogMiner with %d transactions in block\n", pblock->vtx.size());

        int g = 1;
   
        unsigned int interval = 4294967295 / g;
        unsigned int globStart = 0;
        unsigned int globEnd = interval;

        printf("CPU miner using SHA256 algorithm\n");
        printf("With %d threads of miner\n", g);

		MinerThread(globStart, globEnd, std::ref(count[0]), std::ref(pblock), std::ref(pindexPrev), std::ref(mtx));

		fGenerateBitcoins = false;
    }
    return true;
}

/*
===========================================================
					MULTITHREAD MINER LATER
					USE ONE THREAD
===========================================================
*/

void MinerThread(unsigned int start, unsigned int end, __int64& counter, auto_ptr<CBlock>& pblock, CBlockIndex*& pindexPrev,mutex& mtx)
{
    struct unnamed1
    {
        struct unnamed2
        {
            int nVersion;
            uint256 hashPrevBlock;
            uint256 hashMerkleRoot;
            unsigned int nTime;
            unsigned int nBits;
            unsigned int nNonce;
        }
        block;
        unsigned char pchPadding0[64];
        uint256 hash1;
        unsigned char pchPadding1[64];
    }
    tmp;

    uint256 hash = 0;
    unsigned int nStart = GetTime();
    uint256 hashTarget = CBigNum().SetCompact(pblock->nBits).getuint256();

    cout << hashTarget.ToString() << endl;

    tmp.block.nVersion = pblock->nVersion;
    tmp.block.hashPrevBlock = pblock->hashPrevBlock;
    tmp.block.hashMerkleRoot = pblock->hashMerkleRoot;
    tmp.block.nTime = pblock->nTime;
    tmp.block.nBits = pblock->nBits;
    tmp.block.nNonce = start;

	unsigned int nBlocks0 = FormatHashBlocks(&tmp.block, sizeof(tmp.block));
	unsigned int nBlocks1 = FormatHashBlocks(&tmp.hash1, sizeof(tmp.hash1));

    if (tmp.block.nNonce == 0)
        tmp.block.nNonce += 1;

    while(tmp.block.nNonce < end)
    {
		BlockSHA256(&tmp.block, nBlocks0, &tmp.hash1);
		BlockSHA256(&tmp.hash1, nBlocks1, &hash);

        if (hash <= hashTarget)
        {
            /*mtx.lock(); LATER

            if (fFound == true) //is the best way to check?
                break;*/

            cout << hash.ToString() << " " << tmp.block.nNonce << endl;
            
            pblock->nNonce = tmp.block.nNonce;

            cout << pblock->GetHash().ToString() << endl;

            assert(hash == pblock->GetHash());
         
            //// debug print
            printf("Miner:\n");
            printf("proof-of-work found  \n  hash: %s  \ntarget: %s\n", hash.GetHex().c_str(), hashTarget.GetHex().c_str());
            pblock->print();

            CRITICAL_BLOCK(cs_main)
            {
                // Process this block the same as if we had received it from another node
                if (!ProcessBlock(NULL, pblock.release()))
                    printf("ERROR in BitcoinMiner, ProcessBlock, block not accepted\n");
            }

            fFound = true; 
            Sleep(500);  
            break;
        }

        if ((tmp.block.nNonce & 0x3ffff) == 0)
        {
            //cout << hash.ToString() << endl;
            //CheckForShutdown(3);
            if (tmp.block.nNonce == 0)
                break;
            //if (fFound)
                //break;

			SYSTEMTIME time;	//for  single thread
			GetLocalTime(&time);

			printHashrate(counter, &time);
			counter = 0;
			tmp.block.nTime = pblock->nTime = max(pindexPrev->GetMedianTimePast() + 1, GetAdjustedTime());
        }
        counter++;
        tmp.block.nNonce++;
    }
}

void printHashrate(__int64 hashrate, SYSTEMTIME * time_st)
{
    printf("[%hu-%02hu-%02hu %hu:%02hu:%02hu] - %I64u H\\s,", time_st->wYear, time_st->wMonth, time_st->wDay, time_st->wHour, time_st->wMinute, time_st->wSecond, hashrate);
}

void GenesisFind()
{
	struct unnamed1
	{
		struct unnamed2
		{
			int nVersion;
			uint256 hashPrevBlock;
			uint256 hashMerkleRoot;
			unsigned int nTime;
			unsigned int nBits;
			unsigned int nNonce;
		}
		block;
		unsigned char pchPadding0[64];
		uint256 hash1;
		unsigned char pchPadding1[64];
	}
	tmp;

	uint256 hash;
	uint256 hashTarget = GetMaxTarget();//CBigNum().SetCompact(0x1f00ffff).getuint256();

	cout << hashTarget.ToString() << endl;

	char pszTimestamp[19] = "F*ck the goverment";
	CTransaction txNew;
	txNew.vin.resize(1);
	txNew.vout.resize(1);
	txNew.vin[0].scriptSig = CScript() << 520159231 << CBigNum(4) << vector<unsigned char>((unsigned char*)pszTimestamp, (unsigned char*)pszTimestamp + strlen(pszTimestamp));
	txNew.vout[0].nValue = 50 * COIN;
	txNew.vout[0].scriptPubKey = CScript() << CBigNum("0x5F1DF16B2B704C8A578D0BBAF74D385CDE12C11EE50455F3C438EF4C3FBCF649B6DE611FEAE06279A60939E028A8D65C10B73071A6F16719274855FEB0FD8A6704") << OP_CHECKSIG;
	txNew.IPAdress = 0xffeedd00;
	txNew.hash160.push_back(uint160(0));

	CBlock block;
	block.vtx.push_back(txNew);
	block.hashPrevBlock = 0;
	block.hashMerkleRoot = block.BuildMerkleTree();
	block.nVersion = 1;
	block.nTime = 1547138264;
	block.nBits = 0x1f00ffff;
	block.nNonce = 1;

	cout << block.hashMerkleRoot.ToString() << endl;

	tmp.block.nVersion = block.nVersion;
	tmp.block.hashPrevBlock = block.hashPrevBlock;
	tmp.block.hashMerkleRoot = block.hashMerkleRoot;
	tmp.block.nTime = block.nTime;
	tmp.block.nBits = block.nBits;
	tmp.block.nNonce = 0;

	unsigned int nBlocks0 = FormatHashBlocks(&tmp.block, sizeof(tmp.block));
	unsigned int nBlocks1 = FormatHashBlocks(&tmp.hash1, sizeof(tmp.hash1));

	while (hash <= hashTarget)
	{
		tmp.block.nNonce++;
		block.nNonce = tmp.block.nNonce;

		BlockSHA256(&tmp.block, nBlocks0, &tmp.hash1);
		BlockSHA256(&tmp.hash1, nBlocks1, &hash);
	}
	printf("proof-of-work found  \n  hash: %s  \ntarget: %s\nnonce: %u\n", hash.GetHex().c_str(), hashTarget.GetHex().c_str(), block.nNonce);
}
