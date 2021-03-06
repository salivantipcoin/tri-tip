#ifndef TT_CHAIN_STORAGE_H
#define TT_CHAIN_STORAGE_H

namespace  TTcoin
{

class CTTChainStorage
{
	CTTChainStorage();


};

CTTChainStorage::CTTChainStorage()
{

	// Upgrading to 0.8; hard-link the old blknnnn.dat files into /blocks/
	filesystem::path blocksDir = GetDataDir() / "blocks";
	if (!filesystem::exists(blocksDir))
	{
		filesystem::create_directories(blocksDir);
		bool linked = false;
		for (unsigned int i = 1; i < 10000; i++) {
			filesystem::path source = GetDataDir() / strprintf("blk%04u.dat", i);
			if (!filesystem::exists(source)) break;
			filesystem::path dest = blocksDir / strprintf("blk%05u.dat", i-1);
			try {
				filesystem::create_hard_link(source, dest);
				printf("Hardlinked %s -> %s\n", source.string().c_str(), dest.string().c_str());
				linked = true;
			} catch (filesystem::filesystem_error & e) {
				// Note: hardlink creation failing is not a disaster, it just means
				// blocks will get re-downloaded from peers.
				printf("Error hardlinking blk%04u.dat : %s\n", i, e.what());
				break;
			}
		}

	}

	// cache size calculations
	size_t nTotalCache = GetArg("-dbcache", 25) << 20;
	if (nTotalCache < (1 << 22))
		nTotalCache = (1 << 22); // total cache cannot be less than 4 MiB
	size_t nBlockTreeDBCache = nTotalCache / 8;
	if (nBlockTreeDBCache > (1 << 21) && !GetBoolArg("-txindex", false))
		nBlockTreeDBCache = (1 << 21); // block tree db cache shouldn't be larger than 2 MiB
	nTotalCache -= nBlockTreeDBCache;
	size_t nCoinDBCache = nTotalCache / 2; // use half of the remaining cache for coindb cache
	nTotalCache -= nCoinDBCache;
	nCoinCacheSize = nTotalCache / 300; // coins in memory require around 300 bytes

	bool fLoaded = false;
	while (!fLoaded) {
		bool fReset = fReindex;
		std::string strLoadError;

		uiInterface.InitMessage(_("Loading block index..."));

		nStart = GetTimeMillis();
		do {
			try {
				UnloadBlockIndex();
				delete pcoinsTip;
				delete pcoinsdbview;
				delete pblocktree;

				pblocktree = new CBlockTreeDB(nBlockTreeDBCache, false, fReindex);
				pcoinsdbview = new CCoinsViewDB(nCoinDBCache, false, fReindex);
				pcoinsTip = new CCoinsViewCache(*pcoinsdbview);

				if (!LoadBlockIndex()) {
					strLoadError = _("Error loading block database");
					break;
				}

				// If the loaded chain has a wrong genesis, bail out immediately
				// (we're likely using a testnet datadir, or the other way around).
				if (!mapBlockIndex.empty() && pindexGenesisBlock == NULL)
					return InitError(_("Incorrect or no genesis block found. Wrong datadir for network?"));

				// Check for changed -txindex state (only necessary if we are not reindexing anyway)
				if (!fReindex && fTxIndex != GetBoolArg("-txindex", false)) {
					strLoadError = _("You need to rebuild the database using -reindex to change -txindex");
					break;
				}

				// Initialize the block index (no-op if non-empty database was already loaded)
				if (!InitBlockIndex()) {
					strLoadError = _("Error initializing block database");
					break;
				}

				uiInterface.InitMessage(_("Verifying blocks..."));
				if (!VerifyDB()) {
					strLoadError = _("Corrupted block database detected");
					break;
				}
			} catch(std::exception &e) {
				strLoadError = _("Error opening block database");
				break;
			}

			fLoaded = true;
		} while(false);

		if (!fLoaded) {}
		}
	}
}


FindBlockPos(CValidationState &state, CDiskBlockPos &pos, unsigned int nAddSize, unsigned int nHeight, uint64 nTime, bool fKnown = false)


std::map< long long, CTTtransaction > m_approvedTransaction;


void
g()
{
	std::map< long long, CTTtransaction >::const_iterator iterator;
	iterator = m_approvedTransaction.begin();

	unsigned const  blockCountLimit = 1000;
	unsigned const  blockTimePeriod = 10000;
	int  tmpBlockCountLimit = -1, tmpBlockTimePeriod = -1;

	CTTBlock block;

	while( 1 )
	{
		if ( iterator == m_approvedTransaction.end() )
		{
			if ( block.IsNull() )
				block.WriteToDisk();

			break;
		}

		if ( tmpBlockCountLimit < 0 || tmpBlockTimePeriod < 0 )
		{
			tmpBlockCountLimit = blockCountLimit;
			tmpBlockTimePeriod = blockTimePeriod;

			if ( block.IsNull() )
			{
				//build  merkle tree
				//setDate
				block.WriteToDisk();
			}
			block.SetNull();
		}

		tmpBlockTimePeriod -= iterator->first;
		--tmpBlockCountLimit;
		
		block.vtx.push_back( iterator->second );
		
		++iterator;
	}
}




}
#endif
