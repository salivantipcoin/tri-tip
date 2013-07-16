
#ifndef TT_NETWORK_H
#define TT_NETWORK_H

#include <iostream>
#include <fstream>

boost::filesystem::path const  basePath = GetDataDir() / ".lock";

typedef  CBlock  CTTBlock;


namespace  TTcoin
{
class CBitcoinNetworkGate
{
public:
void addBlock( CBlock const & _block );

private:
	void searchForBitcoinTransactions();
	void processBitcoinNetworkBlocks()
private:
	boost::mutex m_blockListMutex;
	CBlockIndex* m_orginIndex;
	CTTEnterPointForTransaction m_enterPointForTransaction;

	std::list< CBlock > m_blocks;

	std::map< uint256, CTransaction > m_reclaimedTansactions;
};

void
CBitcoinNetworkGate::addBlock( CBlock const & _block )
{
	boost::mutex::scoped_lock lock(m_blockListMutex);
	m_blocks.push_back( _block );
}


void 
processBitcoinNetworkBlocks()
{
	while(1)
	{
		{

		boost::mutex::scoped_lock lock(m_blockListMutex);
		BOOST_FOREACH( CBlock const & block, m_blocks )
		{
			block.BuildMerkleTree();
		}

		}
		MilliSleep(1);
	}

}
std::string
getStartBlockHash() const
{
	std::ofstream baseFile( basePath.string().c_str() );
	if( !baseFile.is_open() )
		baseFile.open( basePath.string().c_str() , std::fstream::out);
	
	std::string startHash << baseFile;

	baseFile.close();

	return startHash;
}


CBlockIndex * createBitcoinIndex( std::string _blockHash )
{
	CBlockIndex * blockIndex = new CBlockIndex;

	blockIndex->phashBlock = new uint256( _blockHash );

	return blockIndex;
}

void
setCurrentBlockBitcoinIndex()
{
	pindexBest = createBitcoinIndex( getStartBlockHash() );
}


void
setStartBlockHash( std::string _hash )
{
	std::ofstream baseFile( basePath.string().c_str() );
	if( !baseFile.is_open() )
		baseFile.open( basePath.string().c_str() , std::fstream::in);

	baseFile << _hash;

	baseFile.close();
}

void
CBitcoinNetworkGate::searchForBitcoinTransactions( CBlock & block )
{
	int ret = 0;
	{
		LOCK(cs_wallet);
		while (m_orginIndex)
		{
			std::vector< CTransaction >transactions;

			BOOST_FOREACH(CTransaction& tx, block.vtx)
			{
				boost::optional< CTransaction > transaction = m_enterPointForTransaction.scanInputTransactions( tx );
				if ( transaction )
					transactions.push_back( *transaction );
				// transmit it to CTTNetwork updateTTCoinNetwork( std::vector< CTransaction > & _transactions );
				;
			}
		}
	}
	return ret;
}

class CTTNetwork
{
	CBlockIndex* pindex
// mechanics of  boost  signal
	void updateTTCoinNetwork( std::vector< CTransaction > & _transactions );
// handle  traffic   thread
// loop  to  load  missing  blocks 

// than  when  we  have  block  take  index  and  create CBlock 
	


};



//block  chain of  transactions
/*
thread bitcoin -  comunicate  with  bitcoin network  fetch  transaction
ttnetwork  thread -  comunicate  with  ttnetwork  handle  transaction  old  ways 
thread work -  make  all  prove of  work  stuff 
thread check   for  correctnes  of  transactions and  introduce them to a block
--commnication protocol-- biggest issue
*/
}
// state machine  to  handle  traffic

#endif
