#include <QtGui/QApplication>

#include <iostream>

#include "bitcoin_init.h"
#include "applet.hpp"

#include "util.h"

unsigned char pchMessageStart[4] = { 0xf9, 0xbe, 0xb4, 0xd9 };


int main( int argc, char **argv )
{
	//QApplication a( argc, argv );
    boost::thread_group threadGroup;
	initialise(threadGroup);
	///Applet hello;
	threadGroup.join_all();
	//hello.show();
	return 0;//a.exec();
}



