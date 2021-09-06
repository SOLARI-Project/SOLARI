// Copyright (c) 2019-2020 The Dash Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "test/test_pivx.h"
#include "bls/bls_ies.h"
#include "bls/bls_worker.h"
#include "bls/bls_wrapper.h"
#include "random.h"

#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(bls_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(bls_sethexstr_tests)
{
    CBLSSecretKey sk;
    std::string strValidSecret = "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f";
    // Note: invalid string passed to SetHexStr() should cause it to fail and reset key internal data
    BOOST_CHECK(sk.SetHexStr(strValidSecret));
    BOOST_CHECK(!sk.SetHexStr("000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1g")); // non-hex
    BOOST_CHECK(!sk.IsValid());
    BOOST_CHECK(sk == CBLSSecretKey());
    // Try few more invalid strings
    BOOST_CHECK(sk.SetHexStr(strValidSecret));
    BOOST_CHECK(!sk.SetHexStr("000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e")); // hex but too short
    BOOST_CHECK(!sk.IsValid());
    BOOST_CHECK(sk.SetHexStr(strValidSecret));
    BOOST_CHECK(!sk.SetHexStr("000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f20")); // hex but too long
    BOOST_CHECK(!sk.IsValid());
}

BOOST_AUTO_TEST_CASE(bls_sig_tests)
{
    CBLSSecretKey sk1, sk2;
    sk1.MakeNewKey();
    sk2.MakeNewKey();

    uint256 msgHash1 = uint256S("0000000000000000000000000000000000000000000000000000000000000001");
    uint256 msgHash2 = uint256S("0000000000000000000000000000000000000000000000000000000000000002");

    auto sig1 = sk1.Sign(msgHash1);
    auto sig2 = sk2.Sign(msgHash1);

    BOOST_CHECK(sig1.VerifyInsecure(sk1.GetPublicKey(), msgHash1));
    BOOST_CHECK(!sig1.VerifyInsecure(sk1.GetPublicKey(), msgHash2));

    BOOST_CHECK(sig2.VerifyInsecure(sk2.GetPublicKey(), msgHash1));
    BOOST_CHECK(!sig2.VerifyInsecure(sk2.GetPublicKey(), msgHash2));

    BOOST_CHECK(!sig1.VerifyInsecure(sk2.GetPublicKey(), msgHash1));
    BOOST_CHECK(!sig1.VerifyInsecure(sk2.GetPublicKey(), msgHash2));
    BOOST_CHECK(!sig2.VerifyInsecure(sk1.GetPublicKey(), msgHash1));
    BOOST_CHECK(!sig2.VerifyInsecure(sk1.GetPublicKey(), msgHash2));
}

static BLSIdVector GetRandomBLSIds(size_t n)
{
    BLSIdVector v;
    for (size_t i = 0; i < n; i++) {
        v.emplace_back(GetRandHash());
    }
    return v;
}

std::vector<size_t> GetRandomElements(size_t m, size_t n)
{
    assert(m <= n);
    std::vector<size_t> idxs;
    for (size_t i = 0; i < n; i++) {
        idxs.emplace_back(i);
    }
    Shuffle(idxs.begin(), idxs.end(), FastRandomContext());
    return std::vector<size_t>(idxs.begin(), idxs.begin() + m);
}

struct Member
{
    CBLSId id;
    BLSVerificationVectorPtr vecP;
    BLSSecretKeyVector contributions;
    CBLSSecretKey skShare;

    Member(const CBLSId& _id): id(_id) {}
};

