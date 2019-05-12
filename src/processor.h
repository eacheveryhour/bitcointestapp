#ifndef PROCESSOR_H
#define PROCESSOR_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <syslog.h>

#include <memory>

#include <dbstorage.h>
#include <htttpcommunication.h>
#include <pipecommunication.h>
#include <timer.h>

#include <btc/btc.h>
#include <btc/tool.h>
#include <btc/chainparams.h>
#include <btc/base58.h>
#include <btc/bip32.h>
#include <btc/ecc.h>
#include <btc/ecc_key.h>
#include <btc/random.h>
#include <btc/serialize.h>
#include <btc/tx.h>
#include <btc/utils.h>

#include <loggerinstances.h>

static const btc_chainparams* currentchain = &btc_chainparams_main;

struct StartUpParameters
{
    bool IsRegtest = false;
    std::string DatabaseLocation{};
    std::string RpcLogin{};
    std::string RpcPassword{};
    std::string CurlEndpoint{};
    std::string XpubAddress{};
};

//Standart demonize example, not all signals handled, but ok
static void Daemonize()
{
    pid_t pid;

    /* Fork off the parent process */
    pid = fork();

    /* An error occurred */
    if (pid < 0)
        exit(EXIT_FAILURE);

    /* Success: Let the parent terminate */
    if (pid > 0)
        exit(EXIT_SUCCESS);

    /* On success: The child process becomes session leader */
    if (setsid() < 0)
        exit(EXIT_FAILURE);

    /* Catch, ignore and handle signals */
    //TODO: Implement a working signal handler */
    signal(SIGCHLD, SIG_IGN);
    signal(SIGHUP, SIG_IGN);

    /* Fork off for the second time*/
    pid = fork();

    /* An error occurred */
    if (pid < 0)
        exit(EXIT_FAILURE);

    /* Success: Let the parent terminate */
    if (pid > 0)
        exit(EXIT_SUCCESS);

    /* Set new file permissions */
    umask(0);

    /* Change the working directory to the root directory */
    /* or another appropriated directory */
    chdir("/tmp/");

    /* Close all open file descriptors */
    int x;
    for (x = sysconf(_SC_OPEN_MAX); x>=0; x--)
    {
        close (x);
    }
}

class Processor
{
public:

    Processor(const StartUpParameters &Params) : m_XpubAddress(Params.XpubAddress)
    {
        PLOG_VERBOSE_(MainLogger) << "Processor init!";

        Daemonize();
        InitLogger();
        Init(Params);
        MainLoop();
    }

    ~Processor()
    {
        Dispose();
    }

private:

    void MainLoop()
    {
        while(true)
        {
            ExecutePipeCommands();
        }
    }

