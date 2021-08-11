#!/usr/bin/env python3
# Copyright (c) 2017 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test RPC calls related to net.

Tests correspond to code in rpc/net.cpp.
"""

from test_framework.messages import CAddress, msg_addr, NODE_NETWORK
from test_framework.mininode import P2PInterface
from test_framework.test_framework import PivxTestFramework
from test_framework.util import (
    assert_equal,
    assert_greater_than_or_equal,
    assert_greater_than,
    assert_raises_rpc_error,
    connect_nodes,
    disconnect_nodes,
    p2p_port,
    wait_until,
)

class NetTest(PivxTestFramework):
    def set_test_params(self):
        self.setup_clean_chain = True
        self.num_nodes = 2

    def run_test(self):
        self.log.info("Connect nodes both way")
        connect_nodes(self.nodes[0], 1)
        connect_nodes(self.nodes[1], 0)

        self._test_connection_count()
        self._test_getnettotals()
        self._test_getnetworkinginfo()
        self._test_getaddednodeinfo()
        # self._test_getpeerinfo()
        self._test_getnodeaddresses()

    def _test_connection_count(self):
        assert_equal(self.nodes[0].getconnectioncount(), 2)

    def _test_getnettotals(self):
        # getnettotals totalbytesrecv and totalbytessent should be
        # consistent with getpeerinfo. Since the RPC calls are not atomic,
        # and messages might have been recvd or sent between RPC calls, call
        # getnettotals before and after and verify that the returned values
        # from getpeerinfo are bounded by those values.
        net_totals_before = self.nodes[0].getnettotals()
        peer_info = self.nodes[0].getpeerinfo()
        net_totals_after = self.nodes[0].getnettotals()
        assert_equal(len(peer_info), 2)
        peers_recv = sum([peer['bytesrecv'] for peer in peer_info])
        peers_sent = sum([peer['bytessent'] for peer in peer_info])

        assert_greater_than_or_equal(peers_recv, net_totals_before['totalbytesrecv'])
        assert_greater_than_or_equal(net_totals_after['totalbytesrecv'], peers_recv)
        assert_greater_than_or_equal(peers_sent, net_totals_before['totalbytessent'])
        assert_greater_than_or_equal(net_totals_after['totalbytessent'], peers_sent)

        # test getnettotals and getpeerinfo by doing a ping
        # the bytes sent/received should change
        # note ping and pong are 32 bytes each
        self.nodes[0].ping()
        wait_until(lambda: (self.nodes[0].getnettotals()['totalbytessent'] >= net_totals_after['totalbytessent'] + 32 * 2), timeout=1)
        wait_until(lambda: (self.nodes[0].getnettotals()['totalbytesrecv'] >= net_totals_after['totalbytesrecv'] + 32 * 2), timeout=1)

    def _test_getnetworkinginfo(self):
        assert_equal(self.nodes[0].getnetworkinfo()['connections'], 2)

        disconnect_nodes(self.nodes[0], 1)
        # Wait a bit for all sockets to close
        wait_until(lambda: self.nodes[0].getnetworkinfo()['connections'] == 0, timeout=3)

        self.log.info("Connect nodes both way")
        connect_nodes(self.nodes[0], 1)
        connect_nodes(self.nodes[1], 0)
        assert_equal(self.nodes[0].getnetworkinfo()['connections'], 2)

    def _test_getaddednodeinfo(self):
        assert_equal(self.nodes[0].getaddednodeinfo(True), [])
        # add a node (node2) to node0
        ip_port = "127.0.0.1:{}".format(p2p_port(2))
        self.nodes[0].addnode(ip_port, 'add')
        # check that the node has indeed been added
        added_nodes = self.nodes[0].getaddednodeinfo(True, ip_port)
        assert_equal(len(added_nodes), 1)
        assert_equal(added_nodes[0]['addednode'], ip_port)
        # check that a non-existent node returns an error
        assert_raises_rpc_error(-24, "Node has not been added", self.nodes[0].getaddednodeinfo, True, '1.1.1.1')

    def _test_getpeerinfo(self):
        peer_info = [x.getpeerinfo() for x in self.nodes]
        # check both sides of bidirectional connection between nodes
        # the address bound to on one side will be the source address for the other node
        assert_equal(peer_info[0][0]['addrbind'], peer_info[1][0]['addr'])
        assert_equal(peer_info[1][0]['addrbind'], peer_info[0][0]['addr'])

    def _test_getnodeaddresses(self):
        self.nodes[0].add_p2p_connection(P2PInterface())

        # send some addresses to the node via the p2p message addr
        msg = msg_addr()
        imported_addrs = []
        for i in range(256):
            a = "123.123.123.{}".format(i)
            imported_addrs.append(a)
            addr = CAddress()
            addr.time = 100000000
            addr.nServices = NODE_NETWORK
            addr.ip = a
            addr.port = 51472
            msg.addrs.append(addr)
        self.nodes[0].p2p.send_and_ping(msg)

        # Obtain addresses via rpc call and check they were ones sent in before.
        #
        # All addresses added above are in the same netgroup and so are assigned
        # to the same bucket. Maximum possible addresses in addrman is therefore
        # 64, although actual number will usually be slightly less due to
        # BucketPosition collisions.
        node_addresses = self.nodes[0].getnodeaddresses(0)
        assert_greater_than(len(node_addresses), 50)
        assert_greater_than(65, len(node_addresses))
        for a in node_addresses:
            assert_greater_than(a["time"], 1527811200)  # 1st June 2018
            assert_equal(a["services"], NODE_NETWORK)
            assert a["address"] in imported_addrs
            assert_equal(a["port"], 51472)

        node_addresses = self.nodes[0].getnodeaddresses(1)
        assert_equal(len(node_addresses), 1)

        assert_raises_rpc_error(-8, "Address count out of range", self.nodes[0].getnodeaddresses, -1)

        # addrman's size cannot be known reliably after insertion, as hash collisions may occur
        # so only test that requesting a large number of addresses returns less than that
        LARGE_REQUEST_COUNT = 10000
        node_addresses = self.nodes[0].getnodeaddresses(LARGE_REQUEST_COUNT)
        assert_greater_than(LARGE_REQUEST_COUNT, len(node_addresses))

if __name__ == '__main__':
    NetTest().main()
