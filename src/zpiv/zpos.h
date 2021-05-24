// Copyright (c) 2020 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef PIVX_LEGACY_ZPOS_H
#define PIVX_LEGACY_ZPOS_H

#include "stakeinput.h"
#include "txdb.h"

class CLegacyZPivStake : public CStakeInput
{
private:
    uint32_t nChecksum{0};
    libzerocoin::CoinDenomination denom{libzerocoin::ZQ_ERROR};
    uint256 hashSerial{UINT256_ZERO};

public:
    CLegacyZPivStake() : CStakeInput(nullptr) {}

    explicit CLegacyZPivStake(const libzerocoin::CoinSpend& spend);
    bool InitFromTxIn(const CTxIn& txin) override;
    bool IsZPIV() const override { return true; }
    uint32_t GetChecksum() const { return nChecksum; }
    const CBlockIndex* GetIndexFrom() const override;
    CAmount GetValue() const override;
    CDataStream GetUniqueness() const override;
    bool GetTxOutFrom(CTxOut& out) const override { return false; /* not available */ }
    virtual bool ContextCheck(int nHeight, uint32_t nTime) override;
};

// !TODO: remove from here and make static
const CBlockIndex* FindIndexFrom(uint32_t nChecksum, libzerocoin::CoinDenomination denom);

#endif //PIVX_LEGACY_ZPOS_H
