// Copyright (c) 2014-2016 The Dash developers
// Copyright (c) 2015-2020 The PIVX developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef ACTIVEMASTERNODE_H
#define ACTIVEMASTERNODE_H

#include "init.h"
#include "key.h"
#include "masternode.h"
#include "net.h"
#include "operationresult.h"
#include "sync.h"
#include "validationinterface.h"
#include "wallet/wallet.h"

struct CActiveMasternodeInfo;
class CActiveDeterministicMasternodeManager;

#define ACTIVE_MASTERNODE_INITIAL 0 // initial state
#define ACTIVE_MASTERNODE_SYNC_IN_PROCESS 1
#define ACTIVE_MASTERNODE_NOT_CAPABLE 3
#define ACTIVE_MASTERNODE_STARTED 4

extern CActiveMasternodeInfo activeMasternodeInfo;
extern CActiveDeterministicMasternodeManager* activeMasternodeManager;

struct CActiveMasternodeInfo {
    // Keys for the active Masternode
    CKeyID keyIDOperator;
    CKey keyOperator;
    // Initialized while registering Masternode
    uint256 proTxHash;
    COutPoint outpoint;
    CService service;
    void SetNullProTx() { proTxHash = UINT256_ZERO; outpoint.SetNull(); }
};

class CActiveDeterministicMasternodeManager : public CValidationInterface
{
public:
    enum masternode_state_t {
        MASTERNODE_WAITING_FOR_PROTX,
        MASTERNODE_POSE_BANNED,
        MASTERNODE_REMOVED,
        MASTERNODE_OPERATOR_KEY_CHANGED,
        MASTERNODE_PROTX_IP_CHANGED,
        MASTERNODE_READY,
        MASTERNODE_ERROR,
    };

private:
    masternode_state_t state{MASTERNODE_WAITING_FOR_PROTX};
    std::string strError;

public:
    virtual void UpdatedBlockTip(const CBlockIndex* pindexNew, const CBlockIndex* pindexFork, bool fInitialDownload);

    void Init();

    std::string GetStateString() const;
    std::string GetStatus() const;

    static bool IsValidNetAddr(const CService& addrIn);

private:
    bool GetLocalAddress(CService& addrRet);
};

// Responsible for initializing the masternode
OperationResult initMasternode(const std::string& strMasterNodePrivKey, const std::string& strMasterNodeAddr, bool isFromInit);

// Responsible for activating the Masternode and pinging the network (legacy MN list)
class CActiveMasternode
{
private:
    int status;
    std::string notCapableReason;

public:

    CActiveMasternode()
    {
        vin = nullopt;
        status = ACTIVE_MASTERNODE_INITIAL;
    }

    // Initialized by init.cpp
    // Keys for the main Masternode
    CPubKey pubKeyMasternode;
    CKey privKeyMasternode;

    // Initialized while registering Masternode
    Optional<CTxIn> vin;
    CService service;

    /// Manage status of main Masternode
    void ManageStatus();
    void ResetStatus();
    std::string GetStatusMessage() const;
    int GetStatus() const { return status; }

    /// Ping Masternode
    bool SendMasternodePing(std::string& errorMessage);
    /// Enable cold wallet mode (run a Masternode with no funds)
    bool EnableHotColdMasterNode(CTxIn& vin, CService& addr);

    void GetKeys(CKey& privKeyMasternode, CPubKey& pubKeyMasternode);
};

#endif
