// Copyright (c) 2021 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php.

#include "test/test_pivx.h"

#include "masternode-payments.h"
#include "masternode-sync.h"
#include "masternodeman.h"
#include "spork.h"
#include "primitives/transaction.h"
#include "utilmoneystr.h"
#include "validation.h"

#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_SUITE(mnpayments_tests)

void enableMnSyncAndSuperblocksPayment()
{
    // force mnsync complete
    masternodeSync.RequestedMasternodeAssets = MASTERNODE_SYNC_FINISHED;

    // enable SPORK_13
    int64_t nTime = GetTime() - 10;
    CSporkMessage spork(SPORK_13_ENABLE_SUPERBLOCKS, nTime + 1, nTime);
    sporkManager.AddOrUpdateSporkMessage(spork);
    BOOST_CHECK(sporkManager.IsSporkActive(SPORK_13_ENABLE_SUPERBLOCKS));

    spork = CSporkMessage(SPORK_9_MASTERNODE_BUDGET_ENFORCEMENT, nTime + 1, nTime);
    sporkManager.AddOrUpdateSporkMessage(spork);
    BOOST_CHECK(sporkManager.IsSporkActive(SPORK_9_MASTERNODE_BUDGET_ENFORCEMENT));
}

static bool CreateMNWinnerPayment(const CTxIn& mnVinVoter, int paymentBlockHeight, const CScript& payeeScript,
                                  const CKey& signerKey, const CPubKey& signerPubKey, CValidationState& state)
{
    CMasternodePaymentWinner mnWinner(mnVinVoter, paymentBlockHeight);
    mnWinner.AddPayee(payeeScript);
    BOOST_CHECK(mnWinner.Sign(signerKey, signerPubKey.GetID()));
    return masternodePayments.ProcessMNWinner(mnWinner, nullptr, state);
}

class MNdata
{
public:
    COutPoint collateralOut;
    CKey mnPrivKey;
    CPubKey mnPubKey;
    CPubKey collateralPubKey;
    CScript mnPayeeScript;

    MNdata(const COutPoint& collateralOut, const CKey& mnPrivKey, const CPubKey& mnPubKey,
           const CPubKey& collateralPubKey, const CScript& mnPayeeScript) :
           collateralOut(collateralOut), mnPrivKey(mnPrivKey), mnPubKey(mnPubKey),
           collateralPubKey(collateralPubKey), mnPayeeScript(mnPayeeScript) {}


};

CMasternode buildMN(const MNdata& data, const uint256& tipHash, uint64_t tipTime)
{
    CMasternode mn;
    mn.vin = CTxIn(data.collateralOut);
    mn.pubKeyCollateralAddress = data.mnPubKey;
    mn.pubKeyMasternode = data.collateralPubKey;
    mn.sigTime = GetTime() - 8000 - 1; // MN_WINNER_MINIMUM_AGE = 8000.
    mn.lastPing = CMasternodePing(mn.vin, tipHash, tipTime);
    return mn;
}

class FakeMasternode {
public:
    explicit FakeMasternode(CMasternode& mn, const MNdata& data) : mn(mn), data(data) {}
    CMasternode mn;
    MNdata data;
};

std::vector<FakeMasternode> buildMNList(const uint256& tipHash, uint64_t tipTime, int size)
{
    std::vector<FakeMasternode> ret;
    for (int i=0; i < size; i++) {
        CKey mnKey;
        mnKey.MakeNewKey(true);
        const CPubKey& mnPubKey = mnKey.GetPubKey();
        const CScript& mnPayeeScript = GetScriptForDestination(mnPubKey.GetID());
        // Fake collateral out and key for now
        COutPoint mnCollateral(GetRandHash(), 0);
        const CPubKey& collateralPubKey = mnPubKey;

        // Now add the MN
        MNdata mnData(mnCollateral, mnKey, mnPubKey, collateralPubKey, mnPayeeScript);
        CMasternode mn = buildMN(mnData, tipHash, tipTime);
        BOOST_CHECK(mnodeman.Add(mn));
        ret.emplace_back(mn, mnData);
    }
    return ret;
}

FakeMasternode findMNData(std::vector<FakeMasternode>& mnList, const MasternodeRef& ref)
{
    for (const auto& item : mnList) {
        if (item.data.mnPubKey == ref->pubKeyMasternode) {
            return item;
        }
    }
    throw std::runtime_error("MN not found");
}

bool findStrError(CValidationState& state, const std::string& str)
{
    return state.GetRejectReason().find(str) != std::string::npos;
}

