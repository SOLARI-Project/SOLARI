// Copyright (c) 2018-2021 The Dash Core developers
// Copyright (c) 2021 The PIVX Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "evodb.h"

std::unique_ptr<CEvoDB> evoDb;

CEvoDB::CEvoDB(size_t nCacheSize, bool fMemory, bool fWipe) :
        db(GetDataDir() / "evodb", nCacheSize, fMemory, fWipe),
        dbTransaction(db)
{
}
