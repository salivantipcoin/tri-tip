#include "applet.hpp"


#include <QtGui/QKeyEvent>
#include <QtGui/QFileDialog>
#include <QtCore/QTextStream>
#include <QtCore/QString>
#include <QtCore/QTimer>
#include <QtCore/QCoreApplication>
#include <QtGui/QVBoxLayout>
#include <QtGui/QHBoxLayout>

#include  "key.h"

	Applet::Applet()
	: QWidget()
	{
		QVBoxLayout * m_MainLayout = new QVBoxLayout( this );

		QHBoxLayout * m_addAddressLayout = new QHBoxLayout;
		QHBoxLayout * m_sendCoinLayout = new QHBoxLayout;

		m_addNewAddress = new QPushButton("Add Address");
		m_enterAddress = new QLineEdit;
		m_address = new QLabel( "" );
		m_targetAddress = new QLineEdit( "" );
		m_sendCoin = new QPushButton( "send coin" );

		m_addAddressLayout->addWidget( m_enterAddress );
		m_addAddressLayout->addWidget( m_addNewAddress );

		m_sendCoinLayout->addWidget( m_targetAddress );
		m_sendCoinLayout->addWidget( m_sendCoin );

		m_MainLayout->addLayout( m_addAddressLayout );
		m_MainLayout->addWidget( m_address );
		m_MainLayout->addLayout( m_sendCoinLayout );
	}


