// Copyright (c) 2018-2020 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "zpivchain.h"

#include "txdb.h"
#include "validation.h"
#include "zpiv/zpivmodule.h"

static bool IsTransactionInChain(const uint256& txId, int& nHeightTx)
{
    CTransactionRef tx;
    uint256 hashBlock;
    if (!GetTransaction(txId, tx, hashBlock, true))
        return false;

    if (hashBlock.IsNull() || !mapBlockIndex.count(hashBlock)) {
        return false;
    }

    CBlockIndex* pindex = mapBlockIndex[hashBlock];
    if (!chainActive.Contains(pindex)) {
        return false;
    }

    nHeightTx = pindex->nHeight;
    return true;
}

bool IsSerialInBlockchain(const CBigNum& bnSerial, int& nHeightTx)
{
    uint256 txHash;
    // if not in zerocoinDB then its not in the blockchain
    if (!zerocoinDB->ReadCoinSpend(bnSerial, txHash))
        return false;

    return IsTransactionInChain(txHash, nHeightTx);
}

libzerocoin::CoinSpend TxInToZerocoinSpend(const CTxIn& txin)
{
    CDataStream serializedCoinSpend = ZPIVModule::ScriptSigToSerializedSpend(txin.scriptSig);
    return libzerocoin::CoinSpend(serializedCoinSpend);
}


