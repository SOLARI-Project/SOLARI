// Copyright (c) 2018-2020 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "zpivchain.h"
#include "zpiv/zpivmodule.h"

libzerocoin::CoinSpend TxInToZerocoinSpend(const CTxIn& txin)
{
    CDataStream serializedCoinSpend = ZPIVModule::ScriptSigToSerializedSpend(txin.scriptSig);
    return libzerocoin::CoinSpend(serializedCoinSpend);
}


