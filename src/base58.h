// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Copyright (c) 2017-2020 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

/**
 * Why base-58 instead of standard base-64 encoding?
 * - Don't want 0OIl characters that look the same in some fonts and
 *      could be used to create visually identical looking account numbers.
 * - A string with non-alphanumeric characters is not as easily accepted as an account number.
 * - E-mail usually won't line-break if there's no punctuation to break at.
 * - Double-clicking selects the whole number as one word if it's all alphanumeric.
 */
#ifndef BITCOIN_BASE58_H
#define BITCOIN_BASE58_H

#include "chainparams.h"
#include "key.h"
#include "pubkey.h"
#include "script/standard.h"

#include <string>
#include <vector>

/**
 * Encode a byte sequence as a base58-encoded string.
 * pbegin and pend cannot be NULL, unless both are.
 */
std::string EncodeBase58(const unsigned char* pbegin, const unsigned char* pend);

/**
 * Encode a byte vector as a base58-encoded string
 */
std::string EncodeBase58(const std::vector<unsigned char>& vch);

/**
 * Decode a base58-encoded string (psz) into a byte vector (vchRet).
 * return true if decoding is successful.
 * psz cannot be NULL.
 */
bool DecodeBase58(const char* psz, std::vector<unsigned char>& vchRet);

/**
 * Decode a base58-encoded string (psz) into a string.
 * psz cannot be NULL.
 */
std::string DecodeBase58(const char* psz);

/**
 * Decode a base58-encoded string (str) into a byte vector (vchRet).
 * return true if decoding is successful.
 */
bool DecodeBase58(const std::string& str, std::vector<unsigned char>& vchRet);

/**
 * Encode a byte vector into a base58-encoded string, including checksum
 */
std::string EncodeBase58Check(const std::vector<unsigned char>& vchIn);

/**
 * Decode a base58-encoded string (psz) that includes a checksum into a byte
 * vector (vchRet), return true if decoding is successful
 */
inline bool DecodeBase58Check(const char* psz, std::vector<unsigned char>& vchRet);

/**
 * Decode a base58-encoded string (str) that includes a checksum into a byte
 * vector (vchRet), return true if decoding is successful
 */
bool DecodeBase58Check(const std::string& str, std::vector<unsigned char>& vchRet);

CKey DecodeSecret(const std::string& str);
std::string EncodeSecret(const CKey& key);

std::string EncodeDestination(const CTxDestination& dest, bool isStaking);
std::string EncodeDestination(const CTxDestination& dest, const CChainParams::Base58Type addrType = CChainParams::PUBKEY_ADDRESS);
// DecodeDestinationisStaking flag is set to true when the string arg is from an staking address
CTxDestination DecodeDestination(const std::string& str, bool& isStaking);
CTxDestination DecodeDestination(const std::string& str);
// Return true if the address is valid without care on the type.
bool IsValidDestinationString(const std::string& str);
// Return true if the address is valid and is following the fStaking flag type (true means that the destination must be a staking address, false the opposite).
bool IsValidDestinationString(const std::string& str, bool fStaking);
bool IsValidDestinationString(const std::string& str, bool fStaking, const CChainParams& params);

/**
 * Wrapper class for every supported address
 */
struct Destination {
public:
    explicit Destination() {}
    explicit Destination(const CTxDestination& _dest, bool _isP2CS) : dest(_dest), isP2CS(_isP2CS) {}

    CTxDestination dest{CNoDestination()};
    bool isP2CS{false};

    Destination& operator=(const Destination& from)
    {
        this->dest = from.dest;
        this->isP2CS = from.isP2CS;
        return *this;
    }

    std::string ToString()
    {
        if (!IsValidDestination(dest)) {
            // Invalid address
            return "";
        }
        return EncodeDestination(dest, isP2CS ? CChainParams::STAKING_ADDRESS : CChainParams::PUBKEY_ADDRESS);
    }
};

#endif // BITCOIN_BASE58_H
