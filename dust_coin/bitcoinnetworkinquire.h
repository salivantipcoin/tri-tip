#ifndef BITCOIN_NETWORK_INQUIRE_H
#define BITCOIN_NETWORK_INQUIRE_H

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

#endif