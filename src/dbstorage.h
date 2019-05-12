#ifndef DBSTORAGE_H
#define DBSTORAGE_H

#include <leveldb/db.h>
#include <leveldb/cache.h>
#include <leveldb/comparator.h>
#include <leveldb/write_batch.h>
#include <leveldb/iterator.h>

#include <loggerinstances.h>

#include <unordered_map>
#include <vector>
#include <memory>
#include <string>
#include <iostream>

using namespace leveldb;

struct TxInfo
{
    TxInfo()
    {
        m_LastScannedBlockNum = 0;
        m_Balance = -1;
    }

    TxInfo(int LastScannedBlockNum, int Balance)
    {
        m_LastScannedBlockNum = LastScannedBlockNum;
        m_Balance = Balance;
    }

    int m_LastScannedBlockNum;
    int m_Balance;
};

class DBStorage
{
public:

    DBStorage(const std::string &DBPath = "/tmp/", size_t DBCacheSize = 32)
    {
        m_DBPath = DBPath;
        m_DBCacheSize = DBCacheSize;

        InitDatabase();
        InitLogger();
    }

    ~DBStorage()
    {
        CloseDatabase();
    }
    // NO additional thread sync needed as for leveldb readme.

    // Adding an one address at once
    inline bool AddAddress(const std::string &Address)
    {
        Status Result;
        if(data) Result = data->Put(WriteOptions(), Slice(Address), Slice());

        PLOG_VERBOSE_IF_(DBLogger, Result.ok()) << "Added new address to database: " << Address;
        PLOG_WARNING_IF_(DBLogger, !Result.ok()) << "Error adding new address to database: " << Address;

        return Result.ok();
    }

    // Adding a bunch of addresses at a time (faster)
    inline bool AddAddresses(const std::vector<std::string> &Addresses)
    {
        Status Result;
        WriteBatch Batch;

        for(auto &Address : Addresses)
        {
            Batch.Put(Address, Slice());
        }

        if(data) Result = data->Write(WriteOptions(), &Batch);

        PLOG_VERBOSE_IF_(DBLogger, Result.ok()) << "Added new batch of addresses to database.";
        PLOG_WARNING_IF_(DBLogger, !Result.ok()) << "Error adding batch of addresses to database.";

        return Result.ok();
    }

    // Add/update an one TxInfo at a time
    inline bool UpdateTxInfo(const std::string &Address, const TxInfo &UpdatedInfo)
    {
        Status Result;

        if(data) Result = data->Put(WriteOptions(), Slice(Address), Slice(reinterpret_cast<const char*>(&UpdatedInfo), sizeof (UpdatedInfo)));

        PLOG_VERBOSE_IF_(DBLogger,Result.ok()) << "Added new txinfo to database: " << UpdatedInfo.m_LastScannedBlockNum;
        PLOG_WARNING_IF_(DBLogger,!Result.ok()) << "Error adding new txinfo to database: " << UpdatedInfo.m_LastScannedBlockNum;

        return Result.ok();
    }

    // Add a bunch of pairs address:TxInfo to DB (faster)
    inline bool UpdateTxInfos(const std::unordered_map<std::string, TxInfo> &UpdatedInfos)
    {
        Status Result;
        WriteBatch Batch;

        for(auto &Pair : UpdatedInfos)
        {
            Batch.Put(Pair.first, Slice(reinterpret_cast<const char*>(&Pair.second), sizeof (Pair.second)));
        }

        if(data) Result = data->Write(WriteOptions(), &Batch);

        PLOG_VERBOSE_IF_(DBLogger,Result.ok()) << "Added new batch of addresses and TxInfos to database.";
        PLOG_WARNING_IF_(DBLogger,!Result.ok()) << "Error adding batch of addresses and TxInfos to database.";

        return Result.ok();
    }

    inline bool GetTxInfo(const std::string &Address, TxInfo &Info) const
    {
        Status Result;
        std::string Data;
        if(data) Result = data->Get(ReadOptions(), Address, &Data);

        if(Result.ok())
        {
            memcpy((void*)&Info, (const void*)Data.data(), Data.size());
            return true;
        }

        return false;
    }

    // Iterate all saved addresses (may be slow on big database)
    bool GetAllAddresses(std::vector<std::string> &Addresses)
    {
        leveldb::Iterator *it = data->NewIterator(ReadOptions());

        for (it->SeekToFirst(); it->Valid(); it->Next())
        {
           Addresses.push_back(it->key().ToString());
        }

        delete it;

        if(Addresses.size() > 0)
        {
            return true;
        }
        else
        {
            return false;
        }
    }

    std::unique_ptr<leveldb::Iterator> GetDbIterator() const
    {
        return std::unique_ptr<leveldb::Iterator>(data->NewIterator(ReadOptions()));
    }

private:

    DB* data;
    size_t m_DBCacheSize = 0;

    void InitDatabase()
    {
        Options options;
        options.create_if_missing = true;
        options.compression = kNoCompression;

        if (m_DBCacheSize)
        {
            options.block_cache = NewLRUCache(m_DBCacheSize * 1048576);
        }

        Status status = DB::Open(options, m_DBPath + "data", &data);

        PLOG_VERBOSE_IF_(DBLogger, status.ok()) << "Database opened/created at: " << m_DBPath + "data";
        PLOG_WARNING_IF_(DBLogger, !status.ok()) << "Error opening/creating the database at: " << m_DBPath + "data";

        assert(status.ok());
    }

    void InitLogger()
    {
        plog::init<DBLogger>(GLOBAL_LOG_SEVERITY, "db.log");
    }

    void CloseDatabase()
    {
        delete data;
    }

    std::string m_DBPath = "";
};

#endif // DBSTORAGE_H
