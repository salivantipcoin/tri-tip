#include "bitcoin_messages_handlers.h"
#include "main.h"
#include "alert.h"
#include "checkpoints.h"



#include "alert.h"
#include "checkpoints.h"
#include "db.h"
#include "txdb.h"
#include "net.h"
#include "init.h"
#include "ui_interface.h"
#include "checkqueue.h"
#include <boost/algorithm/string/replace.hpp>
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>


using namespace boost;

multimap<uint256, CBlock*> mapOrphanBlocksByPrev;

extern CCriticalSection cs_main;

CTxMemPool mempool;

bool fImporting = false;
bool fReindex = false;
bool fBenchmark = false;
bool fTxIndex = false;
CCoinsViewCache *pcoinsTip = NULL;
uint256 hashBestChain = 0;
static const int64 nTargetSpacing = 10 * 60;
static const int64 nTargetTimespan = 14 * 24 * 60 * 60; // two weeks

CBlockIndex* pindexGenesisBlock = NULL;
CBlockIndex* pindexBest = NULL;
map<uint256, CBlock*> mapOrphanBlocks;
map<uint256, CDataStream*> mapOrphanTransactions;
static CBigNum bnProofOfWorkLimit(~uint256(0) >> 32);

extern map<uint256, CAlert> mapAlerts;
extern CCriticalSection cs_mapAlerts;
map<uint256, CBlockIndex*> mapBlockIndex;
std::vector<CBlockIndex*> vBlockIndexByHeight;

uint256 hashGenesisBlock("0x000000000019d6689c085ae165831e934ff763ae46a2a6c172b3f1b60a8ce26f");

CMedianFilter<int> cPeerBlockCounts(8, 0); // Amount of blocks that other nodes claim to have

/* don't participate  in transfering messages in bitcoin network for now */
void static ProcessGetData(CNode* pfrom)
{
    vector<CInv> vNotFound;
	pfrom->PushMessage("notfound", vNotFound);
}

bool IsInitialBlockDownload()
{
    return true;
}

uint256 static GetOrphanRoot(const CBlockHeader* pblock)
{
    // Work back to the first block in the orphan chain
    while (mapOrphanBlocks.count(pblock->hashPrevBlock))
        pblock = mapOrphanBlocks[pblock->hashPrevBlock];
    return pblock->GetHash();
}


bool IsFinalTx(const CTransaction &tx, int nBlockHeight, int64 nBlockTime)
{
    // Time based nLockTime implemented in 0.1.6
    if (tx.nLockTime == 0)
        return true;
    if (nBlockHeight == 0)
        nBlockHeight = nBestHeight;
    if (nBlockTime == 0)
        nBlockTime = GetAdjustedTime();
    if ((int64)tx.nLockTime < ((int64)tx.nLockTime < LOCKTIME_THRESHOLD ? (int64)nBlockHeight : nBlockTime))
        return true;
    BOOST_FOREACH(const CTxIn& txin, tx.vin)
        if (!txin.IsFinal())
            return false;
    return true;
}


bool CBlockIndex::IsSuperMajority(int minVersion, const CBlockIndex* pstart, unsigned int nRequired, unsigned int nToCheck)
{
    unsigned int nFound = 0;
    for (unsigned int i = 0; i < nToCheck && nFound < nRequired && pstart != NULL; i++)
    {
        if (pstart->nVersion >= minVersion)
            ++nFound;
        pstart = pstart->pprev;
    }
    return (nFound >= nRequired);
}

bool CheckTransaction(const CTransaction& tx, CValidationState &state)
{
    // Basic checks that don't depend on any context
    if (tx.vin.empty())
        return state.DoS(10, error("CheckTransaction() : vin empty"));
    if (tx.vout.empty())
        return state.DoS(10, error("CheckTransaction() : vout empty"));
    // Size limits
    if (::GetSerializeSize(tx, SER_NETWORK, PROTOCOL_VERSION) > MAX_BLOCK_SIZE)
        return state.DoS(100, error("CTransaction::CheckTransaction() : size limits failed"));

    // Check for negative or overflow output values
    int64 nValueOut = 0;
    BOOST_FOREACH(const CTxOut& txout, tx.vout)
    {
        if (txout.nValue < 0)
            return state.DoS(100, error("CheckTransaction() : txout.nValue negative"));
        if (txout.nValue > MAX_MONEY)
            return state.DoS(100, error("CheckTransaction() : txout.nValue too high"));
        nValueOut += txout.nValue;
        if (!MoneyRange(nValueOut))
            return state.DoS(100, error("CTransaction::CheckTransaction() : txout total out of range"));
    }

    // Check for duplicate inputs
    set<COutPoint> vInOutPoints;
    BOOST_FOREACH(const CTxIn& txin, tx.vin)
    {
        if (vInOutPoints.count(txin.prevout))
            return state.DoS(100, error("CTransaction::CheckTransaction() : duplicate inputs"));
        vInOutPoints.insert(txin.prevout);
    }

    if (tx.IsCoinBase())
    {
        if (tx.vin[0].scriptSig.size() < 2 || tx.vin[0].scriptSig.size() > 100)
            return state.DoS(100, error("CheckTransaction() : coinbase script size"));
    }
    else
    {
        BOOST_FOREACH(const CTxIn& txin, tx.vin)
            if (txin.prevout.IsNull())
                return state.DoS(10, error("CheckTransaction() : prevout is null"));
    }

    return true;
}