BOOST_AUTO_TEST_CASE(dkg)
{
    CBLSWorker worker;
    const size_t N = 40;     // quorum size
    const size_t M = 30;     // threshold

    worker.Start();

    // Create N Members and send contributions
    const BLSIdVector& ids = GetRandomBLSIds(N);
    std::vector<Member> quorum;
    for (const auto& id : ids) {
        Member m(id);
        // Generate contributions
        worker.GenerateContributions((int)M, ids, m.vecP, m.contributions);
        BOOST_CHECK_EQUAL(m.vecP->size(), M);
        BOOST_CHECK_EQUAL(m.contributions.size(), N);
        // Verify contributions against verification vector
        for (size_t j = 0; j < N; j++) {
            BOOST_CHECK(worker.VerifyContributionShare(ids[j], m.vecP, m.contributions[j]));
        }
        quorum.emplace_back(std::move(m));
    }

    // Aggregate received contributions for each Member to produce key shares
    for (size_t i = 0; i < N; i++) {
        Member& m = quorum[i];
        BLSSecretKeyVector rcvSkContributions;
        for (size_t j = 0; j < N; j++) {
            rcvSkContributions.emplace_back(quorum[j].contributions[i]);
        }
        m.skShare = worker.AggregateSecretKeys(rcvSkContributions);
        // Recover public key share for m, and check against the secret key share
        BLSPublicKeyVector rcvPkContributions;
        for (size_t j = 0; j < N; j++) {
            CBLSPublicKey pkContribution = worker.BuildPubKeyShare(quorum[j].vecP, m.id);
            // This is implied by VerifyContributionShare, but let's double check
            BOOST_CHECK(rcvSkContributions[j].GetPublicKey() == pkContribution);
            rcvPkContributions.emplace_back(pkContribution);
        }
        CBLSPublicKey pkShare = worker.AggregatePublicKeys(rcvPkContributions);
        BOOST_CHECK(m.skShare.GetPublicKey() == pkShare);
    }

    // Each member signs a message with its key share producing a signature share
    const uint256& msg = GetRandHash();
    BLSSignatureVector allSigShares;
    for (const Member& m : quorum) {
        allSigShares.emplace_back(m.skShare.Sign(msg));
    }

    // Pick M (random) key shares and recover threshold secret/public key
    const auto& idxs = GetRandomElements(M, N);
    BLSSecretKeyVector skShares;
    BLSIdVector random_ids;
    for (size_t i : idxs) {
        skShares.emplace_back(quorum[i].skShare);
        random_ids.emplace_back(quorum[i].id);
    }
    CBLSSecretKey thresholdSk;
    BOOST_CHECK(thresholdSk.Recover(skShares, random_ids));
    const CBLSPublicKey& thresholdPk = thresholdSk.GetPublicKey();

    // Check that the recovered threshold public key equals the verification
    // vector free coefficient
    std::vector<BLSVerificationVectorPtr> v;
    for (const Member& m : quorum) v.emplace_back(m.vecP);
    CBLSPublicKey pk = worker.BuildQuorumVerificationVector(v)->at(0);
    BOOST_CHECK(pk == thresholdPk);

    // Pick M (random, different BLSids than before) signature shares, and recover
    // the threshold signature
    const auto& idxs2 = GetRandomElements(M, N);
    BLSSignatureVector sigShares;
    BLSIdVector random_ids2;
    for (size_t i : idxs2) {
        sigShares.emplace_back(allSigShares[i]);
        random_ids2.emplace_back(quorum[i].id);
    }
    CBLSSignature thresholdSig;
    BOOST_CHECK(thresholdSig.Recover(sigShares, random_ids2));

    // Verify threshold signature against threshold public key
    BOOST_CHECK(thresholdSig.VerifyInsecure(thresholdPk, msg));

    // Now replace a signature share with an invalid signature, recover the threshold
    // signature again, and check that verification fails with the threshold public key
    CBLSSecretKey dummy_sk;
    dummy_sk.MakeNewKey();
    CBLSSignature dummy_sig = dummy_sk.Sign(msg);
    BOOST_CHECK(dummy_sig != sigShares[0]);
    sigShares[0] = dummy_sig;
    BOOST_CHECK(thresholdSig.Recover(sigShares, random_ids2));
    BOOST_CHECK(!thresholdSig.VerifyInsecure(thresholdPk, msg));

    worker.Stop();
}

BOOST_AUTO_TEST_CASE(bls_ies_tests)
{
    // Test basic encryption and decryption of the BLS Integrated Encryption Scheme.
    CBLSSecretKey aliceSk;
    aliceSk.MakeNewKey();
    const CBLSPublicKey alicePk = aliceSk.GetPublicKey();
    BOOST_CHECK(aliceSk.IsValid());

    CBLSSecretKey bobSk;
    bobSk.MakeNewKey();
    const CBLSPublicKey bobPk = bobSk.GetPublicKey();
    BOOST_CHECK(bobSk.IsValid());
    // Message (no pad allowed, message length must be a multiple of 16 - cypher block size)
    std::string message = "Hello PIVX world";
    std::string msgHex = HexStr(message);
    std::vector<unsigned char> msg(msgHex.begin(), msgHex.end());

    CBLSIESEncryptedBlob iesEnc;
    BOOST_CHECK(iesEnc.Encrypt(bobPk, msg.data(), msg.size()));

    // valid decryption.
    CDataStream decMsg(SER_NETWORK, PROTOCOL_VERSION);
    BOOST_CHECK(iesEnc.Decrypt(bobSk, decMsg));

    std::vector<unsigned char> msg2 = ParseHex(decMsg.data());
    std::string str(msg2.begin(), msg2.end());
    BOOST_CHECK_EQUAL(str, message);

    // Invalid decryption sk
    iesEnc.Decrypt(aliceSk, decMsg);
    msg2 = ParseHex(decMsg.data());
    std::string str2(msg2.begin(), msg2.end());
    BOOST_CHECK(str2 != message);

    // Invalid ephemeral pubkey
    auto iesEphemeralPk = iesEnc.ephemeralPubKey;
    iesEnc.ephemeralPubKey = alicePk;
    iesEnc.Decrypt(bobSk, decMsg);
    msg2 = ParseHex(decMsg.data());
    std::string str3(msg2.begin(), msg2.end());
    BOOST_CHECK(str3 != message);
    iesEnc.ephemeralPubKey = iesEphemeralPk;

    // Invalid iv
    GetRandBytes(iesEnc.iv, sizeof(iesEnc.iv));
    iesEnc.Decrypt(bobSk, decMsg);
    msg2 = ParseHex(decMsg.data());
    std::string str4(msg2.begin(), msg2.end());
    BOOST_CHECK(str4 != message);
}

BOOST_AUTO_TEST_SUITE_END()
