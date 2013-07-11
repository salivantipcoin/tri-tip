
#ifndef TT_NETWORK_H
#define TT_NETWORK_H

#include <iostream>
#include <fstream>

boost::filesystem::path const  basePath = GetDataDir() / ".lock";

namespace  TTcoin
{
class BitcoinNetworkGate
{
public:
void addBlock( CBlock const & _block );

void searchForBitcoinTransactions();
private:

	boost::mutex m_blockListMutex;
	CBlockIndex* m_orginIndex;
	CTTEnterPointForTransaction m_enterPointForTransaction;

	std::list< CBlock > m_blocks;
};

void
BitcoinNetworkGate::addBlock( CBlock const & _block )
{
	boost::mutex::scoped_lock lock(m_blockListMutex);
	m_blocks.push_back( _block );
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
BitcoinNetworkGate::searchForBitcoinTransactions()
{
	int ret = 0;
	{
		LOCK(cs_wallet);
		while (m_orginIndex)
		{

			m_orginIndex = m_orginIndex->GetNextInMainChain();

			CBlock block;
			block.ReadFromDisk( m_orginIndex );

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

// merkle  tree

}
// state machine  to  handle  traffic

#endif
