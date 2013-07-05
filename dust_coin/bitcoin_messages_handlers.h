#ifndef BITCOIN_MESSAGES_HANDLER_H
#define BITCOIN_MESSAGES_HANDLER_H

/* don't participate  in transfering messages in bitcoin network for now */
void static ProcessGetData(CNode* pfrom)
{
		pfrom->PushMessage("notfound", vNotFound);
}

// requires LOCK(cs_vRecvMsg)
bool ProcessMessages(CNode* pfrom)
{
	//if (fDebug)
	//    printf("ProcessMessages(%zu messages)\n", pfrom->vRecvMsg.size());

	//
	// Message format
	//  (4) message start
	//  (12) command
	//  (4) size
	//  (4) checksum
	//  (x) data
	//
	bool fOk = true;

	if (!pfrom->vRecvGetData.empty())
		ProcessGetData(pfrom);

	std::deque<CNetMessage>::iterator it = pfrom->vRecvMsg.begin();
	while (!pfrom->fDisconnect && it != pfrom->vRecvMsg.end()) {
		// Don't bother if send buffer is too full to respond anyway
		if (pfrom->nSendSize >= SendBufferSize())
			break;

		// get next message
		CNetMessage& msg = *it;

		//if (fDebug)
		//    printf("ProcessMessages(message %u msgsz, %zu bytes, complete:%s)\n",
		//            msg.hdr.nMessageSize, msg.vRecv.size(),
		//            msg.complete() ? "Y" : "N");

		// end, if an incomplete message is found
		if (!msg.complete())
			break;

		// at this point, any failure means we can delete the current message
		it++;

		// Scan for message start
		if (memcmp(msg.hdr.pchMessageStart, pchMessageStart, sizeof(pchMessageStart)) != 0) {
			printf("\n\nPROCESSMESSAGE: INVALID MESSAGESTART\n\n");
			fOk = false;
			break;
		}

		// Read header
		CMessageHeader& hdr = msg.hdr;
		if (!hdr.IsValid())
		{
			printf("\n\nPROCESSMESSAGE: ERRORS IN HEADER %s\n\n\n", hdr.GetCommand().c_str());
			continue;
		}
		string strCommand = hdr.GetCommand();

		// Message size
		unsigned int nMessageSize = hdr.nMessageSize;

		// Checksum
		CDataStream& vRecv = msg.vRecv;
		uint256 hash = Hash(vRecv.begin(), vRecv.begin() + nMessageSize);
		unsigned int nChecksum = 0;
		memcpy(&nChecksum, &hash, sizeof(nChecksum));
		if (nChecksum != hdr.nChecksum)
		{
			printf("ProcessMessages(%s, %u bytes) : CHECKSUM ERROR nChecksum=%08x hdr.nChecksum=%08x\n",
				strCommand.c_str(), nMessageSize, nChecksum, hdr.nChecksum);
			continue;
		}

		// Process message
		bool fRet = false;
		try
		{
			{
				LOCK(cs_main);
				fRet = ProcessMessage(pfrom, strCommand, vRecv);
			}
			boost::this_thread::interruption_point();
		}
		catch (std::ios_base::failure& e)
		{
			if (strstr(e.what(), "end of data"))
			{
				// Allow exceptions from under-length message on vRecv
				printf("ProcessMessages(%s, %u bytes) : Exception '%s' caught, normally caused by a message being shorter than its stated length\n", strCommand.c_str(), nMessageSize, e.what());
			}
			else if (strstr(e.what(), "size too large"))
			{
				// Allow exceptions from over-long size
				printf("ProcessMessages(%s, %u bytes) : Exception '%s' caught\n", strCommand.c_str(), nMessageSize, e.what());
			}
			else
			{
				PrintExceptionContinue(&e, "ProcessMessages()");
			}
		}
		catch (boost::thread_interrupted) {
			throw;
		}
		catch (std::exception& e) {
			PrintExceptionContinue(&e, "ProcessMessages()");
		} catch (...) {
			PrintExceptionContinue(NULL, "ProcessMessages()");
		}

		if (!fRet)
			printf("ProcessMessage(%s, %u bytes) FAILED\n", strCommand.c_str(), nMessageSize);
	}

	// In case the connection got shut down, its receive buffer was wiped
	if (!pfrom->fDisconnect)
		pfrom->vRecvMsg.erase(pfrom->vRecvMsg.begin(), it);

	return fOk;
}


