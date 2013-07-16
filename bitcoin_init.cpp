#include "net.h"
#include "netbase.h"
#include "bitcoin_init.h"
#include "db.h"

#include <boost/interprocess/sync/file_lock.hpp>

using namespace std;
using namespace boost;

// Used to pass flags to the Bind() function
enum BindFlags {
    BF_NONE         = 0,
    BF_EXPLICIT     = (1U << 0),
    BF_REPORT_ERROR = (1U << 1)
};

volatile bool fRequestShutdown = false;

int nBestHeight = -1;
CCriticalSection cs_main;
//////////////////////////////////////////////////////////////////////////////
//
// Registration of network node signals.
//

bool ProcessMessages(CNode* pfrom);
bool SendMessages(CNode* pto, bool fSendTrickle);
bool static ProcessMessage(CNode* pfrom, string strCommand, CDataStream& vRecv);

bool static Bind(const CService &addr, unsigned int flags) {
    if (!(flags & BF_EXPLICIT) && IsLimited(addr))
        return false;
    std::string strError;
    if (!BindListenPort(addr, strError)) {
        if (flags & BF_REPORT_ERROR)
            return false;//InitError(strError);
        return false;
    }
    return true;
}

void StartShutdown()
{
    fRequestShutdown = true;
}

void RegisterNodeSignals(CNodeSignals& nodeSignals)
{
    nodeSignals.ProcessMessages.connect(&ProcessMessages);
    nodeSignals.SendMessages.connect(&SendMessages);
}

void UnregisterNodeSignals(CNodeSignals& nodeSignals)
{
    nodeSignals.ProcessMessages.disconnect(&ProcessMessages);
    nodeSignals.SendMessages.disconnect(&SendMessages);
}