bool CBlock::AcceptBlock(CValidationState &state, CDiskBlockPos *dbp)
{
    // Check for duplicate
    uint256 hash = GetHash();
    if (mapBlockIndex.count(hash))
        return state.Invalid(error("AcceptBlock() : block already in mapBlockIndex"));

    // Get prev block index
    CBlockIndex* pindexPrev = NULL;
    int nHeight = 0;
    if (hash != hashGenesisBlock) {
        map<uint256, CBlockIndex*>::iterator mi = mapBlockIndex.find(hashPrevBlock);
        if (mi == mapBlockIndex.end())
            return state.DoS(10, error("AcceptBlock() : prev block not found"));
        pindexPrev = (*mi).second;
        nHeight = pindexPrev->nHeight+1;


        // Check timestamp against prev
        if (GetBlockTime() <= pindexPrev->GetMedianTimePast())
            return state.Invalid(error("AcceptBlock() : block's timestamp is too early"));

        // Check that all transactions are finalized
        BOOST_FOREACH(const CTransaction& tx, vtx)
            if (!IsFinalTx(tx, nHeight, GetBlockTime()))
                return state.DoS(10, error("AcceptBlock() : contains a non-final transaction"));

        // Check that the block chain matches the known block chain up to a checkpoint
        if (!Checkpoints::CheckBlock(nHeight, hash))
            return state.DoS(100, error("AcceptBlock() : rejected by checkpoint lock-in at %d", nHeight));

        // Reject block.nVersion=1 blocks when 95% (75% on testnet) of the network has upgraded:
        if (nVersion < 2)
        {
            if ((!fTestNet && CBlockIndex::IsSuperMajority(2, pindexPrev, 950, 1000)) ||
                (fTestNet && CBlockIndex::IsSuperMajority(2, pindexPrev, 75, 100)))
            {
                return state.Invalid(error("AcceptBlock() : rejected nVersion=1 block"));
            }
        }
        // Enforce block.nVersion=2 rule that the coinbase starts with serialized block height
        if (nVersion >= 2)
        {
            // if 750 of the last 1,000 blocks are version 2 or greater (51/100 if testnet):
            if ((!fTestNet && CBlockIndex::IsSuperMajority(2, pindexPrev, 750, 1000)) ||
                (fTestNet && CBlockIndex::IsSuperMajority(2, pindexPrev, 51, 100)))
            {
                CScript expect = CScript() << nHeight;
                if (!std::equal(expect.begin(), expect.end(), vtx[0].vin[0].scriptSig.begin()))
                    return state.DoS(100, error("AcceptBlock() : block height mismatch in coinbase"));
            }
        }
    }

    // Write block to history file
    try {
        unsigned int nBlockSize = ::GetSerializeSize(*this, SER_DISK, CLIENT_VERSION);
        CDiskBlockPos blockPos;
        if (dbp != NULL)
            blockPos = *dbp;
        if (dbp == NULL)
            if (!WriteToDisk(blockPos))
                return state.Abort(_("Failed to write block"));
    } catch(std::runtime_error &e) {
        return state.Abort(_("System error: ") + e.what());
    }

    // Relay inventory, but don't relay old inventory during initial block download
    int nBlockEstimate = Checkpoints::GetTotalBlocksEstimate();
    if (hashBestChain == hash)
    {
        LOCK(cs_vNodes);
        BOOST_FOREACH(CNode* pnode, vNodes)
            if (nBestHeight > (pnode->nStartingHeight != -1 ? pnode->nStartingHeight - 2000 : nBlockEstimate))
                pnode->PushInventory(CInv(MSG_BLOCK, hash));
    }

    return true;
}

bool CheckProofOfWork(uint256 hash, unsigned int nBits)
{
    CBigNum bnTarget;
    bnTarget.SetCompact(nBits);

    // Check range
    if (bnTarget <= 0 || bnTarget > bnProofOfWorkLimit)
        return error("CheckProofOfWork() : nBits below minimum work");

    // Check proof of work matches claimed amount
    if (hash > bnTarget.getuint256())
        return error("CheckProofOfWork() : hash doesn't match nBits");

    return true;
}