BOOST_FIXTURE_TEST_CASE(mnwinner_test, TestChain100Setup)
{
    CreateAndProcessBlock({}, coinbaseKey);
    CBlock tipBlock = CreateAndProcessBlock({}, coinbaseKey);
    enableMnSyncAndSuperblocksPayment();
    int nextBlockHeight = 103;
    UpdateNetworkUpgradeParameters(Consensus::UPGRADE_V5_3, nextBlockHeight - 1);

    // MN list.
    std::vector<FakeMasternode> mnList = buildMNList(tipBlock.GetHash(), tipBlock.GetBlockTime(), 20);
    std::vector<std::pair<int64_t, MasternodeRef>> mnRank = mnodeman.GetMasternodeRanks(nextBlockHeight - 100);

    // Take the first MN
    auto firstMN = findMNData(mnList, mnRank[0].second);
    CTxIn mnVinVoter(firstMN.mn.vin);
    int paymentBlockHeight = nextBlockHeight;
    CScript payeeScript = firstMN.data.mnPayeeScript;
    CMasternode* pFirstMN = mnodeman.Find(firstMN.mn.vin.prevout);
    pFirstMN->sigTime += 8000 + 1; // MN_WINNER_MINIMUM_AGE = 8000.
    // Voter MN1, fail because the sigTime - GetAdjustedTime() is not greater than MN_WINNER_MINIMUM_AGE.
    CValidationState state1;
    BOOST_CHECK(!CreateMNWinnerPayment(mnVinVoter, paymentBlockHeight, payeeScript,
                                       firstMN.data.mnPrivKey, firstMN.data.mnPubKey, state1));
    // future: add specific error cause
    BOOST_CHECK_MESSAGE(findStrError(state1, "Masternode not in the top"), state1.GetRejectReason());

    // Voter MN2, fail because MN2 doesn't match with the signing keys.
    auto secondMn = findMNData(mnList, mnRank[1].second);
    CMasternode* pSecondMN = mnodeman.Find(secondMn.mn.vin.prevout);
    mnVinVoter = CTxIn(pSecondMN->vin);
    payeeScript = secondMn.data.mnPayeeScript;
    CValidationState state2;
    BOOST_CHECK(!CreateMNWinnerPayment(mnVinVoter, paymentBlockHeight, payeeScript,
                                       firstMN.data.mnPrivKey, firstMN.data.mnPubKey, state2));
    BOOST_CHECK_MESSAGE(findStrError(state2, "voter mnwinner signature"), state2.GetRejectReason());

    // Voter MN2, fail because MN2 is not enabled
    pSecondMN->SetSpent();
    BOOST_CHECK(!pSecondMN->IsEnabled());
    mnVinVoter = CTxIn(pSecondMN->vin);
    CValidationState state3;
    BOOST_CHECK(!CreateMNWinnerPayment(mnVinVoter, paymentBlockHeight, payeeScript,
                                       secondMn.data.mnPrivKey, secondMn.data.mnPubKey, state3));
    // future: could add specific error cause.
    BOOST_CHECK_MESSAGE(findStrError(state3, "Masternode not in the top"), state3.GetRejectReason());

    // Voter MN15 pays to MN3, fail because the voter is not in the top ten.
    auto voterPos15 = findMNData(mnList, mnRank[14].second);
    auto thirdMn = findMNData(mnList, mnRank[2].second);
    CMasternode* p15dMN = mnodeman.Find(voterPos15.mn.vin.prevout);
    mnVinVoter = CTxIn(p15dMN->vin);
    payeeScript = thirdMn.data.mnPayeeScript;
    CValidationState state6;
    BOOST_CHECK(!CreateMNWinnerPayment(mnVinVoter, paymentBlockHeight, payeeScript,
                                       voterPos15.data.mnPrivKey, voterPos15.data.mnPubKey, state6));
    BOOST_CHECK_MESSAGE(findStrError(state6, "Masternode not in the top"), state6.GetRejectReason());

    // Voter MN3, passes
    CMasternode* pThirdMN = mnodeman.Find(thirdMn.mn.vin.prevout);
    mnVinVoter = CTxIn(pThirdMN->vin);
    CValidationState state7;
    BOOST_CHECK(CreateMNWinnerPayment(mnVinVoter, paymentBlockHeight, payeeScript,
                                      thirdMn.data.mnPrivKey, thirdMn.data.mnPubKey, state7));
    BOOST_CHECK_MESSAGE(state7.IsValid(), state7.GetRejectReason());

    // Create block and check that is being paid properly.
    tipBlock = CreateAndProcessBlock({}, coinbaseKey);
    BOOST_CHECK_MESSAGE(tipBlock.vtx[0]->vout.back().scriptPubKey == payeeScript, "error: block not paying to proper MN");
    nextBlockHeight++;

    // Now let's push two valid winner payments and make every MN in the top ten vote for them (having more votes in mnwinnerA than in mnwinnerB).
    // todo: make a tests for the vote count, the most voted mnwinner should be paid..
}

BOOST_AUTO_TEST_SUITE_END()