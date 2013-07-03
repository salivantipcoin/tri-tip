
#ifndef TT_NETWORK_H
#define TT_NETWORK_H

namespace  TTcoin
{
class BitcoinNetworkGate
{
public:

void searchForBitcoinTransactions();
private:
CBlockIndex* m_orginIndex;
CTTEnterPointForTransaction m_enterPointForTransaction;
};

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
