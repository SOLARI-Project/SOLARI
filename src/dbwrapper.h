// Copyright (c) 2012-2014 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_DBWRAPPER_H
#define BITCOIN_DBWRAPPER_H

#include "clientversion.h"
#include "fs.h"
#include "serialize.h"
#include "streams.h"
#include "util.h"
#include "version.h"

#include <typeindex>

#include <leveldb/db.h>
#include <leveldb/write_batch.h>


static const size_t DBWRAPPER_PREALLOC_KEY_SIZE = 64;
static const size_t DBWRAPPER_PREALLOC_VALUE_SIZE = 1024;


class dbwrapper_error : public std::runtime_error
{
public:
    dbwrapper_error(const std::string& msg) : std::runtime_error(msg) {}
};

class CDBWrapper;

/** These should be considered an implementation detail of the specific database.
 */
namespace dbwrapper_private {

/** Handle database error by throwing dbwrapper_error exception.
 */
void HandleError(const leveldb::Status& status);

};


/** Batch of changes queued to be written to a CDBWrapper */
class CDBBatch
{
    friend class CDBWrapper;

private:
    leveldb::WriteBatch batch;

    CDataStream ssKey;
    CDataStream ssValue;

    size_t size_estimate;

public:
    /**
     * @param[in] _parent   CDBWrapper that this batch is to be submitted to
     */
    CDBBatch() : ssKey(SER_DISK, CLIENT_VERSION), ssValue(SER_DISK, CLIENT_VERSION), size_estimate(0) { };

    void Clear()
    {
        batch.Clear();
        size_estimate = 0;
    }

    template <typename K, typename V>
    void Write(const K& key, const V& value)
    {
        CDataStream ssKey(SER_DISK, CLIENT_VERSION);
        ssKey.reserve(DBWRAPPER_PREALLOC_KEY_SIZE);
        ssKey << key;
        Write(ssKey, value);
        ssKey.clear();
    }

    template <typename V>
    void Write(const CDataStream& _ssKey, const V& value)
    {
        leveldb::Slice slKey(_ssKey.data(), _ssKey.size());

        CDataStream ssValue(SER_DISK, CLIENT_VERSION);
        ssValue.reserve(DBWRAPPER_PREALLOC_VALUE_SIZE);
        ssValue << value;
        leveldb::Slice slValue(ssValue.data(), ssValue.size());

        batch.Put(slKey, slValue);

        // LevelDB serializes writes as:
        // - byte: header
        // - varint: key length (1 byte up to 127B, 2 bytes up to 16383B, ...)
        // - byte[]: key
        // - varint: value length
        // - byte[]: value
        // The formula below assumes the key and value are both less than 16k.
        size_estimate += 3 + (slKey.size() > 127) + slKey.size() + (slValue.size() > 127) + slValue.size();
        ssValue.clear();
    }

    template <typename K>
    void Erase(const K& key)
    {
        CDataStream ssKey(SER_DISK, CLIENT_VERSION);
        ssKey.reserve(DBWRAPPER_PREALLOC_KEY_SIZE);
        ssKey << key;
        Erase(ssKey);
        ssKey.clear();
    }

    void Erase(const CDataStream& _ssKey)
    {
        leveldb::Slice slKey(_ssKey.data(), _ssKey.size());

        batch.Delete(slKey);

        // LevelDB serializes erases as:
        // - byte: header
        // - varint: key length
        // - byte[]: key
        // The formula below assumes the key is less than 16kB.
        size_estimate += 2 + (slKey.size() > 127) + slKey.size();
    }

    size_t SizeEstimate() const { return size_estimate; }
};

class CDBIterator
{
private:
    leveldb::Iterator *piter;

public:
    /**
     * @param[in] _piter            The original leveldb iterator.
     */
    CDBIterator(leveldb::Iterator *_piter) : piter(_piter) { };
    ~CDBIterator();

    bool Valid();

    void SeekToFirst();

    template<typename K> void Seek(const K& key) {
        CDataStream ssKey(SER_DISK, CLIENT_VERSION);
        ssKey.reserve(DBWRAPPER_PREALLOC_KEY_SIZE);
        ssKey << key;
        Seek(ssKey);
    }

    void Seek(const CDataStream& ssKey) {
        leveldb::Slice slKey(ssKey.data(), ssKey.size());
        piter->Seek(slKey);
    }

    void Next();

    template<typename K> bool GetKey(K& key) {
        leveldb::Slice slKey = piter->key();
        try {
            CDataStream ssKey(slKey.data(), slKey.data() + slKey.size(), SER_DISK, CLIENT_VERSION);
            ssKey >> key;
        } catch(const std::exception& e) {
            return false;
        }
        return true;
    }