unsigned int GetLegacySigOpCount(const CTransaction& tx)
{
    unsigned int nSigOps = 0;
    BOOST_FOREACH(const CTxIn& txin, tx.vin)
    {
        nSigOps += txin.scriptSig.GetSigOpCount(false);
    }
    BOOST_FOREACH(const CTxOut& txout, tx.vout)
    {
        nSigOps += txout.scriptPubKey.GetSigOpCount(false);
    }
    return nSigOps;
}

bool CBlock::CheckBlock(CValidationState &state, bool fCheckPOW, bool fCheckMerkleRoot) const
{
    // These are checks that are independent of context
    // that can be verified before saving an orphan block.

    // Size limits
    if (vtx.empty() || vtx.size() > MAX_BLOCK_SIZE || ::GetSerializeSize(*this, SER_NETWORK, PROTOCOL_VERSION) > MAX_BLOCK_SIZE)
        return state.DoS(100, error("CheckBlock() : size limits failed"));

    // Check proof of work matches claimed amount
    if (fCheckPOW && !CheckProofOfWork(GetHash(), nBits))
        return state.DoS(50, error("CheckBlock() : proof of work failed"));

    // Check timestamp
    if (GetBlockTime() > GetAdjustedTime() + 2 * 60 * 60)
        return state.Invalid(error("CheckBlock() : block timestamp too far in the future"));

    // First transaction must be coinbase, the rest must not be
    if (vtx.empty() || !vtx[0].IsCoinBase())
        return state.DoS(100, error("CheckBlock() : first tx is not coinbase"));
    for (unsigned int i = 1; i < vtx.size(); i++)
        if (vtx[i].IsCoinBase())
            return state.DoS(100, error("CheckBlock() : more than one coinbase"));

    // Check transactions
    BOOST_FOREACH(const CTransaction& tx, vtx)
        if (!CheckTransaction(tx, state))
            return error("CheckBlock() : CheckTransaction failed");

    // Build the merkle tree already. We need it anyway later, and it makes the
    // block cache the transaction hashes, which means they don't need to be
    // recalculated many times during this block's validation.
    BuildMerkleTree();

    // Check for duplicate txids. This is caught by ConnectInputs(),
    // but catching it earlier avoids a potential DoS attack:
    set<uint256> uniqueTx;
    for (unsigned int i=0; i<vtx.size(); i++) {
        uniqueTx.insert(GetTxHash(i));
    }
    if (uniqueTx.size() != vtx.size())
        return state.DoS(100, error("CheckBlock() : duplicate transaction"));

    unsigned int nSigOps = 0;
    BOOST_FOREACH(const CTransaction& tx, vtx)
    {
        nSigOps += GetLegacySigOpCount(tx);
    }
    if (nSigOps > MAX_BLOCK_SIGOPS)
        return state.DoS(100, error("CheckBlock() : out-of-bounds SigOpCount"));

    // Check merkle root
    if (fCheckMerkleRoot && hashMerkleRoot != BuildMerkleTree())
        return state.DoS(100, error("CheckBlock() : hashMerkleRoot mismatch"));

    return true;
}

bool static AlreadyHave(const CInv& inv)
{
    switch (inv.type)
    {
    case MSG_TX:
        {
        	/* here  is the point  where  I need look  for  my  transaction */
            return true;
        }
    case MSG_BLOCK:
        return mapBlockIndex.count(inv.hash) ||
               mapOrphanBlocks.count(inv.hash);
    }
    // Don't know what it is, just say we already got one
    return true;
}

