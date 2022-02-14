// Copyright (c) 2022 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php.

#include "test/test_pivx.h"

#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_SUITE(net_quorums_tests)

// Simplified copy of quorums_connections.h/cpp, GetQuorumRelayMembers function.
// (merely without the CDeterministicMNCPtr as it's not needed to test the algorithm)
std::set<uint256> GetQuorumRelayMembers(const std::vector<uint256>& mnList,
                                        unsigned int forMemberIndex)
{
    // Special case
    if (mnList.size() == 2) {
        return {mnList[(forMemberIndex + 1) % 2]};
    }
    // Relay to nodes at indexes (i+2^k)%n, where
    //   k: 0..max(1, floor(log2(n-1))-1)
    //   n: size of the quorum/ring
    std::set<uint256> r;
    int gap = 1;
    int gap_max = (int)mnList.size() - 1;
    int k = 0;
    while ((gap_max >>= 1) || k <= 1) {
        size_t idx = (forMemberIndex + gap) % mnList.size();
        r.emplace(mnList[idx]);
        gap <<= 1;
        k++;
    }
    return r;
}

std::vector<uint256> createMNList(unsigned int size)
{
    std::vector<uint256> mns;
    for (size_t i = 0; i < size; i++) {
        uint256 item;
        do { item = g_insecure_rand_ctx.rand256(); } while (std::find(mns.begin(), mns.end(), item) != mns.end());
        mns.emplace_back(item);
    }
    return mns;
}

void checkQuorumRelayMembers(const std::vector<uint256>& list, unsigned int expectedResSize)
{
    for (size_t i = 0; i < list.size(); i++) {
        const auto& set = GetQuorumRelayMembers(list, i);
        BOOST_CHECK_MESSAGE(set.size() == expectedResSize,
                            strprintf("size %d, expected ret size %d, ret size %d ", list.size(), expectedResSize, set.size()));
        BOOST_CHECK(set.count(list[i]) == 0);
    }
}

BOOST_FIXTURE_TEST_CASE(get_quorum_relay_members, BasicTestingSetup)
{
    // 1) Test special case of 2 members
    std::vector<uint256> list = createMNList(2);
    checkQuorumRelayMembers(list, 1);

    // 2) Test quorum sizes 3 to 1200
    list = createMNList(1200);
    size_t expectedRet = 2;
    for (size_t i = 3; i < 1201; i++) {
        std::vector<uint256> copyList = list;
        copyList.resize(i);
        if ((2 << expectedRet) < (int)i) expectedRet++;
        checkQuorumRelayMembers(copyList, expectedRet);
    }
}

BOOST_AUTO_TEST_SUITE_END()
