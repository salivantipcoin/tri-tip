#ifndef TT_ENTER_POINT_FOR_TRANSACTION_H
#define TT_ENTER_POINT_FOR_TRANSACTION_H

namespace TTcoin 
{

typedef CTransaction CTTtransaction;

class CTTEnterPointForTransaction
{
public:
	boost::optional< CTransaction > scanOutputOfTransaction( CTransaction const & txIn);
	void foundTTtransactions(CTransaction const & txIn);
private:
	CScript const m_target;
	std::multimap< unsigned, CTTtransaction > m_ttTransactions;
	unsigned m_currentBlockTime;
};

CTTEnterPointForTransaction::CTTEnterPointForTransaction()
	: m_target( CScript() )
{
}

boost::optional< CTransaction >
CTTEnterPointForTransaction::scanOutputOfTransaction(CTransaction const & txIn)
{ 
	int64 value;

	BOOST_FOREACH( const CTxOut& txout, txIn.vout )
	{
		if ( txout.scriptPubKey == m_target )
			return boost::optional< CTransaction >( txIn );
	}

	return boost::optional< CTransaction >();
}

void
CTTEnterPointForTransaction::foundTTtransactions(CTransaction const & txIn)
{
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
		if ( m_ttTransactions.find( txin.prevout.hash ) == m_ttTransactions.end() )
			return;
		const CTransaction prevTransact = getTransact( txin.prevout.hash );
		scripts.push_back( prevTransact.vout[ txin.prevout.n ].scriptPubKey );
	}

	CTTtransaction primaryTransaction;

	primaryTransaction.vin.push_back( CTxIn( COutPoint() ) );

	CTxOut out;

	out.nValue = value;
	out.scriptPubKey = scripts.begin();

	primaryTransaction.vout.push_back( out );

	m_ttTransactions.push_back( std::make_pair( m_currentBlockTime, primaryTransaction ) );
}

}

#endif