bool SendMessages(CNode* pto, bool fSendTrickle)
{
	TRY_LOCK(cs_main, lockMain);
	if (lockMain) {
		// Don't send anything until we get their version message
		if (pto->nVersion == 0)
			return true;

		// Keep-alive ping. We send a nonce of zero because we don't use it anywhere
		// right now.
		if (pto->nLastSend && GetTime() - pto->nLastSend > 30 * 60 && pto->vSendMsg.empty()) {
			uint64 nonce = 0;
			if (pto->nVersion > BIP0031_VERSION)
				pto->PushMessage("ping", nonce);
			else
				pto->PushMessage("ping");
		}

		// Start block sync
		if (pto->fStartSync && !fImporting && !fReindex) {
			pto->fStartSync = false;
			PushGetBlocks(pto, pindexBest, uint256(0));
		}

		// Address refresh broadcast
		static int64 nLastRebroadcast;
		if (!IsInitialBlockDownload() && (GetTime() - nLastRebroadcast > 24 * 60 * 60))
		{
			{
				LOCK(cs_vNodes);
				BOOST_FOREACH(CNode* pnode, vNodes)
				{
					// Periodically clear setAddrKnown to allow refresh broadcasts
					if (nLastRebroadcast)
						pnode->setAddrKnown.clear();

					// Rebroadcast our address
					if (!fNoListen)
					{
						CAddress addr = GetLocalAddress(&pnode->addr);
						if (addr.IsRoutable())
							pnode->PushAddress(addr);
					}
				}
			}
			nLastRebroadcast = GetTime();
		}

		//
		// Message: addr
		//
		if (fSendTrickle)
		{
			vector<CAddress> vAddr;
			vAddr.reserve(pto->vAddrToSend.size());
			BOOST_FOREACH(const CAddress& addr, pto->vAddrToSend)
			{
				// returns true if wasn't already contained in the set
				if (pto->setAddrKnown.insert(addr).second)
				{
					vAddr.push_back(addr);
					// receiver rejects addr messages larger than 1000
					if (vAddr.size() >= 1000)
					{
						pto->PushMessage("addr", vAddr);
						vAddr.clear();
					}
				}
			}
			pto->vAddrToSend.clear();
			if (!vAddr.empty())
				pto->PushMessage("addr", vAddr);
		}

		//
		// Message: getdata
		//
		vector<CInv> vGetData;
		int64 nNow = GetTime() * 1000000;
		while (!pto->mapAskFor.empty() && (*pto->mapAskFor.begin()).first <= nNow)
		{
			const CInv& inv = (*pto->mapAskFor.begin()).second;
			if (!AlreadyHave(inv))
			{
				if (fDebugNet)
					printf("sending getdata: %s\n", inv.ToString().c_str());
				vGetData.push_back(inv);
				if (vGetData.size() >= 1000)
				{
					pto->PushMessage("getdata", vGetData);
					vGetData.clear();
				}
			}
			pto->mapAskFor.erase(pto->mapAskFor.begin());
		}
		if (!vGetData.empty())
			pto->PushMessage("getdata", vGetData);

	}
	return true;
}


