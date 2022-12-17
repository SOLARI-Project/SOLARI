PIVX Core version *v5.5.0* is now available from:  <https://github.com/pivx-project/pivx/releases>

This is a new major version release, including various bug fixes and performance improvements.

Please report bugs using the issue tracker at github: <https://github.com/pivx-project/pivx/issues>

Mandatory Update
==============

PIVX Core v5.5.0 is a mandatory update for all users. This release contains new consensus rules and improvements that are not backwards compatible with older versions. Users will need to update their clients before enforcement of this update goes into effect.

Update enforcement is currently scheduled to go into effect at the following time:

```
Mainnet: block 3,715,200. ~Jan 20th 2023.
```
Masternodes will need to be restarted once both the masternode daemon and the controller wallet have been upgraded.
If you are running a Masternode over Tor, please read the "How To Upgrade" section.

Note: In preparation for the enforcement, upgraded peers will start rejecting non-upgraded peers few days before the enforcement block height, we recommend everyone to be updated at minimum a week before the final time.

Please report bugs using the issue tracker at github: <https://github.com/pivx-project/pivx/issues>


How to Upgrade
==============

If you are running an older version, shut it down. Wait until it has completely shut down (which might take a few minutes for older versions), then run the installer (on Windows) or just copy over /Applications/PIVX-Qt (on Mac) or pivxd/pivx-qt (on Linux).

Notable Changes
==============

New Rewards Structure
---------------------

Starting at block 3,715,200 The rewards structure is being adjusted after many months of discussion, analysis, and agreement from many in the community.

The new rewards are as follows:

|      Party     | Current Rewards | New Rewards |
| :---           |      :---:      |    :---:    |
| Staker         |        2        |      4      |
| Masternode     |        3        |      6      |
| Budget<sup>1</sup>        |        1        |      10     |

1: Budget coins are **ONLY** created if there is a valid passing budget proposal requesting a specific amount of coins to be created. This occurs roughly once every 30 days.

Network Activity Toggle
-----------------------

The GUI wallet now allows for the user to enable/disable all network activity from the Settings page. This can be handy if, for example, you need to restart your router or modem.

RPC Changes
-----------

### getblock additional verbosity

The `getblock` command now has an additional verbosity level. The command's second parameter has been changed from a boolean to and integer to support this.

- If verbosity is 0, returns a string that is serialized, hex-encoded data for block 'hash'.
- If verbosity is 1, returns an Object with information about block `hash`.
- If verbosity is 2, returns an Object with information about block `hash` and information about each transaction.

### getnewshieldaddress label support

The `getnewshieldaddress` command now supports an optional `label` parameter. If specified, the text label will be added to the wallet's address book.

### New setnetworkactive command

A new `setnetworkactive` command has been added to enable/disable all network activity. The command specs are as follows:

```
setnetworkactive true|false
Enable/Disable all p2p network activity.

Result:
status: (boolean) The final network activity status

Examples:
setnetworkactive true
setnetworkactive false
```

### New scantxoutset command

A new `scantxoutset` command has been added that allows for advanced searching of the unspent transaction outputs (UTXOs). As this is a highly advanced and complex command that won't apply to most end users, the full command spec won't be detailed here.

Please refer to the detailed spec by running `help scantxoutset` or refer to #2780.

*v5.5.0* Change log
==============

Detailed release notes follow. This is the complete list of PRs for this release version. For convenience in locating the code changes and accompanying discussion, both the pull request and git merge commit are mentioned.

### Consensus
- #2763 `f849beaf2c2` [Consensus] New block rewards (tohsnoom)
- #2770 `8d3f950bb40` [Consensus] Set testnet new rewards height (Fuzzbawls)
- #2772 `9be813c8c6d` bugfix new block reward payment and verification (PeterL73)
- #2781 `414be936487` [Consensus] Set v5.5 activation height for mainnet (Fuzzbawls)
- #2784 `3c63095b5fe` [Consensus] Bump v5.5 activation height for mainnet (Fuzzbawls)