// requires LOCK(cs_vRecvMsg)
bool ProcessMessages(CNode* pfrom)
{
	//if (fDebug)
	//    printf("ProcessMessages(%zu messages)\n", pfrom->vRecvMsg.size());

	//
	// Message format
	//  (4) message start
	//  (12) command
	//  (4) size
	//  (4) checksum
	//  (x) data
	//
	bool fOk = true;

	if (!pfrom->vRecvGetData.empty())
		ProcessGetData(pfrom);

	std::deque<CNetMessage>::iterator it = pfrom->vRecvMsg.begin();
	while (!pfrom->fDisconnect && it != pfrom->vRecvMsg.end()) {
		// Don't bother if send buffer is too full to respond anyway
		if (pfrom->nSendSize >= SendBufferSize())
			break;

		// get next message
		CNetMessage& msg = *it;

		//if (fDebug)
		//    printf("ProcessMessages(message %u msgsz, %zu bytes, complete:%s)\n",
		//            msg.hdr.nMessageSize, msg.vRecv.size(),
		//            msg.complete() ? "Y" : "N");

		// end, if an incomplete message is found
		if (!msg.complete())
			break;

		// at this point, any failure means we can delete the current message
		it++;

		// Scan for message start
		if (memcmp(msg.hdr.pchMessageStart, pchMessageStart, sizeof(pchMessageStart)) != 0) {
			printf("\n\nPROCESSMESSAGE: INVALID MESSAGESTART\n\n");
			fOk = false;
			break;
		}

		// Read header
		CMessageHeader& hdr = msg.hdr;
		if (!hdr.IsValid())
		{
			printf("\n\nPROCESSMESSAGE: ERRORS IN HEADER %s\n\n\n", hdr.GetCommand().c_str());
			continue;
		}
		string strCommand = hdr.GetCommand();

		// Message size
		unsigned int nMessageSize = hdr.nMessageSize;

		// Checksum
		CDataStream& vRecv = msg.vRecv;
		uint256 hash = Hash(vRecv.begin(), vRecv.begin() + nMessageSize);
		unsigned int nChecksum = 0;
		memcpy(&nChecksum, &hash, sizeof(nChecksum));
		if (nChecksum != hdr.nChecksum)
		{
			printf("ProcessMessages(%s, %u bytes) : CHECKSUM ERROR nChecksum=%08x hdr.nChecksum=%08x\n",
				strCommand.c_str(), nMessageSize, nChecksum, hdr.nChecksum);
			continue;
		}

		// Process message
		bool fRet = false;
		try
		{
			{
				LOCK(cs_main);
				fRet = ProcessMessage(pfrom, strCommand, vRecv);
			}
			boost::this_thread::interruption_point();
		}
		catch (std::ios_base::failure& e)
		{
			if (strstr(e.what(), "end of data"))
			{
				// Allow exceptions from under-length message on vRecv
				printf("ProcessMessages(%s, %u bytes) : Exception '%s' caught, normally caused by a message being shorter than its stated length\n", strCommand.c_str(), nMessageSize, e.what());
			}
			else if (strstr(e.what(), "size too large"))
			{
				// Allow exceptions from over-long size
				printf("ProcessMessages(%s, %u bytes) : Exception '%s' caught\n", strCommand.c_str(), nMessageSize, e.what());
			}
			else
			{
				PrintExceptionContinue(&e, "ProcessMessages()");
			}
		}
		catch (boost::thread_interrupted) {
			throw;
		}
		catch (std::exception& e) {
			PrintExceptionContinue(&e, "ProcessMessages()");
		} catch (...) {
			PrintExceptionContinue(NULL, "ProcessMessages()");
		}

		if (!fRet)
			printf("ProcessMessage(%s, %u bytes) FAILED\n", strCommand.c_str(), nMessageSize);
	}

	// In case the connection got shut down, its receive buffer was wiped
	if (!pfrom->fDisconnect)
		pfrom->vRecvMsg.erase(pfrom->vRecvMsg.begin(), it);

	return fOk;
}


void PushGetBlocks(CNode* pnode, CBlockIndex* pindexBegin, uint256 hashEnd)
{
    // Filter out duplicate requests
    if (pindexBegin == pnode->pindexLastGetBlocksBegin && hashEnd == pnode->hashLastGetBlocksEnd)
        return;
    pnode->pindexLastGetBlocksBegin = pindexBegin;
    pnode->hashLastGetBlocksEnd = hashEnd;

    pnode->PushMessage("getblocks", CBlockLocator(pindexBegin), hashEnd);
}


bool SendMessages(CNode* pto, bool fSendTrickle)
{
	TRY_LOCK(cs_main, lockMain);
	if (lockMain) {
		// Don't send anything until we get their version message
		if (pto->nVersion == 0)
			return true;

		// Keep-alive ping. We send a nonce of zero because we don't use it anywhere
		// right now.
		if (pto->nLastSend && GetTime() - pto->nLastSend > 30 * 60 && pto->vSendMsg.empty()) {
			uint64 nonce = 0;
			if (pto->nVersion > BIP0031_VERSION)
				pto->PushMessage("ping", nonce);
			else
				pto->PushMessage("ping");
		}

		// Start block sync
		if (pto->fStartSync && !fImporting && !fReindex) {
			pto->fStartSync = false;
			PushGetBlocks(pto, pindexBest, uint256(0));
		}

		// Address refresh broadcast
		static int64 nLastRebroadcast;
		if (!IsInitialBlockDownload() && (GetTime() - nLastRebroadcast > 24 * 60 * 60))
		{
			{
				LOCK(cs_vNodes);
				BOOST_FOREACH(CNode* pnode, vNodes)
				{
					// Periodically clear setAddrKnown to allow refresh broadcasts
					if (nLastRebroadcast)
						pnode->setAddrKnown.clear();

					// Rebroadcast our address
					if (!fNoListen)
					{
						CAddress addr = GetLocalAddress(&pnode->addr);
						if (addr.IsRoutable())
							pnode->PushAddress(addr);
					}
				}
			}
			nLastRebroadcast = GetTime();
		}
		//
		// Message: getdata
		//
		vector<CInv> vGetData;
		int64 nNow = GetTime() * 1000000;
		while (!pto->mapAskFor.empty() && (*pto->mapAskFor.begin()).first <= nNow)
		{
			const CInv& inv = (*pto->mapAskFor.begin()).second;
			if (!AlreadyHave(inv))
			{
				if (fDebugNet)
					printf("sending getdata: %s\n", inv.ToString().c_str());
				vGetData.push_back(inv);
				if (vGetData.size() >= 1000)
				{
					pto->PushMessage("getdata", vGetData);
					vGetData.clear();
				}
			}
			pto->mapAskFor.erase(pto->mapAskFor.begin());
		}
		if (!vGetData.empty())
			pto->PushMessage("getdata", vGetData);

	}
	return true;
}


