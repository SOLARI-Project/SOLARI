// Copyright (c) 2022 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php.

#include "test/test_pivx.h"

#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_SUITE(net_quorums_tests)

// Simplified copy of quorums_connections.h/cpp, GetQuorumRelayMembers function.
// (merely without the CDeterministicMNCPtr as it's not needed to test the algorithm)
std::set<uint256> GetQuorumRelayMembers(const std::vector<uint256>& mnList,
                                        const uint256& forMember,
                                        unsigned int forMemberIndex)
{
    // Relay to nodes at indexes (i+2^k)%n, where
    //   k: 0..max(1, floor(log2(n-1))-1)
    //   n: size of the quorum/ring
    std::set<uint256> r;
    int gap = 1;
    int gap_max = (int)mnList.size() - 1;
    int k = 0;
    while ((gap_max >>= 1) || k <= 1) {
        size_t idx = (forMemberIndex + gap) % mnList.size();
        auto& otherDmnId = mnList[idx];
        if (otherDmnId == forMember) {
            continue;
        }
        r.emplace(otherDmnId);
        gap <<= 1;
        k++;
    }
    return r;
}

std::vector<uint256> createMNList(unsigned int size)
{
    std::vector<uint256> mns;
    for (int i = 0; i < size; i++) {
        uint256 item;
        do { item = g_insecure_rand_ctx.rand256(); } while (std::find(mns.begin(), mns.end(), item) != mns.end());
        mns.emplace_back(item);
    }
    return mns;
}

void checkQuorumRelayMembers(const std::vector<uint256>& list, unsigned int expectedResSize)
{
    for (int i = 0; i < list.size(); i++) {
        const auto& set = GetQuorumRelayMembers(list, list[i], i);
        BOOST_CHECK_MESSAGE(set.size() == expectedResSize || set.size() == expectedResSize - 1,
                            strprintf("size %d, expected ret size %d, ret size %d ", list.size(), expectedResSize, set.size()));
    }
}

BOOST_FIXTURE_TEST_CASE(get_quorum_relay_members, BasicTestingSetup)
{
    SeedInsecureRand(SeedRand::ZEROS);
    std::vector<uint256> list = createMNList(1200);

    // 1) Test random quorum sizes
    for (int i = 0; i < 2; i++) {
        int size = (int) g_insecure_rand_ctx.randbits((int) g_insecure_rand_ctx.randrange(9));
        int expectedRet = 0;
        for (int aux = size; aux >>= 1;) { expectedRet++; }
        std::vector<uint256> copyList = list;
        copyList.resize(size);
        checkQuorumRelayMembers(copyList, expectedRet);
    }

    // 1) Test quorum size of 400, each list should contain 8-9 other elements.
    list.resize(400);
    checkQuorumRelayMembers(list, 9);
    // 2) Test quorum size of 50, each list should contain 5-6 other elements.
    list.resize(50);
    checkQuorumRelayMembers(list, 6);

    // 4) Test quorum size of 2, should only return a single element (cannot return himself)
    // Currently failing, infinite loop
    list.resize(2);
    checkQuorumRelayMembers(list, 1);
}

BOOST_AUTO_TEST_SUITE_END()
