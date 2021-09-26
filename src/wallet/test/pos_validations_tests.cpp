// Copyright (c) 2020 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php.

#include "wallet/test/pos_test_fixture.h"

#include "blockassembler.h"
#include "coincontrol.h"
#include "util/blockstatecatcher.h"
#include "blocksignature.h"
#include "consensus/merkle.h"
#include "primitives/block.h"
#include "script/sign.h"
#include "test/util/blocksutil.h"
#include "util/blockstatecatcher.h"
#include "wallet/wallet.h"

#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_SUITE(pos_validations_tests)

void reSignTx(CMutableTransaction& mtx,
              const std::vector<CTxOut>& txPrevOutputs,
              CWallet* wallet)
{
    CTransaction txNewConst(mtx);
    for (int index=0; index < (int) txPrevOutputs.size(); index++) {
        const CTxOut& prevOut = txPrevOutputs.at(index);
        SignatureData sigdata;
        BOOST_ASSERT(ProduceSignature(
                TransactionSignatureCreator(wallet, &txNewConst, index, prevOut.nValue, SIGHASH_ALL),
                prevOut.scriptPubKey,
                sigdata,
                txNewConst.GetRequiredSigVersion(),
                true));
        UpdateTransaction(mtx, index, sigdata);
    }
}

BOOST_FIXTURE_TEST_CASE(coinstake_tests, TestPoSChainSetup)
{
    // Verify that we are at block 251
    BOOST_CHECK_EQUAL(WITH_LOCK(cs_main, return chainActive.Tip()->nHeight), 250);
    SyncWithValidationInterfaceQueue();

    // Let's create the block
    std::vector<CStakeableOutput> availableCoins;
    BOOST_CHECK(pwalletMain->StakeableCoins(&availableCoins));
    std::unique_ptr<CBlockTemplate> pblocktemplate = BlockAssembler(
            Params(), false).CreateNewBlock(CScript(),
                                            pwalletMain.get(),
                                            true,
                                            &availableCoins,
                                            true);
    std::shared_ptr<CBlock> pblock = std::make_shared<CBlock>(pblocktemplate->block);
    BOOST_CHECK(pblock->IsProofOfStake());

    // Add a second input to a coinstake
    CMutableTransaction mtx(*pblock->vtx[1]);
    const CStakeableOutput& in2 = availableCoins.back();
    availableCoins.pop_back();
    CTxIn vin2(in2.tx->GetHash(), in2.i);
    mtx.vin.emplace_back(vin2);

    CTxOut prevOutput1 = pwalletMain->GetWalletTx(mtx.vin[0].prevout.hash)->tx->vout[mtx.vin[0].prevout.n];
    std::vector<CTxOut> txPrevOutputs{prevOutput1, in2.tx->tx->vout[in2.i]};

    reSignTx(mtx, txPrevOutputs, pwalletMain.get());
    pblock->vtx[1] = MakeTransactionRef(mtx);
    pblock->hashMerkleRoot = BlockMerkleRoot(*pblock);
    BOOST_CHECK(SignBlock(*pblock, *pwalletMain));
    ProcessBlockAndCheckRejectionReason(pblock, "bad-cs-multi-inputs", 250);

    // Check multi-empty-outputs now
    pblock = std::make_shared<CBlock>(pblocktemplate->block);
    mtx = CMutableTransaction(*pblock->vtx[1]);
    for (int i = 0; i < 999; ++i) {
        mtx.vout.emplace_back();
        mtx.vout.back().SetEmpty();
    }
    reSignTx(mtx, {prevOutput1}, pwalletMain.get());
    pblock->vtx[1] = MakeTransactionRef(mtx);
    pblock->hashMerkleRoot = BlockMerkleRoot(*pblock);
    BOOST_CHECK(SignBlock(*pblock, *pwalletMain));
    ProcessBlockAndCheckRejectionReason(pblock, "bad-txns-vout-empty", 250);

    // Now connect the proper block
    pblock = std::make_shared<CBlock>(pblocktemplate->block);
    ProcessNewBlock(pblock, nullptr);
    BOOST_CHECK_EQUAL(WITH_LOCK(cs_main, return chainActive.Tip()->GetBlockHash()), pblock->GetHash());
}

CTransaction CreateAndCommitTx(CWallet* pwalletMain, const CTxDestination& dest, CAmount destValue, CCoinControl* coinControl = nullptr)
{
    CTransactionRef txNew;
    CReserveKey reservekey(pwalletMain);
    CAmount nFeeRet = 0;
    std::string strFailReason;
    BOOST_ASSERT(pwalletMain->CreateTransaction(GetScriptForDestination(dest),
                                   destValue,
                                   txNew,
                                   reservekey,
                                   nFeeRet,
                                   strFailReason,
                                   coinControl));
    pwalletMain->CommitTransaction(txNew, reservekey, nullptr);
    return *txNew;
}

COutPoint GetOutpointWithAmount(const CTransaction& tx, CAmount outpointValue)
{
    for (int i=0; i<tx.vout.size(); i++) {
        if (tx.vout[i].nValue == outpointValue) {
            return COutPoint(tx.GetHash(), i);
        }
    }
    BOOST_ASSERT_MSG(false, "error in test, no output in tx for value");
    return {};
}