unsigned int ComputeMinWork(unsigned int nBase, int64 nTime)
{
    // Testnet has min-difficulty blocks
    // after nTargetSpacing*2 time between blocks:
    if (fTestNet && nTime > nTargetSpacing*2)
        return bnProofOfWorkLimit.GetCompact();

    CBigNum bnResult;
    bnResult.SetCompact(nBase);
    while (nTime > 0 && bnResult < bnProofOfWorkLimit)
    {
        // Maximum 400% adjustment...
        bnResult *= 4;
        // ... in best-case exactly 4-times-normal target time
        nTime -= nTargetTimespan*4;
    }
    if (bnResult > bnProofOfWorkLimit)
        bnResult = bnProofOfWorkLimit;
    return bnResult.GetCompact();
}

bool ProcessBlock(CValidationState &state, CNode* pfrom, CBlock* pblock, CDiskBlockPos *dbp)
{
    // Check for duplicate
    uint256 hash = pblock->GetHash();
    if (mapBlockIndex.count(hash))
        return state.Invalid(error("ProcessBlock() : already have block %d %s", mapBlockIndex[hash]->nHeight, hash.ToString().c_str()));
    if (mapOrphanBlocks.count(hash))
        return state.Invalid(error("ProcessBlock() : already have block (orphan) %s", hash.ToString().c_str()));

    // Preliminary checks
    if (!pblock->CheckBlock(state))
        return error("ProcessBlock() : CheckBlock FAILED");

    CBlockIndex* pcheckpoint = Checkpoints::GetLastCheckpoint(mapBlockIndex);
    if (pcheckpoint && pblock->hashPrevBlock != hashBestChain)
    {
        // Extra checks to prevent "fill up memory by spamming with bogus blocks"
        int64 deltaTime = pblock->GetBlockTime() - pcheckpoint->nTime;
        if (deltaTime < 0)
        {
            return state.DoS(100, error("ProcessBlock() : block with timestamp before last checkpoint"));
        }
        CBigNum bnNewBlock;
        bnNewBlock.SetCompact(pblock->nBits);
        CBigNum bnRequired;
        bnRequired.SetCompact(ComputeMinWork(pcheckpoint->nBits, deltaTime));
        if (bnNewBlock > bnRequired)
        {
            return state.DoS(100, error("ProcessBlock() : block with too little proof-of-work"));
        }
    }


    // If we don't already have its previous block, shunt it off to holding area until we get it
    if (pblock->hashPrevBlock != 0 && !mapBlockIndex.count(pblock->hashPrevBlock))
    {
        printf("ProcessBlock: ORPHAN BLOCK, prev=%s\n", pblock->hashPrevBlock.ToString().c_str());

        // Accept orphans as long as there is a node to request its parents from
        if (pfrom) {
            CBlock* pblock2 = new CBlock(*pblock);
            mapOrphanBlocks.insert(make_pair(hash, pblock2));
            mapOrphanBlocksByPrev.insert(make_pair(pblock2->hashPrevBlock, pblock2));

            // Ask this guy to fill in what we're missing
            PushGetBlocks(pfrom, pindexBest, GetOrphanRoot(pblock2));
        }
        return true;
    }

    // Store to disk
    if (!pblock->AcceptBlock(state, dbp))
        return error("ProcessBlock() : AcceptBlock FAILED");

    // Recursively process any orphan blocks that depended on this one
    vector<uint256> vWorkQueue;
    vWorkQueue.push_back(hash);
    for (unsigned int i = 0; i < vWorkQueue.size(); i++)
    {
        uint256 hashPrev = vWorkQueue[i];
        for (multimap<uint256, CBlock*>::iterator mi = mapOrphanBlocksByPrev.lower_bound(hashPrev);
             mi != mapOrphanBlocksByPrev.upper_bound(hashPrev);
             ++mi)
        {
            CBlock* pblockOrphan = (*mi).second;
            // Use a dummy CValidationState so someone can't setup nodes to counter-DoS based on orphan resolution (that is, feeding people an invalid block based on LegitBlockX in order to get anyone relaying LegitBlockX banned)
            CValidationState stateDummy;
            if (pblockOrphan->AcceptBlock(stateDummy))
                vWorkQueue.push_back(pblockOrphan->GetHash());
            mapOrphanBlocks.erase(pblockOrphan->GetHash());
            delete pblockOrphan;
        }
        mapOrphanBlocksByPrev.erase(hashPrev);
    }

    printf("ProcessBlock: ACCEPTED\n");
    return true;
}


