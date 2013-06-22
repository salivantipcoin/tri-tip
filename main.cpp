#include <QtGui/QApplication>

#include <iostream>


unsigned char pchMessageStart[4] = { 0xf9, 0xbe, 0xb4, 0xd9 };

void initialise();

int main( int argc, char **argv )
{
	QApplication a( argc, argv );

	
	Applet hello;

	hello.show();
	return a.exec();
}