std::shared_ptr<CBlock> CreateBlockInternal(CWallet* pwalletMain, const std::vector<CMutableTransaction>& txns = {}, CBlockIndex* customPrevBlock = nullptr)
{
    std::vector<CStakeableOutput> availableCoins;
    BOOST_CHECK(pwalletMain->StakeableCoins(&availableCoins));
    std::unique_ptr<CBlockTemplate> pblocktemplate = BlockAssembler(
            Params(), false).CreateNewBlock(CScript(),
                                            pwalletMain,
                                            true,
                                            &availableCoins,
                                            true,
                                            false,
                                            customPrevBlock,
                                            false);
    BOOST_ASSERT(pblocktemplate);
    auto pblock = std::make_shared<CBlock>(pblocktemplate->block);
    if (!txns.empty()) {
        for (const auto& tx : txns) {
            pblock->vtx.emplace_back(MakeTransactionRef(tx));
        }
        pblock->hashMerkleRoot = BlockMerkleRoot(*pblock);
        assert(SignBlock(*pblock, *pwalletMain));
    }
    return pblock;
}

BOOST_FIXTURE_TEST_CASE(created_on_fork_tests, TestPoSChainSetup)
{
    // Let's create few more PoS blocks
    for (int i=0; i<30; i++) {
        std::shared_ptr<CBlock> pblock = CreateBlockInternal(pwalletMain.get());
        BOOST_CHECK(ProcessNewBlock(pblock, nullptr));
    }

    // Chains diagram:
    // A -- B -- C -- D -- E -- F
    //           \
    //             -- D1 -- E1 -- F1
    //           \
    //             -- D2 -- E2 -- F2

    // Tests:
    // 1) coins created in D1 and spent in E1
    // 2) coins created and spent in E2, being double spent in F2.
    // 3) coins created in D and spent in E'
    // 4) coins create in D, spent in E and then double spent in E'

    // Let's create block C with a valid c1tx
    auto c1Tx = CreateAndCommitTx(pwalletMain.get(), *pwalletMain->getNewAddress("").getObjResult(), 249 * COIN);
    WITH_LOCK(pwalletMain->cs_wallet, pwalletMain->LockCoin(GetOutpointWithAmount(c1Tx, 249 * COIN)));
    std::shared_ptr<CBlock> pblockC = CreateBlockInternal(pwalletMain.get(), {c1Tx});
    BOOST_CHECK(ProcessNewBlock(pblockC, nullptr));

    // Create block D
    std::shared_ptr<CBlock> pblockD = CreateBlockInternal(pwalletMain.get());

    // Second forked block that connects a new tx
    auto dest = pwalletMain->getNewAddress("").getObjResult();
    auto d1Tx = CreateAndCommitTx(pwalletMain.get(), *dest, 200 * COIN);
    std::shared_ptr<CBlock> pblockD1 = CreateBlockInternal(pwalletMain.get(), {d1Tx});

    // Process blocks
    ProcessNewBlock(pblockD, nullptr);
    ProcessNewBlock(pblockD1, nullptr);
    BOOST_CHECK(WITH_LOCK(cs_main, return chainActive.Tip()->GetBlockHash() ==  pblockD->GetHash()));

    // Ensure that the coin does not exist in the main chain
    const Coin& utxo = pcoinsTip->AccessCoin(COutPoint(d1Tx.GetHash(), 0));
    BOOST_CHECK(utxo.out.IsNull());

    // ### Check (1) -> coins created in D' and spent in E' ###

    // Create tx spending the previously created tx on the forked chain
    CCoinControl coinControl;
    coinControl.fAllowOtherInputs = true;
    coinControl.Select(COutPoint(d1Tx.GetHash(), 0), d1Tx.vout[0].nValue);
    auto e1Tx = CreateAndCommitTx(pwalletMain.get(), *dest, d1Tx.vout[0].nValue, &coinControl);

    CBlockIndex* pindexPrev = mapBlockIndex.at(pblockD1->GetHash());
    std::shared_ptr<CBlock> pblockE1 = CreateBlockInternal(pwalletMain.get(), {e1Tx}, pindexPrev);
    BOOST_CHECK(ProcessNewBlock(pblockE1, nullptr));

    // ### Check (2) -> coins created and spent in E2, being double spent in F2 ###

    // Create block E2 with E2_tx1 and E2_tx2. Where E2_tx2 is spending the outputs of E2_tx1
    CCoinControl coinControlE2;
    coinControlE2.Select(GetOutpointWithAmount(c1Tx, 249 * COIN), 249 * COIN);
    auto E2_tx1 = CreateAndCommitTx(pwalletMain.get(), *dest, 200 * COIN, &coinControlE2);

    coinControl.UnSelectAll();
    coinControl.Select(GetOutpointWithAmount(E2_tx1, 200 * COIN), 200 * COIN);
    coinControl.fAllowOtherInputs = false;
    auto E2_tx2 = CreateAndCommitTx(pwalletMain.get(), *dest, 199 * COIN, &coinControl);

    std::shared_ptr<CBlock> pblockE2 = CreateBlockInternal(pwalletMain.get(), {E2_tx1, E2_tx2}, pindexPrev);
    BOOST_CHECK(ProcessNewBlock(pblockE2, nullptr));

    // Create block with F2_tx1 spending E2_tx1 again.
    auto F2_tx1 = CreateAndCommitTx(pwalletMain.get(), *dest, 199 * COIN, &coinControl);

    pindexPrev = mapBlockIndex.at(pblockE2->GetHash());
    std::shared_ptr<CBlock> pblock5Forked = CreateBlockInternal(pwalletMain.get(), {F2_tx1}, pindexPrev);
    BlockStateCatcher stateCatcher(pblock5Forked->GetHash());
    stateCatcher.registerEvent();
    BOOST_CHECK(!ProcessNewBlock(pblock5Forked, nullptr));
    BOOST_CHECK(stateCatcher.found);
    CValidationState state = stateCatcher.state;
    BOOST_CHECK(!stateCatcher.state.IsValid());
    BOOST_CHECK_EQUAL(state.GetRejectReason(), "bad-txns-inputs-spent-fork-post-split");


}

BOOST_AUTO_TEST_SUITE_END()
