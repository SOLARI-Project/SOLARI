// Copyright (c) 2020 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php.

#ifndef DESTINATION_IO_H
#define DESTINATION_IO_H

#include "script/standard.h"

// Regular + shielded addresses variant.
typedef boost::variant<CTxDestination, libzcash::SaplingPaymentAddress> CWDestination;

namespace Standard {

    std::string EncodeDestination(const CWDestination &address, const CChainParams::Base58Type addrType = CChainParams::PUBKEY_ADDRESS);

    CWDestination DecodeDestination(const std::string& strAddress);
    CWDestination DecodeDestination(const std::string& strAddress, bool& isStaking);
    CWDestination DecodeDestination(const std::string& strAddress, bool& isStaking, bool& isShielded);

    bool IsValidDestination(const CWDestination& dest);

    // boost::get wrapper
    const libzcash::SaplingPaymentAddress* GetShieldedDestination(const CWDestination& dest);
    const CTxDestination * GetTransparentDestination(const CWDestination& dest);

} // End Standard namespace

/**
 * Wrapper class for every supported address
 */
struct Destination {
public:
    explicit Destination() {}
    explicit Destination(const CTxDestination& _dest, bool _isP2CS) : dest(_dest), isP2CS(_isP2CS) {}

    CWDestination dest{CNoDestination()};
    bool isP2CS{false};

    Destination& operator=(const Destination& from)
    {
        this->dest = from.dest;
        this->isP2CS = from.isP2CS;
        return *this;
    }

    // Returns the key ID if Destination is a transparent "regular" destination
    const CKeyID* getKeyID()
    {
        const CTxDestination* regDest = Standard::GetTransparentDestination(dest);
        return (regDest) ? boost::get<CKeyID>(regDest) : nullptr;
    }

    std::string ToString() const
    {
        if (!Standard::IsValidDestination(dest)) {
            // Invalid address
            return "";
        }
        return Standard::EncodeDestination(dest, isP2CS ? CChainParams::STAKING_ADDRESS : CChainParams::PUBKEY_ADDRESS);
    }
};

#endif //DESTINATION_IO_H
