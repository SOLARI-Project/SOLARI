// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Copyright (c) 2014-2015 The Dash developers
// Copyright (c) 2015-2020 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "rpc/client.h"

#include <set>
#include <stdint.h>

#include <univalue.h>


class CRPCConvertParam
{
public:
    std::string methodName; //! method whose params want conversion
    int paramIdx;           //! 0-based idx of param to convert
    std::string paramName; //!< parameter name
};

/**
 * Specifiy a (method, idx, name) here if the argument is a non-string RPC
 * argument and needs to be converted from JSON.
 *
 * @note Parameter indexes start from 0.
 */
static const CRPCConvertParam vRPCConvertParams[] = {
    { "stop", 0, "detach" },
    { "setmocktime", 0, "timestamp" },
    { "getaddednodeinfo", 0, "dummy" },
    { "setgenerate", 0, "generate" },
    { "setgenerate", 1, "genproclimit" },
    { "generate", 0, "nblocks" },
    { "generatetoaddress", 0, "nblocks" },
    { "getnetworkhashps", 0, "nblocks" },
    { "getnetworkhashps", 1, "height" },
    { "delegatestake", 1, "amount" },
    { "delegatestake", 3, "ext_owner" },
    { "delegatestake", 4, "include_delegated" },
    { "delegatestake", 5, "from_shield" },
    { "rawdelegatestake", 1, "amount" },
    { "rawdelegatestake", 3, "ext_owner" },
    { "rawdelegatestake", 4, "include_delegated" },
    { "rawdelegatestake", 5, "from_shield" },
    { "rawdelegatestake", 6, "force" },
    { "sendtoaddress", 1, "amount" },
    { "settxfee", 0, "amount" },
    { "getreceivedbyaddress", 1, "minconf" },
    { "listreceivedbyshieldaddress", 1, "minconf" },
    { "getreceivedbylabel", 1, "minconf" },
    { "listcoldutxos", 0, "not_whitelisted" },
    { "listdelegators", 0, "blacklist" },
    { "getsaplingnotescount", 0, "minconf" },
    { "listreceivedbyaddress", 0, "minconf" },
    { "listreceivedbyaddress", 1, "include_empty" },
    { "listreceivedbyaddress", 2, "include_watchonly" },
    { "listreceivedbylabel", 0, "minconf" },
    { "listreceivedbylabel", 1, "include_empty" },
    { "listreceivedbylabel", 2, "include_watchonly" },
    { "getbalance", 0, "minconf" },
    { "getbalance", 1, "include_watchonly" },
    { "getbalance", 2, "include_delegated" },
    { "getbalance", 3, "include_shield" },
    { "getshieldbalance", 1, "minconf" },
    { "getshieldbalance", 2, "include_watchonly" },
    { "rawshieldsendmany", 1, "amounts" },
    { "rawshieldsendmany", 2, "minconf" },
    { "rawshieldsendmany", 3, "fee" },
    { "shieldsendmany", 1, "amounts" },
    { "shieldsendmany", 2, "minconf" },
    { "shieldsendmany", 3, "fee" },
    { "getblockhash", 0, "height" },
    { "waitforblockheight", 0, "height" },
    { "waitforblockheight", 1, "timeout" },
    { "waitforblock", 1, "timeout" },
    { "waitfornewblock", 0, "timeout" },
    { "listtransactions", 1, "count" },
    { "listtransactions", 2, "from" },
    { "listtransactions", 3, "include_watchonly" },
    { "listtransactions", 4, "include_delegated" },
    { "listtransactions", 5, "include_cold" },
    { "walletpassphrase", 1, "timeout" },
    { "walletpassphrase", 2, "staking_only" },
    { "getblocktemplate", 0, "template_request" },
    { "listsinceblock", 1, "target_confirmations" },
    { "listsinceblock", 2, "include_watchonly" },
    { "sendmany", 1, "amounts" },
    { "sendmany", 2, "minconf" },
    { "addmultisigaddress", 0, "nrequired" },
    { "addmultisigaddress", 1, "keys" },
    { "createmultisig", 0, "nrequired" },
    { "createmultisig", 1, "keys" },
    { "listunspent", 0, "minconf" },
    { "listunspent", 1, "maxconf" },
    { "listunspent", 2, "addresses" },
    { "listunspent", 3, "watchonly_config" },
    { "listunspent", 4, "query_options" },
    { "listshieldunspent", 0, "minconf" },
    { "listshieldunspent", 1, "maxconf" },
    { "listshieldunspent", 2, "include_watchonly" },
    { "listshieldunspent", 3, "addresses" },
    { "logging", 0, "include" },
    { "logging", 1, "exclude" },
    { "getblock", 1, "verbose" },
    { "getblockheader", 1, "verbose" },
    { "gettransaction", 1, "include_watchonly" },
    { "getrawtransaction", 1, "verbose" },
    { "createrawtransaction", 0, "transactions" },
    { "createrawtransaction", 1, "outputs" },
    { "createrawtransaction", 2, "locktime" },
    { "fundrawtransaction", 1, "options" },
    { "signrawtransaction", 1, "prevtxs" },
    { "signrawtransaction", 2, "privkeys" },
    { "sendrawtransaction", 1, "allowhighfees" },
    { "sethdseed", 0, "newkeypool" },
    { "gettxout", 1, "n" },
    { "gettxout", 2, "include_mempool" },
    { "lockunspent", 0, "unlock" },
    { "lockunspent", 1, "transactions" },
    { "importprivkey", 2, "rescan" },
    { "importprivkey", 3, "is_staking_address" },
    { "importaddress", 2, "rescan" },
    { "importaddress", 3, "p2sh" },
    { "importpubkey", 2, "rescan" },
    { "importmulti", 0, "requests" },
    { "importmulti", 1, "options" },
    { "importsaplingkey", 1, "rescan" },
    { "importsaplingkey", 2, "height" },
    { "importsaplingviewingkey", 1, "rescan" },
    { "importsaplingviewingkey", 2, "height" },
    { "initmasternode", 2, "deterministic" },
    { "verifychain", 0, "nblocks" },
    { "keypoolrefill", 0, "newsize" },
    { "getrawmempool", 0, "verbose" },
    { "estimatefee", 0, "nblocks" },
    { "estimatesmartfee", 0, "nblocks" },
    { "prioritisetransaction", 1, "priority_delta" },
    { "prioritisetransaction", 2, "fee_delta" },
    { "setban", 2, "bantime" },
    { "setban", 3, "absolute" },
    { "spork", 1, "value" },
    { "preparebudget", 2, "npayments" },
    { "preparebudget", 3, "start" },
    { "preparebudget", 5, "montly_payment" },
    { "submitbudget", 2, "npayments" },
    { "submitbudget", 3, "start" },
    { "submitbudget", 5, "montly_payment" },
    { "startmasternode", 3, "lockwallet" },
    { "mnbudgetvote", 4, "legacy" },
    { "mnbudgetrawvote", 1, "collat_vout" },
    { "mnbudgetrawvote", 4, "time" },
    { "setstakesplitthreshold", 0, "value" },
    { "autocombinerewards", 0, "enable" },
    { "autocombinerewards", 1, "threshold" },
    { "setautocombinethreshold", 0, "enable" },
    { "setautocombinethreshold", 1, "threshold" },
    { "getblockindexstats", 0, "height" },
    { "getblockindexstats", 1, "range" },
    { "getfeeinfo", 0, "blocks" },
    { "getsupplyinfo", 0, "force_update" },
    { "rescanblockchain", 0, "start_height"},
    { "rescanblockchain", 1, "stop_height"},
    // Echo with conversion (For testing only)
    { "echojson", 0, "arg0" },
    { "echojson", 1, "arg1" },
    { "echojson", 2, "arg2" },
    { "echojson", 3, "arg3" },
    { "echojson", 4, "arg4" },
    { "echojson", 5, "arg5" },
    { "echojson", 6, "arg6" },
    { "echojson", 7, "arg7" },
    { "echojson", 8, "arg8" },
    { "echojson", 9, "arg9" },
};

