#ifndef HTTPPOST_H
#define HTTPPOST_H


CCriticalSection cs_mapMonitored;
CCriticalSection cs_vPOSTQueue;
set<string> setMonitorTx;
set<string> setMonitorBlocks;
class CPOSTRequest;
static vector<boost::shared_ptr<CPOSTRequest> > vPOSTQueue;
bool fMonitorAllTx;

// forward declaration, implementation in bitcoinrpc.cpp
string HTTPPost(const string& host, const string& path, const string& strMsg,
                const map<string,string>& mapRequestHeaders);
int ReadHTTPMessage(std::basic_istream<char>& stream, map<string,
                    string>& mapHeadersRet, string& strMessageRet,
                    int nProto);

void TxToJSON(const CTransaction&, const uint256, Object&);
Object blockToJSON(const CBlock&, const CBlockIndex*);
string JSONRPCRequest(const string&, const Array&, const Value&);

class CPOSTRequest
{
public:
    CPOSTRequest(const string &_url, const string& _body) : url(_url), body(_body)
    {
    }

    virtual bool POST()
    {
        using namespace boost::xpressive;
        // This regex is wrong for IPv6 urls; see http://www.ietf.org/rfc/rfc2732.txt
        //  (they're weird; e.g  "http://[::FFFF:129.144.52.38]:80/index.html" )
        // I can live with non-raw-IPv6 urls for now...
        static sregex url_regex = sregex::compile("^(http|https)://([^:/]+)(:[0-9]{1,5})?(.*)$");

        boost::xpressive::smatch urlparts;
        if (!regex_match(url, urlparts, url_regex))
        {
            printf("URL PARSING FAILED: %s\n", url.c_str());
            return true;
        }
        string protocol = urlparts[1];
        string host = urlparts[2];
        string s_port = urlparts[3];  // Note: includes colon, e.g. ":8080"
        bool fSSL = (protocol == "https" ? true : false);
        int port = (fSSL ? 443 : 80);
        if (s_port.size() > 1) { port = atoi(s_port.c_str()+1); }
        string path = urlparts[4];
        map<string, string> headers;

#ifdef USE_SSL
        io_service io_service;
        ssl::context context(io_service, ssl::context::sslv23);
        context.set_options(ssl::context::no_sslv2);
        SSLStream sslStream(io_service, context);
        SSLIOStreamDevice d(sslStream, fSSL);
        boost::iostreams::stream<SSLIOStreamDevice> stream(d);
        if (!d.connect(host, boost::lexical_cast<string>(port)))
        {
            printf("POST: Couldn't connect to %s:%d", host.c_str(), port);
            return false;
        }
#else
        if (fSSL)
        {
            printf("Cannot POST to SSL server, bitcoin compiled without full openssl libraries.");
            return false;
        }
        ip::tcp::iostream stream(host, boost::lexical_cast<string>(port));
#endif

        stream << HTTPPost(host, path, body, headers) << std::flush;
        map<string, string> mapResponseHeaders;
        string strReply;
        int httpProto = 0;
        int status = ReadHTTPMessage(stream, mapResponseHeaders, strReply, httpProto);
//        printf(" HTTP response %d: %s\n", status, strReply.c_str());

        return (status < 300);
    }

protected:
    string url;
    string body;
};


void monitorTx(const CTransaction& tx);
void monitorBlock(const CBlock&block, const CBlockIndex*);

#endif // HTTPPOST_H
