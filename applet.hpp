#ifndef  APPLET
#define  APPLET

#include <QtGui/QCheckBox>
#include <QtGui/QPushButton>
#include <QtGui/QVBoxLayout>
#include <QtGui/QLabel>
#include <QtGui/QTextEdit>
#include <QtGui/QSlider>
#include <QLineEdit>


class Applet : public QWidget
{
	Q_OBJECT
public:
	Applet();
	QPushButton * m_addNewAddress;
	QLineEdit * m_enterAddress;
	QLabel * m_address;
	QLineEdit * m_targetAddress;
	QPushButton * m_sendCoin;
};

#endif