### RPC
- #2732 `7216dc980f4` Remove cs_main lock from blockToJSON (random-zebra)
- #2734 `f79e009fe37` [RPC] Consistently use ParseHashV to validate hash inputs in rpc (random-zebra)
- #2746 `e3d54848845` Fix mnconnect missing parameter and rename cxxtimer.hpp (random-zebra)
- #2774 `768dfb5a5f1` [RPC] add include_delegated to vRPCConvertParams (PeterL73)
- #2779 `abb888b0ac7` [RPC] Backport getblock verbosity (tecnovert)
- #2780 `8cc21819d80` [RPC] Backport scantxoutset from BitCoin Core v0.17.2 (PeterL73)

### Build Systems
- #2650 `7e2d1aff1f8` Update bls-signatures subtree to latest version (furszy)
- #2694 `060a0a5656d` [Build] Bump master to 5.4.99 (furszy)
- #2695 `2d37006acb1` [CLI][Build] Guard libevent error messages (Fuzzbawls)
- #2702 `f2b45fded0c` [Core][Build] Fix atomic data races in BLS operations (Fuzzbawls)
- #2710 `5c5bb069aa9` [Build] Clean up additional coverage and test data (Fuzzbawls)
- #2731 `04ae0156cca` [Build] Add CMake option to compile with debug flags (Fuzzbawls)
- #2738 `37635f49076` [Build] Fix multi-line comment warning (Fuzzbawls)
- #2740 `0e757ad2c9b` [Lint] Introduce lint-includes.sh script (Fuzzbawls)
- #2759 `d2a30915ca2` [Build][CMake] Use CMake internal instead of custom command for moc (Fuzzbawls)
- #2767 `291ef05f538` [GA] Run unit/functional tests on native macOS 11 (Fuzzbawls)
- #2769 `bb8b073d108` [GA] Disable Unit tests for win64 build in GA (Fuzzbawls)
- #2771 `f656ea9f9e5` [GA] Use explicit llvm version for macOS CMake build. (Fuzzbawls)

### GUI
- #2606 `f6145532b57` [Net][GUI][RPC] Add enable/disable network activity feature (João Barbosa)
- #2655 `7d237ad71e8` [GUI] Clean not longer used addnewaddressdialog. (furszy)
- #2673 `0f3ecb76a96` [Cleanup] Remove unused `RecentRequestsTableModel` and `PlatformStyle` (random-zebra)
- #2705 `5851e7574cf` [GUI][BugFix] Fix missing dashboard chart date filter initialization at startup (furszy)
- #2716 `2f8a0341b6b` [GUI] Don't error on zero amount for new cold staking address (Fuzzbawls)
- #2717 `6c33de8de9c` [GUI][Bug] Don't set a proposal's status to PASSING_NOT_FUNDED prematurely (Fuzzbawls)
- #2721 `bedf4de5948` [GUI] Decouple legacy MNs code from the GUI elements (furszy)
- #2727 `48759887441` [trivial] Fixing several QT related circular dependencies (furszy)
- #2760 `4c466f41984` [GUI][BUG] Properly validate proposal creation Name/URL (Fuzzbawls)
- #2768 `c439ec7a12c` [GUI] Fix Passing Not Funded logic (Fuzzbawls)

### Testing Frameworks
- #2685 `48741d8993d` [QA] Use single node in sapling_changeaddresses (random-zebra)
- #2701 `6ed103f2049` [test] sapling_key_import_export.py whitelist peers to speed up tx relay. (furszy)
- #2711 `73ac5f699e2` [Tests] Improve coverage for merkleblock and bloom code (Fuzzbawls)
- #2724 `bc3a9778ae3` [test] wallet_zapwallettxes.py whitelist peers to speed up tx relay. (furszy)
- #2725 `b6c6a8f36f2` [test] Rewrite rpc_wallet_encrypted_wallet_sapzkeys unit test as functional test (furszy)
- #2747 `4e46b143241` Add proper bech32 encoding/decoding utilities to python framework (random-zebra)
- #2766 `d10832cbc6b` [Tests] Native secp256k1 for functional tests (Fuzzbawls)

