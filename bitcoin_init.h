#ifndef BITCOIN_INIT_H
#define BITCOIN_INIT_H
#include "util.h"


void initialise(boost::thread_group& threadGroup);

void loopAndLoadBlock();

void StartShutdown();

#endif
