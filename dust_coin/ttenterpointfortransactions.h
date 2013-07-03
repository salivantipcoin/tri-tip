#ifndef TT_ENTER_POINT_FOR_TRANSACTION_H
#define TT_ENTER_POINT_FOR_TRANSACTION_H

namespace TTcoin 
{

class CTTEnterPointForTransaction
{
	const CScript m_target;
	boost::optional< CTransaction > scanInputTransactions(const CTransaction& txIn);
};

boost::optional< CTransaction >
CTTEnterPointForTransaction::scanInputTransactions(const CTransaction& txIn)
{ 
	boost::optional< CTransaction >()

	//register  only  from  one  among  them
	std::list< CScript > scripts;
	int64 value;

	BOOST_FOREACH( const CTxOut& txout, txIn.vout )
	{
		if ( txout.scriptPubKey == m_target )
			value += txout.nValue;
	}

	if ( value == 0 )
		return boost::optional< CTransaction >();

	BOOST_FOREACH( const CTxIn& txin, txIn.vin )
	{
		// get previous transaction 
		// most probably  they  have  to  ask  bitcoin  network  about  given  transaction  it  may be  painfull
		const CTransaction prevTransact = getTransact( txin.prevout.hash );
		scripts.push_back( prevTransact.vout[ txin.prevout.n ].scriptPubKey );
	}
	
	CTransaction primaryTransaction;

	primaryTransaction.vin.push_back( CTxIn( COutPoint() ) );

	CTxOut out;

	out.nValue = value;
	out.scriptPubKey = scripts.begin();

	primaryTransaction.vout.push_back( out );

	return boost::optional< CTransaction >( primaryTransaction );
}


}

#endif