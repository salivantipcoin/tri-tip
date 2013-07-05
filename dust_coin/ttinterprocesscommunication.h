/* thread  for  communication with core, may be  it  has to be boost thread */
enum MessageType
{
	 TEST_MESSAGE
	, TRANSACTON_MESSAGE
	, MAINTENANCE_MESSAGE
};

struct Message
{
	MessageType m_messageType;
};

Message message;

QDataStream dataStream << message;



class CTTDialog
{
	int m_index;

	QLocalSocket * m_socket;

	m_socket = new QLocalSocket;

	std::list< Message > m_messageSended;
};


QLocalServer * localServer = new QLocalServer;

localServer->listen ( "ttenvironment" );


readRawData ( char * s, int len )

while( 1 )
{
	if ( localServer->hasPendingConnections() )
/*

nextPendingConnection ()


*/
}
/*
isCongestion();
sendTransaction()
receiveTransaction()
*/


void QLocalSocket::connectToServer ( const QString & name, OpenMode openMode = ReadWrite )

 	
QLocalServer ( QObject * parent = 0 )


QDataStream &operator<<(QDataStream &ds, const Message &obj)
{
	for(int i=0; i< sizeof( Message ); ++i) 
	{
		ds << *( static_cast<char*>( &obj ) +1 );
	}
	return ds;
}
QDataStream &operator>>(QDataStream &ds, Message &obj)
{
	void * massage = static_cast<char*>(&obj);

	for(int i=0; i<i< sizeof( Message ); ++i)
	{
		ds >> *( massage + 1 );
	}
	return ds;
}

char * m_bytesToSend;
QDataStream stream;


m_socket->readData ( m_bytesToSend, qint64 c )



m_socket->writeData ( receiveBuffor, qint64 c )

int  size = sizeof(Message);


stream.readBytes( m_bytesToSend, sizeof(Message) );


Reads the buffer s from the stream and returns a reference to the stream.

The buffer s is allocated using new. Destroy it with the delete[] operator.