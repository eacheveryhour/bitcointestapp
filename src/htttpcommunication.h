#ifndef HTTTPCOMMUNICATION_H
#define HTTTPCOMMUNICATION_H

#include <jsonrpccpp/client.h>
#include <jsonrpccpp/client/connectors/httpclient.h>
#include <iostream>
#include <mutex>

#include <loggerinstances.h>

using namespace jsonrpc;

class HttpCommunication
{
public:

    // Separated password and login and endpoint string to CURL connection
    HttpCommunication(bool IsRegtest,
                      const std::string &Login = "hacker",
                      const std::string &Password = "qwerty")
    {
        std::string Endpoint{};

        if(IsRegtest)
        {
            Endpoint = "http://127.0.0.1:18444";
        }
        else
        {
            Endpoint = "http://127.0.0.1:8332";
        }

        m_CurlEndpoint = MakeCurlEndpoint(Endpoint, Login, Password);

        PLOG_VERBOSE_(HttpLogger) << "Http client starting with connection string: " << m_CurlEndpoint;

        InitLogger();
        Init();
    }

    // Do not forget to clean after us
    ~HttpCommunication()
    {
        if(Http) delete Http;
        if(Connector) delete Connector;
    }

    // Addition of a new address to out watchonly wallet to labeled group
    bool AddNewAddress(const std::string &NewAddress, const bool WithRescan)
    {
        std::string Method = "importaddress";
        Json::Value Parameters, Response; Parameters.append(NewAddress); Parameters.append("Imported"); Parameters.append(WithRescan);

        return CallMethod(Method, Parameters, Response);
    }

    bool GetCurrentBlockChainInfo(int &LastBlock)
    {
        return GetCurrentBlockCount(LastBlock);
    }

    bool GetCurrentBlockCount(int &Index)
    {
        std::string Method = "getblockcount";
        Json::Value Parameter, Response;
        Parameter = Json::arrayValue;

        if(CallMethod(Method, Parameter, Response))
        {
            Index = std::stoi(Response.asString());
            return true;
        }

        return false;
    }

    bool GetBlockHash(const std::string &BlockIndex, std::string &Hash)
    {
        std::string Method = "getblockhash";
        Json::Value Parameter, Response;
        Parameter = Json::arrayValue;
        Parameter.append(std::stoi(BlockIndex));

        if(CallMethod(Method, Parameter, Response))
        {
            Hash = Response.asString();
            return true;
        }

        return false;
    }

    bool GetBlockInfo(const std::string &BlockHash, Json::Value &BlockInfo)
    {
        std::string Method = "getblock";
        Json::Value Parameter = Json::arrayValue;
        Parameter.append(BlockHash);

        if(CallMethod(Method, Parameter, BlockInfo))
        {
            return true;
        }

        return false;
    }

    bool GetRawTxInfo(const std::string &TxId, Json::Value &TxInfo)
    {
        std::string Method = "getrawtransaction";
        Json::Value Parameter = Json::arrayValue;
        Parameter.append(TxId);
        Parameter.append(1);

        if(CallMethod(Method, Parameter, TxInfo))
        {
            return true;
        }

        return false;
    }


    // Possible bottleneck, as we have to wait for an answer from json-rpc server, witch may be very slow.
    // This will be called in background async thread, and must not affect on other calls to DB
    bool CallMethod(const std::string &Method, const Json::Value &Parameters, Json::Value &Response)
    {
        //Guard the transmission environment
        std::unique_lock<std::mutex> lock(m_TransmissionGuard);

        if (Connector)
        {
            try
            {
                Response = Connector->CallMethod(Method, Parameters);
                PLOG_VERBOSE_(HttpLogger) << "Json-RPC call: " << Method;
                return true;
            }
            catch (JsonRpcException &e)
            {
                PLOG_WARNING_(HttpLogger) << "Json-RPC call failed with error: " << e.what();
                return false;
            }
        }

        return false;
    }

private:

    std::mutex m_TransmissionGuard;

    // Making full connection enpoint for CURL, from three pieces;
    std::string MakeCurlEndpoint(const std::string &Endpoint,
                                 const std::string &Login,
                                 const std::string &Password)
    {
        const char *HttpPrefix = "http://";
        std::string::size_type Pos = Endpoint.find(HttpPrefix, 0);

        if(Pos != std::string::npos)
        {
            std::string ReturnValue = Endpoint;
            //Full prefix length without tailing \000
            ReturnValue.insert(Pos + sizeof (HttpPrefix) - 1, Login + ":" + Password + "@");
            return ReturnValue;
        }

        return "";
    }

    bool Init()
    {
        Http = new HttpClient(m_CurlEndpoint);

        if(Http)
        {
            PLOG_VERBOSE_(HttpLogger) << "Http client started successfuly on " << m_CurlEndpoint;

            Connector = new Client(*Http, JSONRPC_CLIENT_V1, false);

            if(Connector)
            {
                PLOG_VERBOSE_(HttpLogger) << "Http client connector started successfuly! ";

                return TestConnection();
            }
        }

        PLOG_FATAL_(HttpLogger) << "Http client init failed on: " << m_CurlEndpoint;

        return false;
    }

    void InitLogger()
    {
         plog::init<HttpLogger>(GLOBAL_LOG_SEVERITY, "http.log");
    }

    // Uptime rpc call is used to detect a connection status of startup.
    bool TestConnection()
    {
        Json::Value Params, Response;
        Params = Json::arrayValue;

        const std::string Ping = "uptime";

        try
        {
          Response = Connector->CallMethod(Ping, Params);

          PLOG_VERBOSE_(HttpLogger) << "Connection to bitcoind succed. Bitcoind uptime: " << Response << std::endl;
          return true;
        }
        catch (JsonRpcException &e)
        {
          PLOG_FATAL_(HttpLogger) << "Connection to bitcoind failed, check for bitcoind is running, and connection uri is valid. Error: " << e.what() << std::endl;
          return false;
        }
    }

private:

    HttpClient *Http = nullptr;
    Client *Connector = nullptr;

    std::string m_CurlEndpoint = "";
};

#endif // HTTTPCOMMUNICATION_H