    unsigned int GetKeySize() {
        return piter->key().size();
    }

    template<typename V> bool GetValue(V& value) {
        leveldb::Slice slValue = piter->value();
        try {
            CDataStream ssValue(slValue.data(), slValue.data() + slValue.size(), SER_DISK, CLIENT_VERSION);
            ssValue >> value;
        } catch(const std::exception& e) {
            return false;
        }
        return true;
    }

    unsigned int GetValueSize() {
        return piter->value().size();
    }

};

class CDBWrapper
{
private:
    //! custom environment this database is using (may be NULL in case of default environment)
    leveldb::Env* penv;

    //! database options used
    leveldb::Options options;

    //! options used when reading from the database
    leveldb::ReadOptions readoptions;

    //! options used when iterating over values of the database
    leveldb::ReadOptions iteroptions;

    //! options used when writing to the database
    leveldb::WriteOptions writeoptions;

    //! options used when sync writing to the database
    leveldb::WriteOptions syncoptions;

    //! the database itself
    leveldb::DB* pdb;

public:
    /**
     * @param[in] path        Location in the filesystem where leveldb data will be stored.
     * @param[in] nCacheSize  Configures various leveldb cache settings.
     * @param[in] fMemory     If true, use leveldb's memory environment.
     * @param[in] fWipe       If true, remove all existing data.
     */
    CDBWrapper(const fs::path& path, size_t nCacheSize, bool fMemory = false, bool fWipe = false);
    ~CDBWrapper();

    template <typename K>
    bool ReadDataStream(const K& key, CDataStream& ssValue) const
    {
        CDataStream ssKey(SER_DISK, CLIENT_VERSION);
        ssKey.reserve(DBWRAPPER_PREALLOC_KEY_SIZE);
        ssKey << key;
        return ReadDataStream(ssKey, ssValue);
    }

    bool ReadDataStream(const CDataStream& ssKey, CDataStream& ssValue) const
    {
        leveldb::Slice slKey(ssKey.data(), ssKey.size());

        std::string strValue;
        leveldb::Status status = pdb->Get(readoptions, slKey, &strValue);
        if (!status.ok()) {
            if (status.IsNotFound())
                return false;
            LogPrintf("LevelDB read failure: %s\n", status.ToString());
            dbwrapper_private::HandleError(status);
        }
        try {
            CDataStream ssValue(strValue.data(), strValue.data() + strValue.size(), SER_DISK, CLIENT_VERSION);
        } catch (const std::exception&) {
            return false;
        }
        return true;
    }

    template <typename K, typename V>
    bool Read(const K& key, V& value) const
    {
        CDataStream ssKey(SER_DISK, CLIENT_VERSION);
        ssKey.reserve(DBWRAPPER_PREALLOC_KEY_SIZE);
        ssKey << key;
        return Read(ssKey, value);
    }

    template <typename V>
    bool Read(const CDataStream& ssKey, V& value) const
    {
        CDataStream ssValue(SER_DISK, CLIENT_VERSION);
        if (!ReadDataStream(ssKey, ssValue)) {
            return false;
        }
        try {
            ssValue >> value;
        } catch (const std::exception&) {
            return false;
        }
        return true;
    }

    template <typename K, typename V>
    bool Write(const K& key, const V& value, bool fSync = false)
    {
        CDBBatch batch;
        batch.Write(key, value);
        return WriteBatch(batch, fSync);
    }

    template <typename K>
    bool Exists(const K& key) const
    {
        CDataStream ssKey(SER_DISK, CLIENT_VERSION);
        ssKey.reserve(DBWRAPPER_PREALLOC_KEY_SIZE);
        ssKey << key;
        return Exists(ssKey);
    }

    bool Exists(const CDataStream& key) const
    {
        leveldb::Slice slKey(key.data(), key.size());

        std::string strValue;
        leveldb::Status status = pdb->Get(readoptions, slKey, &strValue);
        if (!status.ok()) {
            if (status.IsNotFound())
                return false;
            LogPrintf("LevelDB read failure: %s\n", status.ToString());
            dbwrapper_private::HandleError(status);
        }
        return true;
    }

    template <typename K>
    bool Erase(const K& key, bool fSync = false)
    {
        CDBBatch batch;
        batch.Erase(key);
        return WriteBatch(batch, fSync);
    }

    bool WriteBatch(CDBBatch& batch, bool fSync = false);

    // not available for LevelDB; provide for compatibility with BDB
    bool Flush()
    {
        return true;
    }

    bool Sync()
    {
        CDBBatch batch;
        return WriteBatch(batch, true);
    }

    // not exactly clean encapsulation, but it's easiest for now
    CDBIterator* NewIterator()
    {
        return new CDBIterator(pdb->NewIterator(iteroptions));
    }