bool static ProcessMessage(CNode* pfrom, string strCommand, CDataStream& vRecv)
{
	RandAddSeedPerfmon();
	if (fDebug)
		printf("received: %s (%"PRIszu" bytes)\n", strCommand.c_str(), vRecv.size());
	if (mapArgs.count("-dropmessagestest") && GetRand(atoi(mapArgs["-dropmessagestest"])) == 0)
	{
		printf("dropmessagestest DROPPING RECV MESSAGE\n");
		return true;
	}


	if (strCommand == "version")
	{
		// Each connection can only send one version message
		if (pfrom->nVersion != 0)
		{
			pfrom->Misbehaving(1);
			return false;
		}

		int64 nTime;
		CAddress addrMe;
		CAddress addrFrom;
		uint64 nNonce = 1;
		vRecv >> pfrom->nVersion >> pfrom->nServices >> nTime >> addrMe;
		if (pfrom->nVersion < MIN_PROTO_VERSION)
		{
			// Since February 20, 2012, the protocol is initiated at version 209,
			// and earlier versions are no longer supported
			printf("partner %s using obsolete version %i; disconnecting\n", pfrom->addr.ToString().c_str(), pfrom->nVersion);
			pfrom->fDisconnect = true;
			return false;
		}

		if (pfrom->nVersion == 10300)
			pfrom->nVersion = 300;
		if (!vRecv.empty())
			vRecv >> addrFrom >> nNonce;
		if (!vRecv.empty())
			vRecv >> pfrom->strSubVer;
		if (!vRecv.empty())
			vRecv >> pfrom->nStartingHeight;
		if (!vRecv.empty())
			vRecv >> pfrom->fRelayTxes; // set to true after we get the first filter* message
		else
			pfrom->fRelayTxes = true;

		if (pfrom->fInbound && addrMe.IsRoutable())
		{
			pfrom->addrLocal = addrMe;
			SeenLocal(addrMe);
		}

		// Disconnect if we connected to ourself
		if (nNonce == nLocalHostNonce && nNonce > 1)
		{
			printf("connected to self at %s, disconnecting\n", pfrom->addr.ToString().c_str());
			pfrom->fDisconnect = true;
			return true;
		}

		// Be shy and don't send version until we hear
		if (pfrom->fInbound)
			pfrom->PushVersion();

		pfrom->fClient = !(pfrom->nServices & NODE_NETWORK);

		AddTimeData(pfrom->addr, nTime);

		// Change version
		pfrom->PushMessage("verack");
		pfrom->ssSend.SetVersion(min(pfrom->nVersion, PROTOCOL_VERSION));

/*
reconsider  what  happen her  this  is  advertisment  part 
*/

		if (!pfrom->fInbound)
		{
			// Advertise our address
			if (!fNoListen && !IsInitialBlockDownload())
			{
				CAddress addr = GetLocalAddress(&pfrom->addr);
				if (addr.IsRoutable())
					pfrom->PushAddress(addr);
			}

			// Get recent addresses
			if (pfrom->fOneShot || pfrom->nVersion >= CADDR_TIME_VERSION || addrman.size() < 1000)
			{
				pfrom->PushMessage("getaddr");
				pfrom->fGetAddr = true;
			}
			addrman.Good(pfrom->addr);
		} else {
			if (((CNetAddr)pfrom->addr) == (CNetAddr)addrFrom)
			{
				addrman.Add(addrFrom, addrFrom);
				addrman.Good(addrFrom);
			}
		}

		// Relay alerts
		{
			LOCK(cs_mapAlerts);
			BOOST_FOREACH(PAIRTYPE(const uint256, CAlert)& item, mapAlerts)
				item.second.RelayTo(pfrom);
		}

		pfrom->fSuccessfullyConnected = true;

		printf("receive version message: version %d, blocks=%d, us=%s, them=%s, peer=%s\n", pfrom->nVersion, pfrom->nStartingHeight, addrMe.ToString().c_str(), addrFrom.ToString().c_str(), pfrom->addr.ToString().c_str());

		cPeerBlockCounts.input(pfrom->nStartingHeight);
	}


	else if (pfrom->nVersion == 0)
	{
		// Must have a version message before anything else
		pfrom->Misbehaving(1);
		return false;
	}


	else if (strCommand == "verack")
	{
		pfrom->SetRecvVersion(min(pfrom->nVersion, PROTOCOL_VERSION));
	}


	else if (strCommand == "addr")
	{
		vector<CAddress> vAddr;
		vRecv >> vAddr;

		// Don't want addr from older versions unless seeding
		if (pfrom->nVersion < CADDR_TIME_VERSION && addrman.size() > 1000)
			return true;
		if (vAddr.size() > 1000)
		{
			pfrom->Misbehaving(20);
			return error("message addr size() = %"PRIszu"", vAddr.size());
		}

		// Store the new addresses
		vector<CAddress> vAddrOk;
		int64 nNow = GetAdjustedTime();
		int64 nSince = nNow - 10 * 60;
		BOOST_FOREACH(CAddress& addr, vAddr)
		{
			boost::this_thread::interruption_point();

			if (addr.nTime <= 100000000 || addr.nTime > nNow + 10 * 60)
				addr.nTime = nNow - 5 * 24 * 60 * 60;
			pfrom->AddAddressKnown(addr);
			bool fReachable = IsReachable(addr);
			if (addr.nTime > nSince && !pfrom->fGetAddr && vAddr.size() <= 10 && addr.IsRoutable())
			{
				// Relay to a limited number of other nodes
				{
					LOCK(cs_vNodes);
					// Use deterministic randomness to send to the same nodes for 24 hours
					// at a time so the setAddrKnowns of the chosen nodes prevent repeats
					static uint256 hashSalt;
					if (hashSalt == 0)
						hashSalt = GetRandHash();
					uint64 hashAddr = addr.GetHash();
					uint256 hashRand = hashSalt ^ (hashAddr<<32) ^ ((GetTime()+hashAddr)/(24*60*60));
					hashRand = Hash(BEGIN(hashRand), END(hashRand));
					multimap<uint256, CNode*> mapMix;
					BOOST_FOREACH(CNode* pnode, vNodes)
					{
						if (pnode->nVersion < CADDR_TIME_VERSION)
							continue;
						unsigned int nPointer;
						memcpy(&nPointer, &pnode, sizeof(nPointer));
						uint256 hashKey = hashRand ^ nPointer;
						hashKey = Hash(BEGIN(hashKey), END(hashKey));
						mapMix.insert(make_pair(hashKey, pnode));
					}
					int nRelayNodes = fReachable ? 2 : 1; // limited relaying of addresses outside our network(s)
					for (multimap<uint256, CNode*>::iterator mi = mapMix.begin(); mi != mapMix.end() && nRelayNodes-- > 0; ++mi)
						((*mi).second)->PushAddress(addr);
				}
			}
			// Do not store addresses outside our network
			if (fReachable)
				vAddrOk.push_back(addr);
		}
		addrman.Add(vAddrOk, pfrom->addr, 2 * 60 * 60);
		if (vAddr.size() < 1000)
			pfrom->fGetAddr = false;
		if (pfrom->fOneShot)
			pfrom->fDisconnect = true;
	}


	else if (strCommand == "inv")
	{
		vector<CInv> vInv;
		vRecv >> vInv;
		if (vInv.size() > MAX_INV_SZ)
		{
			pfrom->Misbehaving(20);
			return error("message inv size() = %"PRIszu"", vInv.size());
		}

		// find last block in inv vector
		unsigned int nLastBlock = (unsigned int)(-1);
		for (unsigned int nInv = 0; nInv < vInv.size(); nInv++) {
			if (vInv[vInv.size() - 1 - nInv].type == MSG_BLOCK) {
				nLastBlock = vInv.size() - 1 - nInv;
				break;
			}
		}
		for (unsigned int nInv = 0; nInv < vInv.size(); nInv++)
		{
			const CInv &inv = vInv[nInv];

			boost::this_thread::interruption_point();
			pfrom->AddInventoryKnown(inv);

			bool fAlreadyHave = AlreadyHave(inv);
			if (fDebug)
				printf("  got inventory: %s  %s\n", inv.ToString().c_str(), fAlreadyHave ? "have" : "new");

			if (!fAlreadyHave) {
				if (!fImporting && !fReindex)
					pfrom->AskFor(inv);
			} else if (inv.type == MSG_BLOCK && mapOrphanBlocks.count(inv.hash)) {
				PushGetBlocks(pfrom, pindexBest, GetOrphanRoot(mapOrphanBlocks[inv.hash]));
			} else if (nInv == nLastBlock) {
				// In case we are on a very long side-chain, it is possible that we already have
				// the last block in an inv bundle sent in response to getblocks. Try to detect
				// this situation and push another getblocks to continue.
				PushGetBlocks(pfrom, mapBlockIndex[inv.hash], uint256(0));
				if (fDebug)
					printf("force request: %s\n", inv.ToString().c_str());
			}
		}
	}


	else if (strCommand == "getdata")
	{
		vector<CInv> vInv;
		vRecv >> vInv;
		if (vInv.size() > MAX_INV_SZ)
		{
			pfrom->Misbehaving(20);
			return error("message getdata size() = %"PRIszu"", vInv.size());
		}

		if (fDebugNet || (vInv.size() != 1))
			printf("received getdata (%"PRIszu" invsz)\n", vInv.size());

		if ((fDebugNet && vInv.size() > 0) || (vInv.size() == 1))
			printf("received getdata for: %s\n", vInv[0].ToString().c_str());

		pfrom->vRecvGetData.insert(pfrom->vRecvGetData.end(), vInv.begin(), vInv.end());
		ProcessGetData(pfrom);
	}


	else if (strCommand == "getblocks")
	{
	}


	else if (strCommand == "getheaders")
	{
		CBlockLocator locator;
		uint256 hashStop;
		vRecv >> locator >> hashStop;

		CBlockIndex* pindex = NULL;
		if (locator.IsNull())
		{
			// If locator is null, return the hashStop block
			map<uint256, CBlockIndex*>::iterator mi = mapBlockIndex.find(hashStop);
			if (mi == mapBlockIndex.end())
				return true;
			pindex = (*mi).second;
		}
		else
		{
			// Find the last block the caller has in the main chain
			pindex = locator.GetBlockIndex();
			if (pindex)
				pindex = pindex->GetNextInMainChain();
		}

		// we must use CBlocks, as CBlockHeaders won't include the 0x00 nTx count at the end
		vector<CBlock> vHeaders;
		int nLimit = 2000;
		printf("getheaders %d to %s\n", (pindex ? pindex->nHeight : -1), hashStop.ToString().c_str());
		for (; pindex; pindex = pindex->GetNextInMainChain())
		{
			vHeaders.push_back(pindex->GetBlockHeader());
			if (--nLimit <= 0 || pindex->GetBlockHash() == hashStop)
				break;
		}
		pfrom->PushMessage("headers", vHeaders);
	}


	else if (strCommand == "tx")
	{
		vector<uint256> vWorkQueue;
		vector<uint256> vEraseQueue;
		CDataStream vMsg(vRecv);
		CTransaction tx;
		vRecv >> tx;

		CInv inv(MSG_TX, tx.GetHash());
		pfrom->AddInventoryKnown(inv);

		if ( mapAlreadyAskedFor.find(inv) )
		{
			mapAlreadyAskedFor.erase(inv);

			// add inventory transfer them  to  needed  place 
		}

	}


	else if (strCommand == "block" && !fImporting && !fReindex) // Ignore blocks received while importing
	{
		CBlock block;
		vRecv >> block;

		printf("received block %s\n", block.GetHash().ToString().c_str());
		// block.print();

		CInv inv(MSG_BLOCK, block.GetHash());
		pfrom->AddInventoryKnown(inv);

		CValidationState state;
		if (ProcessBlock(state, pfrom, &block))
			mapAlreadyAskedFor.erase(inv);
		int nDoS;
		if (state.IsInvalid(nDoS))
			pfrom->Misbehaving(nDoS);
	}


	else if (strCommand == "getaddr")
	{
		pfrom->vAddrToSend.clear();
		vector<CAddress> vAddr = addrman.GetAddr();
		BOOST_FOREACH(const CAddress &addr, vAddr)
			pfrom->PushAddress(addr);
	}


	else if (strCommand == "mempool")
	{
		std::vector<uint256> vtxid;
		LOCK2(mempool.cs, pfrom->cs_filter);
		mempool.queryHashes(vtxid);
		vector<CInv> vInv;
		BOOST_FOREACH(uint256& hash, vtxid) {
			CInv inv(MSG_TX, hash);
			if ((pfrom->pfilter && pfrom->pfilter->IsRelevantAndUpdate(mempool.lookup(hash), hash)) ||
				(!pfrom->pfilter))
				vInv.push_back(inv);
			if (vInv.size() == MAX_INV_SZ)
				break;
		}
		if (vInv.size() > 0)
			pfrom->PushMessage("inv", vInv);
	}


	else if (strCommand == "ping")
	{
		if (pfrom->nVersion > BIP0031_VERSION)
		{
			uint64 nonce = 0;
			vRecv >> nonce;
			// Echo the message back with the nonce. This allows for two useful features:
			//
			// 1) A remote node can quickly check if the connection is operational
			// 2) Remote nodes can measure the latency of the network thread. If this node
			//    is overloaded it won't respond to pings quickly and the remote node can
			//    avoid sending us more work, like chain download requests.
			//
			// The nonce stops the remote getting confused between different pings: without
			// it, if the remote node sends a ping once per second and this node takes 5
			// seconds to respond to each, the 5th ping the remote sends would appear to
			// return very quickly.
			pfrom->PushMessage("pong", nonce);
		}
	}


	else if (strCommand == "alert")
	{
		CAlert alert;
		vRecv >> alert;

		uint256 alertHash = alert.GetHash();
		if (pfrom->setKnown.count(alertHash) == 0)
		{
			if (alert.ProcessAlert())
			{
				// Relay
				pfrom->setKnown.insert(alertHash);
				{
					LOCK(cs_vNodes);
					BOOST_FOREACH(CNode* pnode, vNodes)
						alert.RelayTo(pnode);
				}
			}
			else {
				// Small DoS penalty so peers that send us lots of
				// duplicate/expired/invalid-signature/whatever alerts
				// eventually get banned.
				// This isn't a Misbehaving(100) (immediate ban) because the
				// peer might be an older or different implementation with
				// a different signature key, etc.
				pfrom->Misbehaving(10);
			}
		}
	}


	else if (strCommand == "filterload")
	{
		CBloomFilter filter;
		vRecv >> filter;

		if (!filter.IsWithinSizeConstraints())
			// There is no excuse for sending a too-large filter
			pfrom->Misbehaving(100);
		else
		{
			LOCK(pfrom->cs_filter);
			delete pfrom->pfilter;
			pfrom->pfilter = new CBloomFilter(filter);
		}
		pfrom->fRelayTxes = true;
	}


	else if (strCommand == "filteradd")
	{
		vector<unsigned char> vData;
		vRecv >> vData;

		// Nodes must NEVER send a data item > 520 bytes (the max size for a script data object,
		// and thus, the maximum size any matched object can have) in a filteradd message
		if (vData.size() > MAX_SCRIPT_ELEMENT_SIZE)
		{
			pfrom->Misbehaving(100);
		} else {
			LOCK(pfrom->cs_filter);
			if (pfrom->pfilter)
				pfrom->pfilter->insert(vData);
			else
				pfrom->Misbehaving(100);
		}
	}


	else if (strCommand == "filterclear")
	{
		LOCK(pfrom->cs_filter);
		delete pfrom->pfilter;
		pfrom->pfilter = NULL;
		pfrom->fRelayTxes = true;
	}


	else
	{
		// Ignore unknown commands for extensibility
	}


	// Update the last seen time for this node's address
	if (pfrom->fNetworkNode)
		if (strCommand == "version" || strCommand == "addr" || strCommand == "inv" || strCommand == "getdata" || strCommand == "ping")
			AddressCurrentlyConnected(pfrom->addr);

}


#endif