void
initialise(boost::thread_group& threadGroup)
{
	 // ********************************************************* Step 2: application initialization: dir lock, daemonize, pidfile, debug log

#ifdef WIN32

    // Initialize Windows Sockets
    WSADATA wsadata;
    int ret = WSAStartup(MAKEWORD(2,2), &wsadata);
    if (ret != NO_ERROR || LOBYTE(wsadata.wVersion ) != 2 || HIBYTE(wsadata.wVersion) != 2)
    {
    	return;
   //     return InitError(strprintf("Error: Winsock library failed to start (WSAStartup returned error %d)", ret));
    }
#endif
#ifndef WIN32
#endif
    // ********************************************************* Step 4: application initialization: dir lock, daemonize, pidfile, debug log

    std::string strDataDir = GetDataDir().string();

    // Make sure only a single Bitcoin process is using the data directory.
    boost::filesystem::path pathLockFile = GetDataDir() / ".lock";
    FILE* file = fopen(pathLockFile.string().c_str(), "a"); // empty lock file; created if it doesn't exist.
    if (file) fclose(file);
    static boost::interprocess::file_lock lock(pathLockFile.string().c_str());
    if (!lock.try_lock())
    	return;
    	//  return InitError(strprintf(_("Cannot obtain a lock on data directory %s. Bitcoin is probably already running."), strDataDir.c_str()));

    if (GetBoolArg("-shrinkdebugfile", !fDebug))
        ShrinkDebugFile();
    printf("\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n");
    printf("Bitcoin version %s (%s)\n", FormatFullVersion().c_str(), CLIENT_DATE.c_str());
    printf("Using OpenSSL version %s\n", SSLeay_version(SSLEAY_VERSION));
    if (!fLogTimestamps)
        printf("Startup time: %s\n", DateTimeStrFormat("%Y-%m-%d %H:%M:%S", GetTime()).c_str());
    printf("Default data directory %s\n", GetDefaultDataDir().string().c_str());
    printf("Using data directory %s\n", strDataDir.c_str());
   // printf("Using at most %i connections (%i file descriptors available)\n", nMaxConnections, nFD);
    std::ostringstream strErrors;

    if (fDaemon)
        fprintf(stdout, "Bitcoin server starting\n");
/*
    if (nScriptCheckThreads) {
        printf("Using %u threads for script verification\n", nScriptCheckThreads);
        for (int i=0; i<nScriptCheckThreads-1; i++)
            threadGroup.create_thread(&ThreadScriptCheck);
    }
    */

    // ********************************************************* Step 6: network initialization
   RegisterNodeSignals(GetNodeSignals());
/*
    int nSocksVersion = GetArg("-socks", 5);
    if (nSocksVersion != 4 && nSocksVersion != 5)*/


    int nSocksVersion = 5;
#if defined(USE_IPV6)
#if ! USE_IPV6
    else
        SetLimited(NET_IPV6);
#endif
#endif

    CService addrProxy;
    bool fProxy = false;
    if (mapArgs.count("-proxy")) {
        addrProxy = CService(mapArgs["-proxy"], 9050);
        if (!addrProxy.IsValid())
            return;// InitError(strprintf(_("Invalid -proxy address: '%s'"), mapArgs["-proxy"].c_str()));

        if (!IsLimited(NET_IPV4))
            SetProxy(NET_IPV4, addrProxy, nSocksVersion);
        if (nSocksVersion > 4) {
#ifdef USE_IPV6
            if (!IsLimited(NET_IPV6))
                SetProxy(NET_IPV6, addrProxy, nSocksVersion);
#endif
            SetNameProxy(addrProxy, nSocksVersion);
        }
        fProxy = true;
    }

    // -tor can override normal proxy, -notor disables tor entirely
    if (!(mapArgs.count("-tor") && mapArgs["-tor"] == "0") && (fProxy || mapArgs.count("-tor"))) {
        CService addrOnion;
        if (!mapArgs.count("-tor"))
            addrOnion = addrProxy;
        else
            addrOnion = CService(mapArgs["-tor"], 9050);
        if (!addrOnion.IsValid())
            return;// InitError(strprintf(_("Invalid -tor address: '%s'"), mapArgs["-tor"].c_str()));
        SetProxy(NET_TOR, addrOnion, 5);
        SetReachable(NET_TOR);
    }

    // see Step 2: parameter interactions for more information about these
    fNoListen = !GetBoolArg("-listen", true);
    fDiscover = GetBoolArg("-discover", true);
    fNameLookup = GetBoolArg("-dns", true);

    bool fBound = false;
    if (!fNoListen) {
        if (mapArgs.count("-bind")) {
            BOOST_FOREACH(std::string strBind, mapMultiArgs["-bind"]) {
                CService addrBind;
                if (!Lookup(strBind.c_str(), addrBind, GetListenPort(), false))
                    return;// InitError(strprintf(_("Cannot resolve -bind address: '%s'"), strBind.c_str()));
                fBound |= Bind(addrBind, (BF_EXPLICIT | BF_REPORT_ERROR));
            }
        }
        else {
            struct in_addr inaddr_any;
            inaddr_any.s_addr = INADDR_ANY;
#ifdef USE_IPV6
            fBound |= Bind(CService(in6addr_any, GetListenPort()), BF_NONE);
#endif
            fBound |= Bind(CService(inaddr_any, GetListenPort()), !fBound ? BF_REPORT_ERROR : BF_NONE);
        }
        if (!fBound)
            return;// InitError(_("Failed to listen on any port. Use -listen=0 if you want this."));
    }

    if (mapArgs.count("-externalip")) {
        BOOST_FOREACH(string strAddr, mapMultiArgs["-externalip"]) {
            CService addrLocal(strAddr, GetListenPort(), fNameLookup);
            if (!addrLocal.IsValid())
                return;// InitError(strprintf(_("Cannot resolve -externalip address: '%s'"), strAddr.c_str()));
            AddLocal(CService(strAddr, GetListenPort(), fNameLookup), LOCAL_MANUAL);
        }
    }

// return InitError(strprintf(_("Unknown -socks proxy version requested: %i"), nSocksVersion));
    // ********************************************************* Step 10: load peers
    int64 nStart;
   // uiInterface.InitMessage(_("Loading addresses..."));

    nStart = GetTimeMillis();

    {
        CAddrDB::SetMessageStart(pchMessageStart);
        CAddrDB adb;
        if (!adb.Read(addrman))
            printf("Invalid or missing peers.dat; recreating\n");
    }

    printf("Loaded %i addresses from peers.dat  %"PRI64d"ms\n",
           addrman.size(), GetTimeMillis() - nStart);

    // ********************************************************* Step 11: start node

    //if (!CheckDiskSpace())
    //    return false;

    //if (!strErrors.str().empty())
    //    return InitError(strErrors.str());

    RandAddSeedPerfmon();


    CTTChainStorage * storage = new CTTChainStorage;
/*


	setup ttcoin network
	
*/

	setCurrentBlockBitcoinIndex();

	StartNode(threadGroup);

//   threadGroup.create_thread(boost::bind(&LoopForever<void (*)()>, "", &handleClients));


//   threadGroup.create_thread(boost::bind(&LoopForever<void (*)()>, "", &));  **  set  of  ttcomunication threads

//   threadGroup.create_thread(boost::bind(&LoopForever<void (*)()>, "", &createProveOfWork));

//   threadGroup.create_thread(boost::bind(&LoopForever<void (*)()>, "", &createProveOfWork));

//   threadGroup.create_thread(boost::bind(&LoopForever<void (*)()>, "", &handleBlocks));  // introduce  transactions, handle synchronizations, handle blockchain
/*
    turn out  to  be  a  lot  of  thread  -ttnetwork  thread -  comunicate  with  ttnetwork  handle  transaction  old  ways 
// in  this  app  there  will be  a lot  of  threads 

	thread  communication  with  clients 
    thread work -  make  all  prove of  work  stuff 
    thread check   for  correctnes  of  transactions and  introduce them to a block
*/
}
