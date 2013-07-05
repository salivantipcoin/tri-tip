#ifndef BITCOIN_NETWORK_INQUIRE_H
#define BITCOIN_NETWORK_INQUIRE_H

map<uint256, CBlockIndex*> mapBlockIndex;

class BitcoinNetworkInquire
{
public:
	void updateBlock();
	void pleaseGiveMeTransactionRequest( const uint256& hashIn );
private:
	CNode* m_node;
	static const std::string m_primaryBlockHash;
	static const CScript m_tipcoinHomeAddress;

	std::string m_mostRecentBlockHash;
};

void
BitcoinNetworkInquire::updateBlock()
{
//first  find  nodes 

// register ack  your  address in  network

// based on  block hash go  and  download all  missing  blocks 

// answer  you  do  not  have  anything in mean  time

//iterate  through  transaction  and incorporate  needed  into  blockchain 

//  create  and  load my  own  block  chain 

// open  ttnetwork  start  asking  about blocks build  history  and  check if  all  valid /* may  store  transaction broadcasted   but  do  nothing  with  them till update */

//  transfer transaction to all pears /* we  need  kind  of  sufficient  protocol */

// some  kind  of  state  machine  which  will  handle  traffic 

// wallets  managment  for now  it  could  be  very simple 

// recovery  mode  find  and  update   what  went  wrong 

/*	//m_node->AskFor( CInv(MSG_TX, m_mostRecentBlockHash ) );
	while(1)
	{
		//ask  about  blocks
	}
*/
}

void
BitcoinNetworkInquire::pleaseGiveMeTransactionRequest( const uint256& hashIn )
{
	m_node->AskFor( CInv(MSG_TX, hashIn ) );
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

	//  enlist  this  block on  the  waiting  map 




	return true;
}


#endif