    void ExecutePipeCommands()
    {
        assert(m_PipeCommunication);

        std::queue<PipeCommand> Commands;
        PipeCommand Command;
        m_PipeCommunication->GetAllRecieved(Commands);

        while(Commands.size() > 0)
        {
            Command = Commands.front();

            if(StrToLower(Command.GetCommand()) == "generateaddress")
            {
                const std::string NewHdAddress = GenerateNewHdAddress();
                const std::string NewRawAddress = ExtractRawAddressFromHd(NewHdAddress);

                PLOG_VERBOSE_(MainLogger) << "New derived HD: " << NewHdAddress;
                PLOG_VERBOSE_(MainLogger) << "New raw address: " << NewRawAddress;

                TxInfo CurrentInfo = GetCurrentBlockChainInfo();

                AddNewAddressToDatabase(NewRawAddress, CurrentInfo);
                AddNewAddressToBitcoind(NewRawAddress);
            }
            else if(StrToLower(Command.GetCommand()) == "getbalance")
            {
                int Balance = 0;
                if(GetBalance(Command.GetParameter(), Balance))
                {
                    SendBalanceToOutPipe(Command.GetParameter(), Balance);
                }
            }

            Commands.pop();
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    std::string GenerateNewHdAddress()
    {
        std::string GeneratedHdAddress{};
        //Max HD address length, for not nulling this string from garbage symbols tailing \000
        char GeneratedHd[112];

        //Just an m/i depth
        std::string KeyPath = "m/" + std::to_string(CurrentGenerationDepth);

        if (hd_derive(currentchain, m_XpubAddress.c_str(), KeyPath.c_str(), GeneratedHd, sizeof (GeneratedHd)))
        {
            GeneratedHdAddress = GeneratedHd;
        }
        CurrentGenerationDepth++;

        return GeneratedHdAddress;
    }

    std::string ExtractRawAddressFromHd(const std::string &HdAddress)
    {
        std::string ExtractedAddress{};
        //Max bitcoin raw address length, for not nulling this string from garbage symbols tailing \000
        char ExtractedStr[35];

        btc_hdnode node;

        if (btc_hdnode_deserialize(HdAddress.c_str(), currentchain, &node))
        {
            btc_hdnode_get_p2pkh_address(&node, currentchain, ExtractedStr, sizeof(ExtractedStr));
            ExtractedAddress = ExtractedStr;
        }

        return ExtractedAddress;
    }

    TxInfo GetCurrentBlockChainInfo() const
    {
        assert(m_HttpCommunication);

        TxInfo ReturnValue;
        m_HttpCommunication->GetCurrentBlockChainInfo(ReturnValue.m_LastScannedBlockNum);

        return ReturnValue;
    }

    void AddNewAddressToDatabase(const std::string &NewAddress, TxInfo &CurrentTxInfo)
    {
        assert(m_DBStorage);

        PLOG_VERBOSE_(MainLogger) << "Adding new address to DB: " << NewAddress;
        m_DBStorage->UpdateTxInfo(NewAddress, CurrentTxInfo);
    }

    void AddNewAddressToBitcoind(const std::string &NewAddress)
    {
        assert(m_HttpCommunication);

        PLOG_VERBOSE_(MainLogger) << "Adding new address to bitcoind: " << NewAddress;
        m_HttpCommunication->AddNewAddress(NewAddress, BlockChainRescanNeeded);
    }

    bool GetBalance(const std::string &OnAddress, int &Balance)
    {
        assert(m_DBStorage);

        TxInfo Info;

        if(m_DBStorage->GetTxInfo(OnAddress, Info))
        {
            PLOG_VERBOSE_(MainLogger) << "Found balance on address: " << OnAddress;
            Balance = Info.m_Balance;
            return true;
        }

        return false;
    }

    void SendBalanceToOutPipe(const std::string &Address, const int &Balance)
    {
        assert(m_PipeCommunication);

        m_PipeCommunication->SendMessage("[ Address: " + Address + " < > " + "Balance: " + std::to_string(Balance) + " ]");
    }

    void UpdateDatabase()
    {
        assert(m_DBStorage);
        assert(m_HttpCommunication);

        const TxInfo *IterationInfo;
        const std::string *IterationAddress;
        std::string BlockHash;
        Json::Value BlockInfoJson, TxInfoJson;
        int CurrentBlockCountInt = 0, LastSavedBlockNum = 0, Balance = 0;

        m_HttpCommunication->GetCurrentBlockCount(CurrentBlockCountInt);

        std::unordered_map<std::string, TxInfo> Updated;
        std::vector<std::string> Addresses;
        std::unique_ptr<leveldb::Iterator> DBIterator = m_DBStorage->GetDbIterator();

        //For each table entry
        for (DBIterator->SeekToFirst(); DBIterator->Valid(); DBIterator->Next())
        {
           //Current key
           IterationAddress = reinterpret_cast<const std::string*>(DBIterator->key().data());
           //Current value
           IterationInfo = reinterpret_cast<const TxInfo*>(DBIterator->value().data());

           if(IterationInfo && IterationAddress)
           {
               LastSavedBlockNum = IterationInfo->m_LastScannedBlockNum;

               //From last saved block num, to current local block num
               for(int StartBlock = LastSavedBlockNum; StartBlock < CurrentBlockCountInt; ++StartBlock)
               {
                   //Getting block hash
                   if(m_HttpCommunication->GetBlockHash(std::to_string(StartBlock), BlockHash))
                   {
                       //Getting full block info
                        if(m_HttpCommunication->GetBlockInfo(BlockHash, BlockInfoJson))
                        {
                            //Parse block info json from root, at tx array
                            for(auto &Tx : BlockInfoJson["tx"])
                            {
                                //Get full transaction info json
                                if(m_HttpCommunication->GetRawTxInfo(Tx.asString(), TxInfoJson))
                                {
                                    //Not tested as for -txindex=1 consumed all space on debian virtual machine :D
                                    Json::Value VoutRoot = BlockInfoJson["vout"];

                                    for(auto &Address : VoutRoot["addresses"])
                                    {
                                        if(Address == *IterationAddress)
                                        {
                                            Json::Value Amount = VoutRoot["amount"];
                                            Balance += Amount.asInt();
                                        }
                                    }
                                }
                            }
                        }
                   }
               }
           }

        Updated.insert(std::pair<std::string, TxInfo>(*IterationAddress, TxInfo(CurrentBlockCountInt, Balance)));

        }

        PLOG_VERBOSE_(MainLogger) << "Async DB update called, num of records:" << Addresses.size();
        m_DBStorage->UpdateTxInfos(Updated);
    }

    void Init(const StartUpParameters &Params)
    {
       if(Params.IsRegtest) currentchain = &btc_chainparams_regtest;
       m_DBStorage = new DBStorage(Params.DatabaseLocation);
       m_HttpCommunication = new HttpCommunication(Params.IsRegtest, Params.RpcLogin, Params.RpcPassword);
       m_PipeCommunication = new PipeCommunication();
    }

    void InitLogger()
    {
        plog::init<MainLogger>(GLOBAL_LOG_SEVERITY, "main.log");
    }

    void Dispose()
    {
        if(m_DBStorage) delete m_DBStorage;
        if(m_HttpCommunication) delete m_HttpCommunication;
        if(m_PipeCommunication) delete m_PipeCommunication;
    }

private:

    DBStorage *m_DBStorage = nullptr;
    HttpCommunication *m_HttpCommunication = nullptr;
    PipeCommunication *m_PipeCommunication = nullptr;

    Timer DBUpdater{std::chrono::seconds{60}, std::bind(&Processor::UpdateDatabase, this), true, true};

    std::string m_XpubAddress{};

    bool BlockChainRescanNeeded = false;
    uint32_t CurrentGenerationDepth = 0;
};

#endif // PROCESSOR_H
