// Copyright (c) 2017-2020 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "zpiv/zpos.h"

#include "validation.h"
#include "zpivchain.h"


/*
 * LEGACY: Kept for IBD in order to verify zerocoin stakes occurred when zPoS was active
 * Find the first occurrence of a certain accumulator checksum.
 * Return block index pointer or nullptr if not found
 */

uint32_t ParseAccChecksum(uint256 nCheckpoint, const libzerocoin::CoinDenomination denom)
{
    int pos = std::distance(libzerocoin::zerocoinDenomList.begin(),
            find(libzerocoin::zerocoinDenomList.begin(), libzerocoin::zerocoinDenomList.end(), denom));
    return (UintToArith256(nCheckpoint) >> (32*((libzerocoin::zerocoinDenomList.size() - 1) - pos))).Get32();
}

static const CBlockIndex* FindIndexFrom(uint32_t nChecksum, libzerocoin::CoinDenomination denom)
{
    // First look in the legacy database
    int nHeightChecksum = 0;
    if (zerocoinDB->ReadAccChecksum(nChecksum, denom, nHeightChecksum)) {
        return chainActive[nHeightChecksum];
    }

    // Not found. Scan the chain.
    const Consensus::Params& consensus = Params().GetConsensus();
    CBlockIndex* pindex = chainActive[consensus.vUpgrades[Consensus::UPGRADE_ZC].nActivationHeight];
    if (!pindex) return nullptr;
    while (pindex && pindex->nHeight <= consensus.height_last_ZC_AccumCheckpoint) {
        if (ParseAccChecksum(pindex->nAccumulatorCheckpoint, denom) == nChecksum) {
            // Found. Save to database and return
            zerocoinDB->WriteAccChecksum(nChecksum, denom, pindex->nHeight);
            return pindex;
        }
        //Skip forward in groups of 10 blocks since checkpoints only change every 10 blocks
        if (pindex->nHeight % 10 == 0) {
            pindex = chainActive[pindex->nHeight + 10];
            continue;
        }
        pindex = chainActive.Next(pindex);
    }
    return nullptr;
}

CLegacyZPivStake* CLegacyZPivStake::NewZPivStake(const CTxIn& txin)
{
    // Construct the stakeinput object
    if (!txin.IsZerocoinSpend()) {
        LogPrintf("%s: unable to initialize CLegacyZPivStake from non zc-spend", __func__);
        return nullptr;
    }

    // Check spend type
    libzerocoin::CoinSpend spend = TxInToZerocoinSpend(txin);
    if (spend.getSpendType() != libzerocoin::SpendType::STAKE) {
        LogPrintf("%s : spend is using the wrong SpendType (%d)", __func__, (int)spend.getSpendType());
        return nullptr;
    }

    uint32_t _nChecksum = spend.getAccumulatorChecksum();
    libzerocoin::CoinDenomination _denom = spend.getDenomination();
    const arith_uint256& nSerial = spend.getCoinSerialNumber().getuint256();
    const uint256& _hashSerial = Hash(nSerial.begin(), nSerial.end());

    // Find the pindex with the accumulator checksum
    const CBlockIndex* _pindexFrom = FindIndexFrom(_nChecksum, _denom);
    if (_pindexFrom == nullptr) {
        LogPrintf("%s : Failed to find the block index for zpiv stake origin", __func__);
        return nullptr;
    }

    // All good
    return new CLegacyZPivStake(_pindexFrom, _nChecksum, _denom, _hashSerial);
}

const CBlockIndex* CLegacyZPivStake::GetIndexFrom() const
{
    return pindexFrom;
}

CAmount CLegacyZPivStake::GetValue() const
{
    return denom * COIN;
}

CDataStream CLegacyZPivStake::GetUniqueness() const
{
    CDataStream ss(SER_GETHASH, 0);
    ss << hashSerial;
    return ss;
}

// Verify stake contextual checks
bool CLegacyZPivStake::ContextCheck(int nHeight, uint32_t nTime)
{
    const Consensus::Params& consensus = Params().GetConsensus();
    if (!consensus.NetworkUpgradeActive(nHeight, Consensus::UPGRADE_ZC_V2) || nHeight >= consensus.height_last_ZC_AccumCheckpoint)
        return error("%s : zPIV stake block: height %d outside range", __func__, nHeight);

    // The checkpoint needs to be from 200 blocks ago
    const int cpHeight = nHeight - 1 - consensus.ZC_MinStakeDepth;
    const libzerocoin::CoinDenomination denom = libzerocoin::AmountToZerocoinDenomination(GetValue());
    if (ParseAccChecksum(chainActive[cpHeight]->nAccumulatorCheckpoint, denom) != GetChecksum())
        return error("%s : accum. checksum at height %d is wrong.", __func__, nHeight);

    // All good
    return true;
}
