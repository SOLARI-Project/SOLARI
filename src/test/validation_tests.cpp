// Copyright (c) 2020-2021 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php.

#include "test/test_pivx.h"
#include "blockassembler.h"
#include "consensus/merkle.h"
#include "primitives/transaction.h"
#include "sapling/sapling_validation.h"
#include "test/librust/utiltest.h"

#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(validation_tests, TestingSetup)

void test_simple_sapling_invalidity(CMutableTransaction& tx)
{
    CAmount nDummyValueOut;
    {
        CMutableTransaction newTx(tx);
        CValidationState state;

        BOOST_CHECK(!CheckTransaction(newTx, state, false));
        BOOST_CHECK(state.GetRejectReason() == "bad-txns-vin-empty");
    }
    {
        CMutableTransaction newTx(tx);
        CValidationState state;

        newTx.sapData->vShieldedSpend.emplace_back();
        newTx.sapData->vShieldedSpend[0].nullifier = GetRandHash();

        BOOST_CHECK(!CheckTransaction(newTx, state, false));
        BOOST_CHECK(state.GetRejectReason() == "bad-txns-vout-empty");
    }
    {
        // Ensure that nullifiers are never duplicated within a transaction.
        CMutableTransaction newTx(tx);
        CValidationState state;

        newTx.sapData->vShieldedSpend.emplace_back();
        newTx.sapData->vShieldedSpend[0].nullifier = GetRandHash();

        newTx.sapData->vShieldedOutput.emplace_back();

        newTx.sapData->vShieldedSpend.emplace_back();
        newTx.sapData->vShieldedSpend[1].nullifier = newTx.sapData->vShieldedSpend[0].nullifier;

        BOOST_CHECK(!SaplingValidation::CheckTransactionWithoutProofVerification(newTx, state, nDummyValueOut));
        BOOST_CHECK(state.GetRejectReason() == "bad-spend-description-nullifiers-duplicate");

        newTx.sapData->vShieldedSpend[1].nullifier = GetRandHash();

        BOOST_CHECK(SaplingValidation::CheckTransactionWithoutProofVerification(newTx, state, nDummyValueOut));
    }
    {
        CMutableTransaction newTx(tx);
        CValidationState state;

        // Create a coinbase transaction
        CTxIn vin;
        vin.prevout = COutPoint();
        newTx.vin.emplace_back(vin);
        CTxOut vout;
        vout.nValue = 2;
        newTx.vout.emplace_back(vout);

        newTx.sapData->vShieldedSpend.emplace_back();

        BOOST_CHECK(!CheckTransaction(newTx, state, false));
        BOOST_CHECK(state.GetRejectReason() == "bad-txns-invalid-sapling");
    }
    {
        CMutableTransaction newTx(tx);
        CValidationState state;

        // Create a coinstake transaction
        CTxIn vin;
        vin.prevout = COutPoint(UINT256_ZERO, 0);
        newTx.vin.emplace_back(vin);
        CTxOut vout;
        vout.nValue = 0;
        newTx.vout.emplace_back(vout);
        vout.nValue = 2;
        newTx.vout.emplace_back(vout);

        newTx.sapData->vShieldedSpend.emplace_back();

        BOOST_CHECK(!CheckTransaction(newTx, state, false));
        BOOST_CHECK(state.GetRejectReason() == "bad-txns-invalid-sapling");
    }
}

BOOST_AUTO_TEST_CASE(test_simple_shielded_invalid)
{
    // Switch to regtest parameters so we can activate Sapling
    SelectParams(CBaseChainParams::REGTEST);

    CMutableTransaction mtx;
    mtx.nVersion = CTransaction::TxVersion::SAPLING;

    UpdateNetworkUpgradeParameters(Consensus::UPGRADE_V5_0, Consensus::NetworkUpgrade::ALWAYS_ACTIVE);
    test_simple_sapling_invalidity(mtx);
    UpdateNetworkUpgradeParameters(Consensus::UPGRADE_V5_0, Consensus::NetworkUpgrade::NO_ACTIVATION_HEIGHT);

    // Switch back to mainnet parameters as originally selected in test fixture
    SelectParams(CBaseChainParams::MAIN);
}

void CheckBlockZcRejection(const std::shared_ptr<CBlock>& pblock, CMutableTransaction& mtx)
{
    pblock->vtx.emplace_back(MakeTransactionRef(mtx));
    pblock->hashMerkleRoot = BlockMerkleRoot(*pblock);
    CValidationState state;
    BOOST_CHECK(!ProcessNewBlock(state, nullptr, pblock, nullptr));
    BOOST_CHECK(!state.IsValid());
    BOOST_CHECK_EQUAL(state.GetRejectReason(), "bad-blk-with-zc");
}

void CheckMempoolZcRejection(CMutableTransaction& mtx)
{
    LOCK(cs_main);
    CValidationState state;
    BOOST_CHECK(!AcceptToMemoryPool(
            mempool, state, MakeTransactionRef(mtx), true, nullptr, false, true));
    BOOST_CHECK(!state.IsValid());
    BOOST_CHECK_EQUAL(state.GetRejectReason(), "bad-tx-with-zc");
}

BOOST_AUTO_TEST_CASE(zerocoin_rejection_tests)
{
    SelectParams(CBaseChainParams::REGTEST);
    UpdateNetworkUpgradeParameters(Consensus::UPGRADE_V5_0, Consensus::NetworkUpgrade::ALWAYS_ACTIVE);
    const CChainParams& chainparams = Params();

    std::unique_ptr<CBlockTemplate> pblocktemplate;
    CScript scriptPubKey = CScript() << OP_DUP << OP_HASH160 << ParseHex("8d5b4f83212214d6ef693e02e6d71969fddad976") << OP_EQUALVERIFY << OP_CHECKSIG;
    BOOST_CHECK(pblocktemplate = BlockAssembler(Params(), false).CreateNewBlock(scriptPubKey, pwalletMain, false));
    pblocktemplate->block.hashPrevBlock = chainparams.GetConsensus().hashGenesisBlock;

    // Base tx
    CMutableTransaction mtx;
    CTxIn vin;
    vin.prevout = COutPoint(UINT256_ZERO, 0);
    mtx.vin.emplace_back(vin);

    // Zerocoin mints rejection test
    mtx.vout.emplace_back();
    mtx.vout[0].scriptPubKey = CScript() << OP_ZEROCOINMINT <<
                                         CBigNum::randBignum(chainparams.GetConsensus().Zerocoin_Params(false)->coinCommitmentGroup.groupOrder).getvch();
    mtx.vout[0].nValue = 1 * COIN;
    CheckBlockZcRejection(std::make_shared<CBlock>(pblocktemplate->block), mtx);
    CheckMempoolZcRejection(mtx);

    // Zerocoin spends rejection test
    mtx.vout[0].scriptPubKey = scriptPubKey;
    mtx.vin[0].scriptSig = CScript() << OP_ZEROCOINSPEND;
    CheckBlockZcRejection(std::make_shared<CBlock>(pblocktemplate->block), mtx);
    CheckMempoolZcRejection(mtx);

    // Zerocoin public spends rejection test
    mtx.vin[0].scriptSig = CScript() << OP_ZEROCOINPUBLICSPEND;
    CheckBlockZcRejection(std::make_shared<CBlock>(pblocktemplate->block), mtx);
    CheckMempoolZcRejection(mtx);
}

BOOST_AUTO_TEST_SUITE_END()
