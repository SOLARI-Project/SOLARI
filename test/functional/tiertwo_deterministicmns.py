#!/usr/bin/env python3
# Copyright (c) 2021 The PIVX Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

#
# Test deterministic masternodes
#

from decimal import Decimal
from random import randrange
import time

from test_framework.test_framework import PivxTestFramework
from test_framework.blocktools import (
    create_block,
    create_coinbase,
)
from test_framework.messages import CTxOut, COIN
from test_framework.util import (
    assert_greater_than,
    assert_equal,
    assert_raises_rpc_error,
    bytes_to_hex_str,
    create_new_dmn,
    hex_str_to_bytes,
    spend_mn_collateral,
)


class DIP3Test(PivxTestFramework):

    def set_test_params(self):
        # 1 miner, 1 controller, 6 remote mns
        self.num_nodes = 8
        self.minerPos = 0
        self.controllerPos = 1
        self.setup_clean_chain = True
        self.extra_args = [["-nuparams=v5_shield:1", "-nuparams=v6_evo:130"]] * self.num_nodes
        self.extra_args[0].append("-sporkkey=932HEevBSujW2ud7RfB1YF91AFygbBRQj3de3LyaCRqNzKKgWXi")

    def add_new_dmn(self, mns, strType, op_keys=None, from_out=None, lock=True):
        mns.append(self.register_new_dmn(2 + len(mns),
                                         self.minerPos,
                                         self.controllerPos,
                                         strType,
                                         outpoint=from_out,
                                         op_addr_and_key=op_keys,
                                         fLock=lock))

    def check_mn_list(self, mns):
        for i in range(self.num_nodes):
            self.check_mn_list_on_node(i, mns)
        self.log.info("Deterministic list contains %d masternodes for all peers." % len(mns))

    def get_addr_balance(self, node, addr):
        rcv = node.listreceivedbyaddress(0, False, False, addr)
        return rcv[0]['amount'] if len(rcv) > 0 else 0

    def get_last_paid_mn(self):
        return next(x['proTxHash'] for x in self.nodes[0].listmasternodes()
                    if x['dmnstate']['lastPaidHeight'] == self.nodes[0].getblockcount())

    def create_block(self, mn_payee_script, height, prevhash):
        coinbase = create_coinbase(height)
        coinbase.vout[0].nValue -= 3 * COIN
        coinbase.vout.append(CTxOut(int(3 * COIN), hex_str_to_bytes(mn_payee_script)))
        coinbase.rehash()
        return create_block(int(prevhash, 16), coinbase, nVersion=10)

    def wait_until_mnsync_completed(self):
        SYNC_FINISHED = [999] * self.num_nodes
        synced = [-1] * self.num_nodes
        timeout = time.time() + 120
        while synced != SYNC_FINISHED and time.time() < timeout:
            synced = [node.mnsync("status")["RequestedMasternodeAssets"]
                      for node in self.nodes]
            if synced != SYNC_FINISHED:
                time.sleep(5)
        if synced != SYNC_FINISHED:
            raise AssertionError("Unable to complete mnsync: %s" % str(synced))

    def run_test(self):
        self.disable_mocktime()

        # Additional connections to miner and owner
        for nodePos in [self.minerPos, self.controllerPos]:
            self.connect_to_all(nodePos)
        miner = self.nodes[self.minerPos]
        controller = self.nodes[self.controllerPos]

        dummy_add = controller.getnewaddress("dummy")

        # Enforce mn payments and reject legacy mns at block 131
        self.activate_spork(0, "SPORK_8_MASTERNODE_PAYMENT_ENFORCEMENT")
        assert_equal("success", self.set_spork(self.minerPos, "SPORK_21_LEGACY_MNS_MAX_HEIGHT", 130))
        time.sleep(1)
        assert_equal([130] * self.num_nodes, [self.get_spork(x, "SPORK_21_LEGACY_MNS_MAX_HEIGHT")
                                              for x in range(self.num_nodes)])
        mns = []

        # Mine 100 blocks
        self.log.info("Mining...")
        miner.generate(110)
        self.sync_blocks()
        self.assert_equal_for_all(110, "getblockcount")

        # Test rejection before enforcement
        self.log.info("Testing rejection of ProRegTx before DIP3 activation...")
        assert_raises_rpc_error(-1, "Evo upgrade is not active yet", self.add_new_dmn, mns, "internal")
        assert_raises_rpc_error(-1, "Evo upgrade is not active yet", self.add_new_dmn, mns, "fund")
        # Can create the raw proReg
        dmn = create_new_dmn(2, controller, dummy_add, None)
        tx, sig = self.protx_register_ext(miner, controller, dmn, None, False, False)
        # but cannot send it
        assert_raises_rpc_error(-1, "Evo upgrade is not active yet", miner.protx_register_submit, tx, sig)
        self.log.info("Done. Now mine blocks till enforcement...")

        # DIP3 activates at block 130.
        miner.generate(130 - miner.getblockcount())
        self.sync_blocks()
        self.assert_equal_for_all(130, "getblockcount")

        # -- DIP3 enforced and SPORK_21 active here --
        self.wait_until_mnsync_completed()

        # Create 3 DMNs and init the remote nodes
        self.log.info("Initializing masternodes...")
        self.add_new_dmn(mns, "internal")
        self.add_new_dmn(mns, "external")
        self.add_new_dmn(mns, "fund")
        for mn in mns:
            self.nodes[mn.idx].initmasternode(mn.operator_key, "", True)
            time.sleep(1)
        miner.generate(1)
        self.sync_blocks()

        # Init the other 3 remote nodes before creating the ProReg tx
        self.log.info("Initializing more masternodes...")
        op_keys = []
        for i in range(3):
            idx = 2 + len(mns) + i
            add_and_key = []
            add_and_key.append(miner.getnewaddress("oper-%d-key" % idx))
            add_and_key.append(miner.dumpprivkey(add_and_key[0]))
            self.nodes[idx].initmasternode(add_and_key[1], "", True)
            op_keys.append(add_and_key)
            time.sleep(1)

        # Now send the ProReg txes and check list
        self.add_new_dmn(mns, "internal", op_keys[0])
        self.add_new_dmn(mns, "external", op_keys[1])
        self.add_new_dmn(mns, "fund", op_keys[2])
        miner.generate(2)
        self.sync_blocks()
        time.sleep(1)
        self.log.info("Masternodes started.")
        self.check_mn_list(mns)

        # Check status from remote nodes
        assert_equal([self.nodes[idx].getmasternodestatus()['status'] for idx in range(2, self.num_nodes)],
                     ["Ready"] * (self.num_nodes - 2))
        self.log.info("All masternodes ready.")

        # Test collateral spending
        dmn = mns.pop(randrange(len(mns)))  # pop one at random
        self.log.info("Spending collateral of mn with idx=%d..." % dmn.idx)
        spend_txid = spend_mn_collateral(controller, dmn)
        self.sync_mempools([miner, controller])
        miner.generate(1)
        self.sync_blocks()
        assert_greater_than(miner.getrawtransaction(spend_txid, True)["confirmations"], 0)
        self.check_mn_list(mns)

        # Register dmn again, with the collateral of dmn2
        # dmn must be added again to the list, and dmn2 must be removed
        dmn2 = mns.pop(randrange(len(mns)))  # pop one at random
        dmn_keys = [dmn.operator, dmn.operator_key]
        dmn2_keys = [dmn2.operator, dmn2.operator_key]
        self.log.info("Reactivating node %d reusing the collateral of node %d..." % (dmn.idx, dmn2.idx))
        mns.append(self.register_new_dmn(dmn.idx, self.minerPos, self.controllerPos, "external",
                                         outpoint=dmn2.collateral, op_addr_and_key=dmn_keys,
                                         fLock=False))
        miner.generate(1)
        self.sync_blocks()
        self.check_mn_list(mns)

        # Now try to register dmn2 again with an already-used IP
        self.log.info("Trying duplicate IP...")
        rand_idx = mns[randrange(len(mns))].idx
        assert_raises_rpc_error(-1, "bad-protx-dup-IP-address",
                                self.register_new_dmn, rand_idx, self.minerPos, self.controllerPos, "fund",
                                op_addr_and_key=dmn2_keys)

        # Now try with duplicate operator key
        self.log.info("Trying duplicate operator key...")
        dmn2b = create_new_dmn(dmn2.idx, controller, dummy_add, dmn_keys)
        assert_raises_rpc_error(-1, "bad-protx-dup-operator-key",
                                self.protx_register_fund, miner, controller, dmn2b, dummy_add, False)

        # Now try with duplicate owner key
        self.log.info("Trying duplicate owner key...")
        dmn2c = create_new_dmn(dmn2.idx, controller, dummy_add, dmn2_keys)
        dmn2c.owner = mns[randrange(len(mns))].owner
        assert_raises_rpc_error(-1, "bad-protx-dup-owner-key",
                                self.protx_register_fund, miner, controller, dmn2c, dummy_add, False)

        # Finally, register it properly. This time setting 10% of the reward for the operator
        op_rew = {"reward": 10.00, "address": self.nodes[dmn2.idx].getnewaddress()}
        self.log.info("Reactivating the node with a new registration (with operator reward)...")
        dmn2c = create_new_dmn(dmn2.idx, controller, dummy_add, dmn2_keys)
        self.protx_register_fund(miner, controller, dmn2c, dummy_add, True, op_rew)
        mns.append(dmn2c)
        time.sleep(1)
        self.sync_mempools([miner, controller])
        miner.generate(6)
        self.sync_blocks()
        self.check_mn_list(mns)     # 6 masternodes again

        # Test payments.
        # Mine 12 blocks and check that each masternode has been paid exactly twice.
        # Save last paid masternode. Check that it's the last paid also after the 12 blocks.
        # Note: dmn2 sends (2 * 0.3 PIV) to the operator, and (2 * 2.7 PIV) to the owner
        self.log.info("Testing masternode payments...")
        last_paid_mn = self.get_last_paid_mn()
        starting_balances = {"operator": self.get_addr_balance(self.nodes[dmn2c.idx], op_rew["address"])}
        for mn in mns:
            starting_balances[mn.payee] = self.get_addr_balance(controller, mn.payee)
        miner.generate(12)
        self.sync_blocks()
        for mn in mns:
            bal = self.get_addr_balance(controller, mn.payee)
            expected = starting_balances[mn.payee] + (Decimal('6.0') if mn.idx != dmn2c.idx else Decimal('5.4'))
            if bal != expected:
                raise Exception("Invalid balance (%s != %s) for node %d" % (bal, expected, mn.idx))
        self.log.info("All masternodes paid twice.")
        assert_equal(self.get_addr_balance(self.nodes[dmn2c.idx], op_rew["address"]),
                     starting_balances["operator"] + Decimal('0.6'))
        self.log.info("Operator paid twice.")
        assert_equal(last_paid_mn, self.get_last_paid_mn())
        self.log.info("Order preserved.")

        # Test invalid payment
        self.wait_until_mnsync_completed()   # just to be sure
        self.log.info("Testing invalid masternode payment...")
        mn_payee_script = miner.validateaddress(miner.getnewaddress())['scriptPubKey']
        block = self.create_block(mn_payee_script, miner.getblockcount(), miner.getbestblockhash())
        block.solve()
        assert_equal(miner.submitblock(bytes_to_hex_str(block.serialize())), "bad-cb-payee")

        self.log.info("All good.")


if __name__ == '__main__':
    DIP3Test().main()
