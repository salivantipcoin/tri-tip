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
	void * payload;
};

Message message;

QDataStream dataStream << message;



class CTTDialog
{

	CTTDialog( QLocalSocket * _localSocket );
	
	void sendMessage( Message _message );
	boost::optional< Message > getMessage();
	
	void getIndex() const;

private:
	std::list< Message > m_messageSended;

	int m_index;

	QLocalSocket * m_socket;
};

CTTDialog::CTTDialog( QLocalSocket * _localSocket )
	: m_socket( _localSocket )
{
}

void
CTTDialog::getIndex() const
{
	return m_index;
}

void
CTTDialog::sendMessage( Message _message )
{
	QDataStream data;

	int messageSize =sizeof( Message );

	data << _message;

	char writeBuffor[ messageSize ];

	data.readBytes ( writeBuffor, messageSize )

	m_socket->writeData ( writeBuffor, qint64 c )
}


boost::optional< Message >
CTTDialog::getMessage()
{
	if ( m_socket->bytesAvailable() > 0 )
		return boost::optional< Message >();

	int messageSize =sizeof( Message );

	assert( m_socket->bytesAvailable() >= messageSize );
	char * m_bytesToSend;

	m_socket->readData ( m_bytesToSend, messageSize ) ;

	QDataStream stream( m_bytesToSend );

	Message message;
	message << stream;

	return boost::optional< Message >( message );
}

struct MessagesToProcess
{
	QMutex * m_mutex;
	std::map< unsigned int, Message > messages;
}; 

MessagesToProcess messagesToProcess;

processMessage( const Message message, int index )
{
	messagesToProcess.m_mutex->lock();

	messages.insert( std::make_pair( index, message ) );

	messagesToProcess.m_mutex->unlock();
}

struct Responses
{
	QMutex * m_mutex;
	std::map< unsigned int, Message > responses;
}; 

Responses  responses;

void sendResponse( CTTDialog* dialog )
{
	std::map< unsigned int, Message >::iterator  iterator;
	
	responses.m_mutex->lock();

	while( 1 )
	{
		iterator = responses.find( dialog->getIndex() );

		dialog->sendMessage( iterator->second );

		responses.erase( iterator );
	}
	
	responses.m_mutex->unlock();
}

void 
serverMainLoop()
{

QLocalServer * localServer = new QLocalServer;

localServer->listen ( "ttenvironment" );

std::list< CTTDialog* > activeConnections;

while( 1 )
{
	if ( localServer->hasPendingConnections() )
	{
		activeConnections.pushback( new CTTDialog( localServer->nextPendingConnection () ) );
	}

	BOOST_FOREACH( CTTDialog* dialog, activeConnections )
	{
		boost::optional< Message > message = dialog->getMessage();
		if ( message )
			processMessage( *message, dialog->getIndex() );

		sendResponse( dialog );
	}
}

}

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





m_socket->writeData ( receiveBuffor, qint64 c )

int  size = sizeof(Message);


stream.readBytes( m_bytesToSend, sizeof(Message) );


Reads the buffer s from the stream and returns a reference to the stream.

The buffer s is allocated using new. Destroy it with the delete[] operator.