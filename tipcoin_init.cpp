
unsigned short GetListenTTcoinPort()
{
	return (unsigned short)8334;
}

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

    // see Step 2: parameter interactions for more information about these
    fNoListen = !GetBoolArg("-listen", true);
    fDiscover = GetBoolArg("-discover", true);
    fNameLookup = GetBoolArg("-dns", true);

    bool fBound = false;
    if (!fNoListen) {
        if (mapArgs.count("-bind")) {
            BOOST_FOREACH(std::string strBind, mapMultiArgs["-bind"]) {
                CService addrBind;
                if (!Lookup(strBind.c_str(), addrBind, GetListenTTcoinPort(), false))
                    return;// InitError(strprintf(_("Cannot resolve -bind address: '%s'"), strBind.c_str()));
                fBound |= Bind(addrBind, (BF_EXPLICIT | BF_REPORT_ERROR));
            }
        }
        else {
            struct in_addr inaddr_any;
            inaddr_any.s_addr = INADDR_ANY;
#ifdef USE_IPV6
            fBound |= Bind(CService(in6addr_any, GetListenTTcoinPort()), BF_NONE);
#endif
            fBound |= Bind(CService(inaddr_any,  GetListenTTcoinPort()), !fBound ? BF_REPORT_ERROR : BF_NONE);
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