### Tier Two / Budget
- #2394 `d277bdffcd7` [budget] Clean pre-v5 consensus guards for proposal start block and max payment amount (furszy)
- #2648 `7937e8e2eb9` [wallet] Reopen CDBEnv after encryption instead of shutting down (furszy)
- #2657 `1709f6bf21d` [TierTwo] Fix more circular dependencies. (furszy)
- #2658 `5dc6b88fde4` [budget] Do not count votes twice after calling GetBudgetWithHighestVoteCount. (furszy)
- #2684 `ab30cd257ab` [Refactor] Move tier two managers initialization from init.cpp to tiertwo/init.cpp (furszy)
- #2686 `204770a6879` [TierTwo] Introduce network requests manager (furszy)
- #2690 `017953ee087` [Refactoring] Decouple and encapsulate tier two synchronization state (random-zebra)
- #2699 `da647d42e90` [Refactor] Deduplicate final commitment validity checks (random-zebra)
- #2723 `1eaa10c3884` Finish moving tier two initialization code from init.cpp to tiertwo/init.cpp and add disabledkg init flag (furszy)
- #2735 `3b59032bf98` Further TierTwo init encapsulation (furszy)
- #2749 `5a7b5141a7f` [Budget] Use space and time efficient data structure to check for already sent item sync requests (furszy)

### Wallet
- #2674 `ac52d737bcc` [Refactor] Prevent multiple calls to ExtractDestination (random-zebra)
- #2675 `4b8b541f5a3` [Wallet] Clear Shield notes witness cache before a full rescan (random-zebra)
- #2679 `6a992f853a6` [Wallet] Avoid second mapWallet lookup (random-zebra)
- #2680 `273bca3c563` [Refactor] Avoid double lookups in sapling maps (random-zebra)
- #2683 `50a6d260b6e` pre-filter coins by destination in AvailableCoins (random-zebra)
- #2704 `f2df53c3da2` [Wallet] Crypter code cleanup (furszy)
- #2707 `03f9946950b` [validation] Create zerocoin coinspends cache (furszy)
- #2736 `3aefab3c374` [wallet] fix ATMP call not contemplating the "missing inputs" error (furszy)
- #2750 `fc63a6fa0e8` [Refactor] Create coinstake outputs moved from stakeInput to the wallet class. (furszy)

### P2P Network
- #2632 `97728e56733` [net] Connman options encapsulation (furszy)
- #2639 `a8d228cccc6` [net] disallow sending messages before receiving verack + enable p2p_leak.py test (furszy)
- #2676 `8eb5087be1a` [Net_processing]  Do not log "notfound" msg as unknown. (furszy)
- #2706 `f2ec56945c2` [Net] Implement poll() (Patrick Strateman)
- #2739 `1f133b1321c` Guard "-maxconnections" arg if the node is a Masternode. (furszy)

### Misc Core
- #2681 `4645a54f0d8` [BUG] Fix possible crashes at shutdown (Matt Corallo)
- #2697 `4d2d7708293` Kill 'zpivchain' files and (furszy)
- #2700 `771c84b2d3d` avoid sensitive data in core dumps (Vasil Dimov)
- #2709 `5b795e8abcf` [Trivial] Fix LogPrintf arguments in WipeAccChecksums (Fuzzbawls)
- #2715 `fda28bd0605` [Refactor] Add LookupBlockIndex (furszy)
- #2726 `7b265f22e87` Improve PID file error and datadir handling (Hennadii Stepanov)
- #2728 `be27fbac09e` fix missing cs_main lock in AppInitMain and LoadExternalBlockFile (random-zebra)

### Docs
- #2656 `e844bd6cdf4` [Doc] Remove assets-attribution.md file. (furszy)
- #2693 `cfddfbf38bf` [Doc] Add v5.4.0 historical release notes (furszy)

## Credits

Thanks to everyone who directly contributed to this release:
- Alexander Block
- Andrew Chow
- Chun Kuan Lee
- Cory Fields
- Fuzzbawls
- Hennadii Stepanov
- James O'Beirne
- João Barbosa
- Luke Dashjr
- MarcoFalke
- Marko Bencun
- Martin Ankerl
- Matt Corallo
- Patrick Strateman
- PeterL73
- Pieter Wuille
- Tim Ruffing
- UdjinM6
- Vasil Dimov
- furszy
- lmanners
- practicalswift
- random-zebra
- tecnovert
- tohsnoom
- xdustinface

As well as everyone that helped translating on [Transifex](https://www.transifex.com/projects/p/pivx-project-translations/), the QA team during Testing and the Node hosts supporting our Testnet.
