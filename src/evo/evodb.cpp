// Copyright (c) 2018-2021 The Dash Core developers
// Copyright (c) 2021 The PIVX Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "evodb.h"

std::unique_ptr<CEvoDB> evoDb;

CEvoDB::CEvoDB(size_t nCacheSize, bool fMemory, bool fWipe) :
        db(fMemory ? "" : (GetDataDir() / "evodb"), nCacheSize, fMemory, fWipe),
        rootBatch(),
        rootDBTransaction(db, rootBatch),
        curDBTransaction(rootDBTransaction, rootDBTransaction)
{
}

bool CEvoDB::CommitRootTransaction()
{
    assert(curDBTransaction.IsClean());
    rootDBTransaction.Commit();
    bool ret = db.WriteBatch(rootBatch);
    rootBatch.Clear();
    return ret;
}

bool CEvoDB::VerifyBestBlock(const uint256& hash)
{
    uint256 hashBestBlock;
    return Read(EVODB_BEST_BLOCK, hashBestBlock) && hashBestBlock == hash;
}

void CEvoDB::WriteBestBlock(const uint256& hash)
{
    Write(EVODB_BEST_BLOCK, hash);
}