class CRPCConvertTable
{
private:
    std::set<std::pair<std::string, int>> members;
    std::set<std::pair<std::string, std::string>> membersByName;

public:
    CRPCConvertTable();

    bool convert(const std::string& method, int idx)
    {
        return (members.count(std::make_pair(method, idx)) > 0);
    }
    bool convert(const std::string& method, const std::string& name)
    {
        return (membersByName.count(std::make_pair(method, name)) > 0);
    }
};

CRPCConvertTable::CRPCConvertTable()
{
    const unsigned int n_elem =
        (sizeof(vRPCConvertParams) / sizeof(vRPCConvertParams[0]));

    for (unsigned int i = 0; i < n_elem; i++) {
        members.insert(std::make_pair(vRPCConvertParams[i].methodName,
                                      vRPCConvertParams[i].paramIdx));
        membersByName.insert(std::make_pair(vRPCConvertParams[i].methodName,
                                            vRPCConvertParams[i].paramName));
    }
}

static CRPCConvertTable rpcCvtTable;

/** Non-RFC4627 JSON parser, accepts internal values (such as numbers, true, false, null)
 * as well as objects and arrays.
 */
UniValue ParseNonRFCJSONValue(const std::string& strVal)
{
    UniValue jVal;
    if (!jVal.read(std::string("[")+strVal+std::string("]")) ||
        !jVal.isArray() || jVal.size()!=1)
        throw std::runtime_error(std::string("Error parsing JSON:")+strVal);
    return jVal[0];
}

UniValue RPCConvertValues(const std::string &strMethod, const std::vector<std::string> &strParams)
{
    UniValue params(UniValue::VARR);

    for (unsigned int idx = 0; idx < strParams.size(); idx++) {
        const std::string& strVal = strParams[idx];

        if (!rpcCvtTable.convert(strMethod, idx)) {
            // insert string value directly
            params.push_back(strVal);
        } else {
            // parse string as JSON, insert bool/number/object/etc. value
            params.push_back(ParseNonRFCJSONValue(strVal));
        }
    }

    return params;
}

UniValue RPCConvertNamedValues(const std::string &strMethod, const std::vector<std::string> &strParams)
{
    UniValue params(UniValue::VOBJ);

    for (const std::string &s: strParams) {
        size_t pos = s.find("=");
        if (pos == std::string::npos) {
            throw(std::runtime_error("No '=' in named argument '"+s+"', this needs to be present for every argument (even if it is empty)"));
        }

        std::string name = s.substr(0, pos);
        std::string value = s.substr(pos+1);

        if (!rpcCvtTable.convert(strMethod, name)) {
            // insert string value directly
            params.pushKV(name, value);
        } else {
            // parse string as JSON, insert bool/number/object/etc. value
            params.pushKV(name, ParseNonRFCJSONValue(value));
        }
    }

    return params;
}
