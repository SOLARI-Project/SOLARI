// Copyright (c) 2022 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php.

#include "netbase.h"
#include "test/test_pivx.h"
#include "tiertwo/netfulfilledman.h"
#include "utiltime.h"

#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(netfulfilledman_tests, RegTestingSetup)

BOOST_AUTO_TEST_CASE(netfulfilledman_simple_add_and_expire)
{
    int64_t now = GetTime();
    SetMockTime(now);

    CNetFulfilledRequestManager fulfilledMan;
    CService service = LookupNumeric("1.1.1.1", 9999);
    std::string request = "request";
    BOOST_ASSERT(!fulfilledMan.HasFulfilledRequest(service, request));

    // Add request
    fulfilledMan.AddFulfilledRequest(service, request);
    // Verify that the request is there
    BOOST_ASSERT(fulfilledMan.HasFulfilledRequest(service, request));

    // Advance mock time to surpass FulfilledRequestExpireTime
    SetMockTime(now + 60 * 60 + 1);

    // Verify that the request exists and expired now
    BOOST_CHECK(fulfilledMan.Size() == 1);
    BOOST_CHECK(!fulfilledMan.HasFulfilledRequest(service, request));

    // Verify request removal
    fulfilledMan.CheckAndRemove();
    BOOST_CHECK(fulfilledMan.Size() == 0);
}

BOOST_AUTO_TEST_SUITE_END()
