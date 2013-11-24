#include "init.h"
#include "util.h"
#include "sync.h"
#include "ui_interface.h"
#include "base58.h"
#include "bitcoinrpc.h"
#include "db.h"

#include <boost/asio.hpp>
#include <boost/asio/ip/v6_only.hpp>
#include <boost/bind.hpp>
#include <boost/filesystem.hpp>
#include <boost/foreach.hpp>
#include <boost/iostreams/concepts.hpp>
#include <boost/iostreams/stream.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/xpressive/xpressive_dynamic.hpp>
#include <list>

using namespace std;
using namespace boost;
using namespace boost::asio;
using namespace json_spirit;

#include "httppost.h"

/*
 *Add transaction to postqueue
 */
void monitorTx(const CTransaction& tx)
{
    Array params; // JSON-RPC requests are always "params" : [ ... ]
    Object entry;
    uint256 hashBlock = 0;
    TxToJSON(tx, hashBlock, entry);
    params.push_back(entry);

    string postBody = JSONRPCRequest("monitortx", params, Value());
    printf("monitortx Postbody: %s", postBody.c_str());
    {
        LOCK2(cs_mapMonitored, cs_vPOSTQueue);
        BOOST_FOREACH (const string& url, setMonitorTx)
        {
            boost::shared_ptr<CPOSTRequest> postRequest(new CPOSTRequest(url, postBody));
            vPOSTQueue.push_back(postRequest);
        }
    }
}

/*
 *Add block to postqueue
 **/
void monitorBlock(const CBlock& block, const CBlockIndex* pblockindex)
{
    Array params; // JSON-RPC requests are always "params" : [ ... ]
    params.push_back(blockToJSON(block, pblockindex));

    string postBody = JSONRPCRequest("monitorblock", params, Value());
    printf("monitorblock Postbody: %s", postBody.c_str());
    {
        LOCK2(cs_mapMonitored, cs_vPOSTQueue);
        BOOST_FOREACH (const string& url, setMonitorBlocks)
        {
            boost::shared_ptr<CPOSTRequest> postRequest(new CPOSTRequest(url, postBody));
            vPOSTQueue.push_back(postRequest);
        }
    }
}

void ThreadHttpPost() {
    loop
    {
        // get work to be done
        vector<boost::shared_ptr<CPOSTRequest> > work;
        {
           LOCK(cs_vPOSTQueue);
           work = vPOSTQueue;
           vPOSTQueue.clear();
        }
        BOOST_FOREACH (boost::shared_ptr<CPOSTRequest> r, work)
            r->POST();

        boost::this_thread::interruption_point();
        if (vPOSTQueue.empty())
            MilliSleep(100);
    }
}

