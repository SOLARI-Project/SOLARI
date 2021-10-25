(note: this is a temporary file, to be added-to by anybody, and moved to release-notes at release time)

PIVX Core version *version* is now available from:  <https://github.com/pivx-project/pivx/releases>

This is a new major version release, including various bug fixes and performance improvements, as well as updated translations.

Please report bugs using the issue tracker at github: <https://github.com/pivx-project/pivx/issues>


How to Upgrade
==============

If you are running an older version, shut it down. Wait until it has completely shut down (which might take a few minutes for older versions), then run the installer (on Windows) or just copy over /Applications/PIVX-Qt (on Mac) or pivxd/pivx-qt (on Linux).

Notable Changes
==============

(Developers: add your notes here as part of your pull requests whenever possible)


### Deprecated autocombinerewards RPC Command

The `autocombinerewards` RPC command was soft-deprecated in v5.3.0 and replaced with explicit setter/getter commands `setautocombinethreshold`/`getautocombinethreshold`. PIVX Core, by default, will no longer accept the `autocombinerewards` command, returning a deprecation error, unless the `pivxd`/`pivx-qt` is started with the `-deprecatedrpc=autocombinerewards` option.

This command will be fully removed in v6.0.0.

*version* Change log
==============

Detailed release notes follow. This overview includes changes that affect behavior, not code moves, refactors and string updates. For convenience in locating the code changes and accompanying discussion, both the pull request and git merge commit are mentioned.

### Core Features

### Build System

### P2P Protocol and Network Code

### GUI

### RPC/REST

### Wallet

### Miscellaneous

## Credits

Thanks to everyone who directly contributed to this release:


As well as everyone that helped translating on [Transifex](https://www.transifex.com/projects/p/pivx-project-translations/), the QA team during Testing and the Node hosts supporting our Testnet.
