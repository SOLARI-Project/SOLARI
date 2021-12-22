// Copyright (c) 2018-2020 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef PIVX_ZPIVCHAIN_H
#define PIVX_ZPIVCHAIN_H

#include "libzerocoin/CoinSpend.h"

class CBigNum;
class CTxIn;

bool IsSerialInBlockchain(const CBigNum& bnSerial, int& nHeightTx);
libzerocoin::CoinSpend TxInToZerocoinSpend(const CTxIn& txin);

#endif //PIVX_ZPIVCHAIN_H