   /**
    * Return true if the database managed by this class contains no entries.
    */
    bool IsEmpty();

    template<typename K>
    size_t EstimateSize(const K& key_begin, const K& key_end) const
    {
        CDataStream ssKey1(SER_DISK, CLIENT_VERSION), ssKey2(SER_DISK, CLIENT_VERSION);
        ssKey1.reserve(DBWRAPPER_PREALLOC_KEY_SIZE);
        ssKey2.reserve(DBWRAPPER_PREALLOC_KEY_SIZE);
        ssKey1 << key_begin;
        ssKey2 << key_end;
        leveldb::Slice slKey1(&ssKey1[0], ssKey1.size());
        leveldb::Slice slKey2(&ssKey2[0], ssKey2.size());
        uint64_t size = 0;
        leveldb::Range range(slKey1, slKey2);
        pdb->GetApproximateSizes(&range, 1, &size);
        return size;
    }

};

template<typename Parent, typename CommitTarget>
class CDBTransaction {
protected:
    Parent &parent;
    CommitTarget &commitTarget;

    struct DataStreamCmp {
        static bool less(const CDataStream& a, const CDataStream& b) {
            return std::lexicographical_compare(a.begin(), a.end(), b.begin(), b.end());
        }
        bool operator()(const CDataStream& a, const CDataStream& b) const {
            return less(a, b);
        }
    };

    struct ValueHolder {
        virtual ~ValueHolder() = default;
        virtual void Write(const CDataStream& ssKey, CommitTarget &parent) = 0;
    };
    typedef std::unique_ptr<ValueHolder> ValueHolderPtr;

    template <typename V>
    struct ValueHolderImpl : ValueHolder {
        ValueHolderImpl(const V &_value) : value(_value) { }

        virtual void Write(const CDataStream& ssKey, CommitTarget &commitTarget) {
            // we're moving the value instead of copying it. This means that Write() can only be called once per
            // ValueHolderImpl instance. Commit() clears the write maps, so this ok.
            commitTarget.Write(ssKey, std::move(value));
        }
        V value;
    };

    template<typename K>
    static CDataStream KeyToDataStream(const K& key) {
        CDataStream ssKey(SER_DISK, CLIENT_VERSION);
        ssKey.reserve(DBWRAPPER_PREALLOC_KEY_SIZE);
        ssKey << key;
        return ssKey;
    }

    typedef std::map<CDataStream, ValueHolderPtr, DataStreamCmp> WritesMap;
    typedef std::set<CDataStream, DataStreamCmp> DeletesSet;

    WritesMap writes;
    DeletesSet deletes;

public:
    CDBTransaction(Parent &_parent, CommitTarget &_commitTarget) : parent(_parent), commitTarget(_commitTarget) {}

    template <typename K, typename V>
    void Write(const K& key, const V& v) {
        Write(KeyToDataStream(key), v);
    }

    template <typename V>
    void Write(const CDataStream& ssKey, const V& v) {
        deletes.erase(ssKey);
        auto it = writes.emplace(ssKey, nullptr).first;
        it->second = std::make_unique<ValueHolderImpl<V>>(v);
    }

    template <typename K, typename V>
    bool Read(const K& key, V& value) {
        return Read(KeyToDataStream(key), value);
    }

    template <typename V>
    bool Read(const CDataStream& ssKey, V& value) {
        if (deletes.count(ssKey)) {
            return false;
        }

        auto it = writes.find(ssKey);
        if (it != writes.end()) {
            auto *impl = dynamic_cast<ValueHolderImpl<V> *>(it->second.get());
            if (!impl) {
                throw std::runtime_error("Read called with V != previously written type");
            }
            value = impl->value;
            return true;
        }

        return parent.Read(ssKey, value);
    }

    template <typename K>
    bool Exists(const K& key) {
        return Exists(KeyToDataStream(key));
    }

    bool Exists(const CDataStream& ssKey) {
        if (deletes.count(ssKey)) {
            return false;
        }

        if (writes.count(ssKey)) {
            return true;
        }

        return parent.Exists(ssKey);
    }

    template <typename K>
    void Erase(const K& key) {
        return Erase(KeyToDataStream(key));
    }

    void Erase(const CDataStream& ssKey) {
        writes.erase(ssKey);
        deletes.emplace(ssKey);
    }

    void Clear() {
        writes.clear();
        deletes.clear();
    }

    void Commit() {
        for (const auto &k : deletes) {
            commitTarget.Erase(k);
        }
        for (auto &p : writes) {
            p.second->Write(p.first, commitTarget);
        }
        Clear();
    }

    bool IsClean() {
        return writes.empty() && deletes.empty();
    }
};

#endif // BITCOIN_DBWRAPPER_H
