// Copyright (c) 2020-2021 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php.

#include "test/test_pivx.h"
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

BOOST_AUTO_TEST_SUITE_END()