bool static ProcessMessage(CNode* pfrom, string strCommand, CDataStream& vRecv)
{
	RandAddSeedPerfmon();
	if (fDebug)
		printf("received: %s (%"PRIszu" bytes)\n", strCommand.c_str(), vRecv.size());
	if (mapArgs.count("-dropmessagestest") && GetRand(atoi(mapArgs["-dropmessagestest"])) == 0)
	{
		printf("dropmessagestest DROPPING RECV MESSAGE\n");
		return true;
	}


	if (strCommand == "version")
	{
		// Each connection can only send one version message
		if (pfrom->nVersion != 0)
		{
			pfrom->Misbehaving(1);
			return false;
		}

		int64 nTime;
		CAddress addrMe;
		CAddress addrFrom;
		uint64 nNonce = 1;
		vRecv >> pfrom->nVersion >> pfrom->nServices >> nTime >> addrMe;
		if (pfrom->nVersion < MIN_PROTO_VERSION)
		{
			// Since February 20, 2012, the protocol is initiated at version 209,
			// and earlier versions are no longer supported
			printf("partner %s using obsolete version %i; disconnecting\n", pfrom->addr.ToString().c_str(), pfrom->nVersion);
			pfrom->fDisconnect = true;
			return false;
		}

		if (pfrom->nVersion == 10300)
			pfrom->nVersion = 300;
		if (!vRecv.empty())
			vRecv >> addrFrom >> nNonce;
		if (!vRecv.empty())
			vRecv >> pfrom->strSubVer;
		if (!vRecv.empty())
			vRecv >> pfrom->nStartingHeight;
		if (!vRecv.empty())
			vRecv >> pfrom->fRelayTxes; // set to true after we get the first filter* message
		else
			pfrom->fRelayTxes = true;

		if (pfrom->fInbound && addrMe.IsRoutable())
		{
			pfrom->addrLocal = addrMe;
			SeenLocal(addrMe);
		}

		// Disconnect if we connected to ourself
		if (nNonce == nLocalHostNonce && nNonce > 1)
		{
			printf("connected to self at %s, disconnecting\n", pfrom->addr.ToString().c_str());
			pfrom->fDisconnect = true;
			return true;
		}

		// Be shy and don't send version until we hear
		if (pfrom->fInbound)
			pfrom->PushVersion();

		pfrom->fClient = !(pfrom->nServices & NODE_NETWORK);

		AddTimeData(pfrom->addr, nTime);

		// Change version
		pfrom->PushMessage("verack");
		pfrom->ssSend.SetVersion(min(pfrom->nVersion, PROTOCOL_VERSION));

/*
reconsider  what  happen her  this  is  advertisment  part
*/

		if (!pfrom->fInbound)
		{
			// Advertise our address
			if (!fNoListen && !IsInitialBlockDownload())
			{
				CAddress addr = GetLocalAddress(&pfrom->addr);
				if (addr.IsRoutable())
					pfrom->PushAddress(addr);
			}

			// Get recent addresses
			if (pfrom->fOneShot || pfrom->nVersion >= CADDR_TIME_VERSION || addrman.size() < 1000)
			{
				pfrom->PushMessage("getaddr");
				pfrom->fGetAddr = true;
			}
			addrman.Good(pfrom->addr);
		} else {
			if (((CNetAddr)pfrom->addr) == (CNetAddr)addrFrom)
			{
				addrman.Add(addrFrom, addrFrom);
				addrman.Good(addrFrom);
			}
		}

		// Relay alerts
		{
			LOCK(cs_mapAlerts);
			BOOST_FOREACH(PAIRTYPE(const uint256, CAlert)& item, mapAlerts)
				item.second.RelayTo(pfrom);
		}

		pfrom->fSuccessfullyConnected = true;

		printf("receive version message: version %d, blocks=%d, us=%s, them=%s, peer=%s\n", pfrom->nVersion, pfrom->nStartingHeight, addrMe.ToString().c_str(), addrFrom.ToString().c_str(), pfrom->addr.ToString().c_str());

		cPeerBlockCounts.input(pfrom->nStartingHeight);
	}


	else if (pfrom->nVersion == 0)
	{
		// Must have a version message before anything else
		pfrom->Misbehaving(1);
		return false;
	}


	else if (strCommand == "verack")
	{
		pfrom->SetRecvVersion(min(pfrom->nVersion, PROTOCOL_VERSION));
	}


	else if (strCommand == "addr")
	{
		vector<CAddress> vAddr;
		vRecv >> vAddr;

		// Don't want addr from older versions unless seeding
		if (pfrom->nVersion < CADDR_TIME_VERSION && addrman.size() > 1000)
			return true;
		if (vAddr.size() > 1000)
		{
			pfrom->Misbehaving(20);
			return error("message addr size() = %"PRIszu"", vAddr.size());
		}

		// Store the new addresses
		vector<CAddress> vAddrOk;
		int64 nNow = GetAdjustedTime();
		int64 nSince = nNow - 10 * 60;
		BOOST_FOREACH(CAddress& addr, vAddr)
		{
			boost::this_thread::interruption_point();

			if (addr.nTime <= 100000000 || addr.nTime > nNow + 10 * 60)
				addr.nTime = nNow - 5 * 24 * 60 * 60;
			pfrom->AddAddressKnown(addr);
			bool fReachable = IsReachable(addr);
			if (addr.nTime > nSince && !pfrom->fGetAddr && vAddr.size() <= 10 && addr.IsRoutable())
			{
				// Relay to a limited number of other nodes
				{
					LOCK(cs_vNodes);
					// Use deterministic randomness to send to the same nodes for 24 hours
					// at a time so the setAddrKnowns of the chosen nodes prevent repeats
					static uint256 hashSalt;
					if (hashSalt == 0)
						hashSalt = GetRandHash();
					uint64 hashAddr = addr.GetHash();
					uint256 hashRand = hashSalt ^ (hashAddr<<32) ^ ((GetTime()+hashAddr)/(24*60*60));
					hashRand = Hash(BEGIN(hashRand), END(hashRand));
					multimap<uint256, CNode*> mapMix;
					BOOST_FOREACH(CNode* pnode, vNodes)
					{
						if (pnode->nVersion < CADDR_TIME_VERSION)
							continue;
						unsigned int nPointer;
						memcpy(&nPointer, &pnode, sizeof(nPointer));
						uint256 hashKey = hashRand ^ nPointer;
						hashKey = Hash(BEGIN(hashKey), END(hashKey));
						mapMix.insert(make_pair(hashKey, pnode));
					}
					int nRelayNodes = fReachable ? 2 : 1; // limited relaying of addresses outside our network(s)
					for (multimap<uint256, CNode*>::iterator mi = mapMix.begin(); mi != mapMix.end() && nRelayNodes-- > 0; ++mi)
						((*mi).second)->PushAddress(addr);
				}
			}
			// Do not store addresses outside our network
			if (fReachable)
				vAddrOk.push_back(addr);
		}
		addrman.Add(vAddrOk, pfrom->addr, 2 * 60 * 60);
		if (vAddr.size() < 1000)
			pfrom->fGetAddr = false;
		if (pfrom->fOneShot)
			pfrom->fDisconnect = true;
	}


	else if (strCommand == "inv")
	{
		vector<CInv> vInv;
		vRecv >> vInv;
		if (vInv.size() > MAX_INV_SZ)
		{
			pfrom->Misbehaving(20);
			return error("message inv size() = %"PRIszu"", vInv.size());
		}

		// find last block in inv vector
		unsigned int nLastBlock = (unsigned int)(-1);
		for (unsigned int nInv = 0; nInv < vInv.size(); nInv++) {
			if (vInv[vInv.size() - 1 - nInv].type == MSG_BLOCK) {
				nLastBlock = vInv.size() - 1 - nInv;
				break;
			}
		}
		for (unsigned int nInv = 0; nInv < vInv.size(); nInv++)
		{
			const CInv &inv = vInv[nInv];

			boost::this_thread::interruption_point();
			pfrom->AddInventoryKnown(inv);

			bool fAlreadyHave = AlreadyHave(inv);
			if (fDebug)
				printf("  got inventory: %s  %s\n", inv.ToString().c_str(), fAlreadyHave ? "have" : "new");

			if (!fAlreadyHave) {
				if (!fImporting && !fReindex)
					pfrom->AskFor(inv);
			} else if (inv.type == MSG_BLOCK && mapOrphanBlocks.count(inv.hash)) {
				PushGetBlocks(pfrom, pindexBest, GetOrphanRoot(mapOrphanBlocks[inv.hash]));
			} else if (nInv == nLastBlock) {
				// In case we are on a very long side-chain, it is possible that we already have
				// the last block in an inv bundle sent in response to getblocks. Try to detect
				// this situation and push another getblocks to continue.
				PushGetBlocks(pfrom, mapBlockIndex[inv.hash], uint256(0));
				if (fDebug)
					printf("force request: %s\n", inv.ToString().c_str());
			}
		}
	}


	else if (strCommand == "getdata")
	{
		vector<CInv> vInv;
		vRecv >> vInv;
		if (vInv.size() > MAX_INV_SZ)
		{
			pfrom->Misbehaving(20);
			return error("message getdata size() = %"PRIszu"", vInv.size());
		}

		if (fDebugNet || (vInv.size() != 1))
			printf("received getdata (%"PRIszu" invsz)\n", vInv.size());

		if ((fDebugNet && vInv.size() > 0) || (vInv.size() == 1))
			printf("received getdata for: %s\n", vInv[0].ToString().c_str());

		pfrom->vRecvGetData.insert(pfrom->vRecvGetData.end(), vInv.begin(), vInv.end());
		ProcessGetData(pfrom);
	}


	else if (strCommand == "getblocks")
	{
	}


	else if (strCommand == "getheaders")
	{

	}


	else if (strCommand == "tx")
	{
		vector<uint256> vWorkQueue;
		vector<uint256> vEraseQueue;
		CDataStream vMsg(vRecv);
		CTransaction tx;
		vRecv >> tx;

		CInv inv(MSG_TX, tx.GetHash());
		pfrom->AddInventoryKnown(inv);

		if ( mapAlreadyAskedFor.find(inv) != mapAlreadyAskedFor.end() )
		{
			mapAlreadyAskedFor.erase(inv);

			// add inventory transfer them  to  needed  place
		}

	}


	else if (strCommand == "block" && !fImporting && !fReindex) // Ignore blocks received while importing
	{
		CBlock block;
		vRecv >> block;

		printf("received block %s\n", block.GetHash().ToString().c_str());
		// block.print();

		CInv inv(MSG_BLOCK, block.GetHash());
		pfrom->AddInventoryKnown(inv);

		CValidationState state;
		if (ProcessBlock(state, pfrom, &block))
			mapAlreadyAskedFor.erase(inv);
		int nDoS;
		if (state.IsInvalid(nDoS))
			pfrom->Misbehaving(nDoS);
	}


	else if (strCommand == "getaddr")
	{
		pfrom->vAddrToSend.clear();
		vector<CAddress> vAddr = addrman.GetAddr();
		BOOST_FOREACH(const CAddress &addr, vAddr)
			pfrom->PushAddress(addr);
	}


	else if (strCommand == "mempool")
	{

	}


	else if (strCommand == "ping")
	{
		if (pfrom->nVersion > BIP0031_VERSION)
		{
			uint64 nonce = 0;
			vRecv >> nonce;
			// Echo the message back with the nonce. This allows for two useful features:
			//
			// 1) A remote node can quickly check if the connection is operational
			// 2) Remote nodes can measure the latency of the network thread. If this node
			//    is overloaded it won't respond to pings quickly and the remote node can
			//    avoid sending us more work, like chain download requests.
			//
			// The nonce stops the remote getting confused between different pings: without
			// it, if the remote node sends a ping once per second and this node takes 5
			// seconds to respond to each, the 5th ping the remote sends would appear to
			// return very quickly.
			pfrom->PushMessage("pong", nonce);
		}
	}


	else if (strCommand == "alert")
	{
		CAlert alert;
		vRecv >> alert;

		uint256 alertHash = alert.GetHash();
		if (pfrom->setKnown.count(alertHash) == 0)
		{
			if (alert.ProcessAlert())
			{
				// Relay
				pfrom->setKnown.insert(alertHash);
				{
					LOCK(cs_vNodes);
					BOOST_FOREACH(CNode* pnode, vNodes)
						alert.RelayTo(pnode);
				}
			}
			else {
				// Small DoS penalty so peers that send us lots of
				// duplicate/expired/invalid-signature/whatever alerts
				// eventually get banned.
				// This isn't a Misbehaving(100) (immediate ban) because the
				// peer might be an older or different implementation with
				// a different signature key, etc.
				pfrom->Misbehaving(10);
			}
		}
	}


	else if (strCommand == "filterload")
	{
		CBloomFilter filter;
		vRecv >> filter;

		if (!filter.IsWithinSizeConstraints())
			// There is no excuse for sending a too-large filter
			pfrom->Misbehaving(100);
		else
		{
			LOCK(pfrom->cs_filter);
			delete pfrom->pfilter;
			pfrom->pfilter = new CBloomFilter(filter);
		}
		pfrom->fRelayTxes = true;
	}


	else if (strCommand == "filteradd")
	{
		vector<unsigned char> vData;
		vRecv >> vData;

		// Nodes must NEVER send a data item > 520 bytes (the max size for a script data object,
		// and thus, the maximum size any matched object can have) in a filteradd message
		if (vData.size() > MAX_SCRIPT_ELEMENT_SIZE)
		{
			pfrom->Misbehaving(100);
		} else {
			LOCK(pfrom->cs_filter);
			if (pfrom->pfilter)
				pfrom->pfilter->insert(vData);
			else
				pfrom->Misbehaving(100);
		}
	}


	else if (strCommand == "filterclear")
	{
		LOCK(pfrom->cs_filter);
		delete pfrom->pfilter;
		pfrom->pfilter = NULL;
		pfrom->fRelayTxes = true;
	}


	else
	{
		// Ignore unknown commands for extensibility
	}


	// Update the last seen time for this node's address
	if (pfrom->fNetworkNode)
		if (strCommand == "version" || strCommand == "addr" || strCommand == "inv" || strCommand == "getdata" || strCommand == "ping")
			AddressCurrentlyConnected(pfrom->addr);

}
