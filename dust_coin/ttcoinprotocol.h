enum TransactionStatus
{
	  KNOW
	, ACK
	, STORED
	, UNKNOWN
	, INVALID
};

struct CTTTransaction
{
	TransactionStatus m_status;
	CTransaction m_transaction;
};

std::list< CTTTransaction > TTtransactions;
std::set< uint256 ,CTTTransaction > TTbroadcastTransaction;
std::set< uint256 ,CTTTransaction > TTbroadcastedTransactions;
std::set< uint256 ,CTTTransaction > TTAckTransaction;
std::set< uint256 ,CTTTransaction > TTHaveToAckTransaction;
std::set< uint256 ,CTTTransaction > TTToStoreTransactions;

void  
loopOverTransactions()
{

	lookFor( transaction, TTAckTransaction );

	TTHaveToAckTransaction

	if ( transaction.m_status == KNOW )

}


BOOST_FOREACH( std::pair< uint256 ,CTTTransaction > hashedtransaction, TTbroadcastTransaction )
{
	CTTTransaction transaction = hashedtransaction.second;

	fillNodesQueues( transaction );

	TTbroadcastTransaction.erase( hashedtransaction );
	TTbroadcastedTransactions.push( hashedtransaction );
}

analyseContingent()
{
	//  check if  you know  this  transaction

}

INFO -  can't  override  anything'
ACK - can  overide  according  to  resolve  conflict  function 
ACK  cannot overide  INFO ??
INFO  can  fight but  how 

INFO may  not  work  on  certain  nodes  then they  will be  part  of  Network 

node  could  never  KNOW 
ok 
INFO 
INFO - double spending will cancel if  created  from  other  node, if  created  from behind it  will be  droped

IN OPOSITE  DIRECTION  ACK after  that no way to cancel operation at  all





base  on  time 


STORED can't  be  overiden



analyseResponses()
{
	//  check if  you know  this  transaction 
}

resolveConflicts()
{

};


searchOver

TTbroadcastTransaction


updateMyTransaction
{
//  read  time  stamp  
// decide  if   need  to  update 
};
//------------------------------------------ --
here  put int  consideration ,  this  is 
//--------------------------------------------
here  analyse  prove of  work ,  analyse  queues   and  set  prove of  work   condition 