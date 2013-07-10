#ifndef BITCOIN_MESSAGES_HANDLER_H
#define BITCOIN_MESSAGES_HANDLER_H

#include "net.h"

using namespace std;

/* don't participate  in transfering messages in bitcoin network for now */
void static ProcessGetData(CNode* pfrom);

// requires LOCK(cs_vRecvMsg)
bool ProcessMessages(CNode* pfrom);


bool SendMessages(CNode* pto, bool fSendTrickle);


bool static ProcessMessage(CNode* pfrom, string strCommand, CDataStream& vRecv);


#endif
