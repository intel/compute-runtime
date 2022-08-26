/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "iaf_nl_api.h"

#include "shared/source/os_interface/linux/sys_calls.h"
#include "shared/source/utilities/directory.h"

#include <fcntl.h>
#include <netlink/attr.h>
#include <netlink/genl/ctrl.h>
#include <netlink/genl/family.h>
#include <sys/socket.h>

namespace L0 {

extern "C" {

// C linkage callback routines for Netlink
static int globalHandleMsg(struct nl_msg *msg, void *arg) {
    IafNlApi *pIafNlApi = reinterpret_cast<IafNlApi *>(arg);
    return pIafNlApi->handleMsg(msg);
}

static int globalNlOperation(struct nl_cache_ops *ops, struct genl_cmd *cmd, struct genl_info *info, void *arg) {
    IafNlApi *pIafNlApi = reinterpret_cast<IafNlApi *>(arg);
    return pIafNlApi->nlOperation(ops, cmd, info);
}
}

static const struct {
    int id;
    char *name;
} iafCmds[] = {
    {.id = IAF_CMD_OP_DEVICE_ENUM, .name = const_cast<char *>("DEVICE_ENUM")},
    {.id = IAF_CMD_OP_PORT_ENABLE, .name = const_cast<char *>("PORT_ENABLE")},
    {.id = IAF_CMD_OP_PORT_DISABLE, .name = const_cast<char *>("PORT_DISABLE")},
    {.id = IAF_CMD_OP_PORT_STATE_QUERY, .name = const_cast<char *>("PORT_STATE_QUERY")},
    {.id = IAF_CMD_OP_PORT_USAGE_ENABLE, .name = const_cast<char *>("PORT_USAGE_ENABLE")},
    {.id = IAF_CMD_OP_PORT_USAGE_DISABLE, .name = const_cast<char *>("PORT_USAGE_DISABLE")},
    {.id = IAF_CMD_OP_PORT_USAGE_STATE_QUERY, .name = const_cast<char *>("PORT_USAGE_STATE_QUERY")},
    {.id = IAF_CMD_OP_PORT_BEACON_ENABLE, .name = const_cast<char *>("PORT_BEACON_ENABLE")},
    {.id = IAF_CMD_OP_PORT_BEACON_DISABLE, .name = const_cast<char *>("PORT_BEACON_DISABLE")},
    {.id = IAF_CMD_OP_PORT_BEACON_STATE_QUERY, .name = const_cast<char *>("PORT_BEACON_STATE_QUERY")},
    {.id = IAF_CMD_OP_PORT_ROUTED_QUERY, .name = const_cast<char *>("PORT_ROUTED_QUERY")},
    {.id = IAF_CMD_OP_REM_REQUEST, .name = const_cast<char *>("REM_REQUEST")},
    {.id = IAF_CMD_OP_ROUTING_GEN_QUERY, .name = const_cast<char *>("ROUTING_GEN_QUERY")},
    {.id = IAF_CMD_OP_FABRIC_DEVICE_PROPERTIES, .name = const_cast<char *>("FABRIC_DEVICE_PROPERTIES")},
    {.id = IAF_CMD_OP_SUB_DEVICE_PROPERTIES_GET, .name = const_cast<char *>("SUB_DEVICE_PROPERTIES_GET")},
    {.id = IAF_CMD_OP_FPORT_STATUS_QUERY, .name = const_cast<char *>("FPORT_STATUS_QUERY")},
    {.id = IAF_CMD_OP_SUB_DEVICE_TRAP_COUNT_QUERY, .name = const_cast<char *>("FPORT_EVENT_COUNT_QUERY")},
    {.id = IAF_CMD_OP_FPORT_PROPERTIES, .name = const_cast<char *>("FPORT_PROPERTIES")},
    {.id = IAF_CMD_OP_FPORT_XMIT_RECV_COUNTS, .name = const_cast<char *>("FPORT_XMIT_RECV_COUNTS")},
    {.id = 0, .name = nullptr},
};

struct PortProperties {
    uint64_t neighborGuid;
    uint8_t neighborPortNumber;
    int32_t enabledRxWidth;
    int64_t enabledRxBitrate;
    int32_t activeRxWidth;
    int64_t activeRxBitrate;
    int32_t enabledTxWidth;
    int64_t enabledTxBitrate;
    int32_t activeTxWidth;
    int64_t activeTxBitrate;
};

struct Subdevice {
    uint64_t guid;
    std::vector<uint8_t> ports;
};

struct Generation {
    uint32_t start;
    uint32_t end;
};

ze_result_t IafNlApi::allocMsg(const uint16_t cmdOp, struct nl_msg *&msg) {
    msg = pNlApi->nlmsgAlloc();
    if (nullptr == msg) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    if (nullptr == pNlApi->genlmsgPut(msg, NL_AUTO_PID, NL_AUTO_SEQ, familyId, 0, 0, cmdOp, 1)) {
        pNlApi->nlmsgFree(msg);
        msg = nullptr;
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    return ZE_RESULT_SUCCESS;
}

ze_result_t IafNlApi::issueRequest(const uint16_t cmdOp, const uint32_t fabricId, const uint32_t attachId, const uint8_t portNumber, void *pOutput) {
    ze_result_t result = init();
    if (ZE_RESULT_SUCCESS != result) {
        return result;
    }
    struct nl_msg *msg;
    result = allocMsg(cmdOp, msg);
    if (ZE_RESULT_SUCCESS == result) {
        pNlApi->nlaPutU32(msg, IAF_ATTR_FABRIC_ID, fabricId);
        pNlApi->nlaPutU8(msg, IAF_ATTR_SD_INDEX, attachId);
        pNlApi->nlaPutU8(msg, IAF_ATTR_FABRIC_PORT_NUMBER, portNumber);
        result = performTransaction(cmdOp, msg, pOutput);
    }
    cleanup();
    return result;
}

ze_result_t IafNlApi::issueRequest(const uint16_t cmdOp, const uint32_t fabricId, const uint32_t attachId, void *pOutput) {
    ze_result_t result = init();
    if (ZE_RESULT_SUCCESS != result) {
        return result;
    }
    struct nl_msg *msg;
    result = allocMsg(cmdOp, msg);
    if (ZE_RESULT_SUCCESS == result) {
        pNlApi->nlaPutU32(msg, IAF_ATTR_FABRIC_ID, fabricId);
        pNlApi->nlaPutU8(msg, IAF_ATTR_SD_INDEX, attachId);
        result = performTransaction(cmdOp, msg, pOutput);
    }
    cleanup();
    return result;
}

ze_result_t IafNlApi::issueRequest(const uint16_t cmdOp, const uint32_t fabricId, void *pOutput) {
    ze_result_t result = init();
    if (ZE_RESULT_SUCCESS != result) {
        return result;
    }
    struct nl_msg *msg;
    result = allocMsg(cmdOp, msg);
    if (ZE_RESULT_SUCCESS == result) {
        pNlApi->nlaPutU32(msg, IAF_ATTR_FABRIC_ID, fabricId);
        result = performTransaction(cmdOp, msg, pOutput);
    }
    cleanup();
    return result;
}

ze_result_t IafNlApi::issueRequest(const uint16_t cmdOp, void *pOutput) {
    ze_result_t result = init();
    if (ZE_RESULT_SUCCESS != result) {
        return result;
    }
    struct nl_msg *msg;
    result = allocMsg(cmdOp, msg);
    if (ZE_RESULT_SUCCESS == result) {
        result = performTransaction(cmdOp, msg, pOutput);
    }
    cleanup();
    return result;
}

ze_result_t IafNlApi::performTransaction(const uint16_t cmdOp, struct nl_msg *msg, void *pOutput) {
    Operation *pOperation = new Operation(cmdOp, pOutput);
    uint64_t context = reinterpret_cast<uint64_t>(pOperation);

    pNlApi->nlaPutU64(msg, IAF_ATTR_CMD_OP_CONTEXT, context);
    pNlApi->nlaPutU8(msg, IAF_ATTR_CMD_OP_MSG_TYPE, IAF_CMD_MSG_REQUEST);

    validContexts.push_back(context);
    if (0 > pNlApi->nlSendAuto(nlSock, msg)) {
        pOperation->done = true;
    }
    pNlApi->nlmsgFree(msg);

    while (!pOperation->done) {
        int res = pNlApi->nlRecvmsgsDefault(nlSock);
        if (0 > res) {
            if (-NLE_PERM == res) {
                pOperation->result = ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS;
            }
            pOperation->done = true;
        }
    }
    validContexts.remove(context);

    ze_result_t result = pOperation->result;
    delete pOperation;
    return result;
}

ze_result_t IafNlApi::handleResponse(const uint16_t cmdOp, struct genl_info *info, void *pOutput) {
    switch (cmdOp) {
    case IAF_CMD_OP_FPORT_STATUS_QUERY:
        return fPortStatusQueryRsp(info, pOutput);
    case IAF_CMD_OP_FPORT_XMIT_RECV_COUNTS:
        return getThroughputRsp(info, pOutput);
    case IAF_CMD_OP_PORT_STATE_QUERY:
        return portStateQueryRsp(info, pOutput);
    case IAF_CMD_OP_PORT_BEACON_STATE_QUERY:
        return portBeaconStateQueryRsp(info, pOutput);
    case IAF_CMD_OP_ROUTING_GEN_QUERY:
        return routingGenQueryRsp(info, pOutput);
    case IAF_CMD_OP_DEVICE_ENUM:
        return deviceEnumRsp(info, pOutput);
    case IAF_CMD_OP_FABRIC_DEVICE_PROPERTIES:
        return fabricDevicePropertiesRsp(info, pOutput);
    case IAF_CMD_OP_SUB_DEVICE_PROPERTIES_GET:
        return subdevicePropertiesGetRsp(info, pOutput);
    case IAF_CMD_OP_FPORT_PROPERTIES:
        return fportPropertiesRsp(info, pOutput);
    case IAF_CMD_OP_PORT_BEACON_ENABLE:
    case IAF_CMD_OP_PORT_BEACON_DISABLE:
    case IAF_CMD_OP_PORT_ENABLE:
    case IAF_CMD_OP_PORT_DISABLE:
    case IAF_CMD_OP_PORT_USAGE_ENABLE:
    case IAF_CMD_OP_PORT_USAGE_DISABLE:
    case IAF_CMD_OP_REM_REQUEST:
        return ZE_RESULT_SUCCESS;
    default:
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

ze_result_t IafNlApi::fPortStatusQueryRsp(struct genl_info *info, void *pOutput) {
    IafPortState *pState = reinterpret_cast<IafPortState *>(pOutput);

    const struct nlmsghdr *nlh = info->nlh;
    auto nla = pNlApi->nlmsgAttrdata(nlh, GENL_HDRLEN);
    auto rem = pNlApi->nlmsgAttrlen(nlh, GENL_HDRLEN);
    for (; pNlApi->nlaOk(nla, rem); nla = pNlApi->nlaNext(nla, &(rem))) {
        if (pNlApi->nlaType(nla) == IAF_ATTR_FABRIC_PORT) {
            auto cur = (struct nlattr *)pNlApi->nlaData(nla);
            auto rem = pNlApi->nlaLen(nla);
            for (; pNlApi->nlaOk(cur, rem); cur = pNlApi->nlaNext(cur, &(rem))) {
                switch (pNlApi->nlaType(cur)) {
                case IAF_ATTR_FPORT_HEALTH:
                    pState->healthStatus = pNlApi->nlaGetU8(cur);
                    break;
                case IAF_ATTR_FPORT_ISSUE_LQI:
                    pState->lqi = pNlApi->nlaGetU8(cur);
                    break;
                case IAF_ATTR_FPORT_ISSUE_LWD:
                    pState->lwd = pNlApi->nlaGetU8(cur);
                    break;
                case IAF_ATTR_FPORT_ISSUE_RATE:
                    pState->rate = pNlApi->nlaGetU8(cur);
                    break;
                case IAF_ATTR_FPORT_ERROR_FAILED:
                    pState->failed = pNlApi->nlaGetU8(cur);
                    break;
                case IAF_ATTR_FPORT_ERROR_ISOLATED:
                    pState->isolated = pNlApi->nlaGetU8(cur);
                    break;
                case IAF_ATTR_FPORT_ERROR_FLAPPING:
                    pState->flapping = pNlApi->nlaGetU8(cur);
                    break;
                case IAF_ATTR_FPORT_ERROR_LINK_DOWN:
                    pState->linkDown = pNlApi->nlaGetU8(cur);
                    break;
                case IAF_ATTR_FPORT_ERROR_DID_NOT_TRAIN:
                    pState->didNotTrain = pNlApi->nlaGetU8(cur);
                    break;
                default:
                    break;
                }
            }
        }
    }
    return ZE_RESULT_SUCCESS;
}

ze_result_t IafNlApi::getThroughputRsp(struct genl_info *info, void *pOutput) {
    IafPortThroughPut *pThroughput = reinterpret_cast<IafPortThroughPut *>(pOutput);
    pThroughput->txCounter = 0UL;
    pThroughput->rxCounter = 0UL;
    if (info->attrs[IAF_ATTR_FPORT_TX_BYTES]) {
        pThroughput->txCounter = pNlApi->nlaGetU64(info->attrs[IAF_ATTR_FPORT_TX_BYTES]);
    }
    if (info->attrs[IAF_ATTR_FPORT_RX_BYTES]) {
        pThroughput->rxCounter = pNlApi->nlaGetU64(info->attrs[IAF_ATTR_FPORT_RX_BYTES]);
    }
    return ZE_RESULT_SUCCESS;
}

ze_result_t IafNlApi::portStateQueryRsp(struct genl_info *info, void *pOutput) {
    bool *pEnabled = reinterpret_cast<bool *>(pOutput);
    const struct nlmsghdr *nlh = info->nlh;

    *pEnabled = false;
    auto nla = pNlApi->nlmsgAttrdata(nlh, GENL_HDRLEN);
    auto rem = pNlApi->nlmsgAttrlen(nlh, GENL_HDRLEN);
    for (; pNlApi->nlaOk(nla, rem); nla = pNlApi->nlaNext(nla, &(rem))) {
        if (pNlApi->nlaIsNested(nla)) {
            auto cur = (struct nlattr *)pNlApi->nlaData(nla);
            auto rem = pNlApi->nlaLen(nla);
            for (; pNlApi->nlaOk(cur, rem); cur = pNlApi->nlaNext(cur, &(rem))) {
                switch (pNlApi->nlaType(cur)) {
                case IAF_ATTR_ENABLED_STATE:
                    *pEnabled = (0 != pNlApi->nlaGetU8(cur));
                    break;
                default:
                    break;
                }
            }
        }
    }
    return ZE_RESULT_SUCCESS;
}

ze_result_t IafNlApi::portBeaconStateQueryRsp(struct genl_info *info, void *pOutput) {
    bool *pEnabled = reinterpret_cast<bool *>(pOutput);
    const struct nlmsghdr *nlh = info->nlh;

    *pEnabled = false;
    auto nla = pNlApi->nlmsgAttrdata(nlh, GENL_HDRLEN);
    auto rem = pNlApi->nlmsgAttrlen(nlh, GENL_HDRLEN);
    for (; pNlApi->nlaOk(nla, rem); nla = pNlApi->nlaNext(nla, &(rem))) {
        if (pNlApi->nlaIsNested(nla)) {
            auto cur = (struct nlattr *)pNlApi->nlaData(nla);
            auto rem = pNlApi->nlaLen(nla);
            for (; pNlApi->nlaOk(cur, rem); cur = pNlApi->nlaNext(cur, &(rem))) {
                switch (pNlApi->nlaType(cur)) {
                case IAF_ATTR_ENABLED_STATE:
                    *pEnabled = (0 != pNlApi->nlaGetU8(cur));
                    break;
                default:
                    break;
                }
            }
        }
    }
    return ZE_RESULT_SUCCESS;
}

ze_result_t IafNlApi::routingGenQueryRsp(struct genl_info *info, void *pOutput) {
    Generation *pGeneration = reinterpret_cast<Generation *>(pOutput);
    if (info->attrs[IAF_ATTR_ROUTING_GEN_START]) {
        pGeneration->start = pNlApi->nlaGetU32(info->attrs[IAF_ATTR_ROUTING_GEN_START]);
    }

    if (info->attrs[IAF_ATTR_ROUTING_GEN_END]) {
        pGeneration->end = pNlApi->nlaGetU32(info->attrs[IAF_ATTR_ROUTING_GEN_END]);
    }

    return ZE_RESULT_SUCCESS;
}

ze_result_t IafNlApi::deviceEnumRsp(struct genl_info *info, void *pOutput) {
    std::vector<uint32_t> *pFabricIds = reinterpret_cast<std::vector<uint32_t> *>(pOutput);
    const struct nlmsghdr *nlh = info->nlh;

    pFabricIds->clear();
    auto nla = pNlApi->nlmsgAttrdata(nlh, GENL_HDRLEN);
    auto rem = pNlApi->nlmsgAttrlen(nlh, GENL_HDRLEN);
    for (; pNlApi->nlaOk(nla, rem); nla = pNlApi->nlaNext(nla, &(rem))) {
        if (pNlApi->nlaIsNested(nla)) {
            auto cur = (struct nlattr *)pNlApi->nlaData(nla);
            auto rem = pNlApi->nlaLen(nla);
            for (; pNlApi->nlaOk(cur, rem); cur = pNlApi->nlaNext(cur, &(rem))) {
                switch (pNlApi->nlaType(cur)) {
                case IAF_ATTR_FABRIC_ID:
                    pFabricIds->push_back(pNlApi->nlaGetU32(cur));
                    break;
                default:
                    break;
                }
            }
        }
    }

    return ZE_RESULT_SUCCESS;
}

ze_result_t IafNlApi::fabricDevicePropertiesRsp(struct genl_info *info, void *pOutput) {
    uint32_t *pNumSubdevices = reinterpret_cast<uint32_t *>(pOutput);

    *pNumSubdevices = 0;
    if (info->attrs[IAF_ATTR_SUBDEVICE_COUNT]) {
        *pNumSubdevices = pNlApi->nlaGetU8(info->attrs[IAF_ATTR_SUBDEVICE_COUNT]);
    }

    return ZE_RESULT_SUCCESS;
}

ze_result_t IafNlApi::subdevicePropertiesGetRsp(struct genl_info *info, void *pOutput) {
    Subdevice *pSubdevice = reinterpret_cast<Subdevice *>(pOutput);
    const struct nlmsghdr *nlh = info->nlh;

    if (info->attrs[IAF_ATTR_GUID]) {
        pSubdevice->guid = pNlApi->nlaGetU64(info->attrs[IAF_ATTR_GUID]);
    }

    auto nla = pNlApi->nlmsgAttrdata(nlh, GENL_HDRLEN);
    auto rem = pNlApi->nlmsgAttrlen(nlh, GENL_HDRLEN);
    for (; pNlApi->nlaOk(nla, rem); nla = pNlApi->nlaNext(nla, &(rem))) {
        if (pNlApi->nlaType(nla) == IAF_ATTR_FABRIC_PORT) {
            uint8_t port = 0;
            uint8_t type = IAF_FPORT_TYPE_DISCONNECTED;

            auto cur = (struct nlattr *)pNlApi->nlaData(nla);
            auto rem = pNlApi->nlaLen(nla);
            for (; pNlApi->nlaOk(cur, rem); cur = pNlApi->nlaNext(cur, &(rem))) {
                switch (pNlApi->nlaType(cur)) {
                case IAF_ATTR_FABRIC_PORT_NUMBER:
                    port = pNlApi->nlaGetU8(cur);
                    break;
                case IAF_ATTR_FABRIC_PORT_TYPE:
                    type = pNlApi->nlaGetU8(cur);
                    break;
                default:
                    break;
                }
            }
            if (0 != port && IAF_FPORT_TYPE_DISCONNECTED != type) {
                pSubdevice->ports.push_back(port);
            }
        }
    }

    return ZE_RESULT_SUCCESS;
}

ze_result_t IafNlApi::fportPropertiesRsp(struct genl_info *info, void *pOutput) {
    PortProperties *pPortProperties = reinterpret_cast<PortProperties *>(pOutput);
    const struct nlmsghdr *nlh = info->nlh;

    auto nla = pNlApi->nlmsgAttrdata(nlh, GENL_HDRLEN);
    auto rem = pNlApi->nlmsgAttrlen(nlh, GENL_HDRLEN);
    for (; pNlApi->nlaOk(nla, rem); nla = pNlApi->nlaNext(nla, &(rem))) {
        if (pNlApi->nlaType(nla) == IAF_ATTR_FABRIC_PORT) {
            int32_t activeWidth = -1;
            int32_t degradedRxWidth = -1;
            int32_t degradedTxWidth = -1;
            int64_t activeBitrate = -1;
            int64_t maxBitrate = -1;

            auto cur = (struct nlattr *)pNlApi->nlaData(nla);
            auto rem = pNlApi->nlaLen(nla);
            for (; pNlApi->nlaOk(cur, rem); cur = pNlApi->nlaNext(cur, &(rem))) {
                switch (pNlApi->nlaType(cur)) {
                case IAF_ATTR_FPORT_NEIGHBOR_GUID:
                    pPortProperties->neighborGuid = pNlApi->nlaGetU64(cur);
                    break;
                case IAF_ATTR_FPORT_NEIGHBOR_PORT_NUMBER:
                    pPortProperties->neighborPortNumber = pNlApi->nlaGetU8(cur);
                    break;
                case IAF_ATTR_FPORT_LINK_WIDTH_ENABLED:
                    pPortProperties->enabledRxWidth = pPortProperties->enabledTxWidth = translateWidth(pNlApi->nlaGetU8(cur));
                    break;
                case IAF_ATTR_FPORT_LINK_WIDTH_ACTIVE:
                    activeWidth = translateWidth(pNlApi->nlaGetU8(cur));
                    break;
                case IAF_ATTR_FPORT_BPS_LINK_SPEED_ACTIVE:
                    activeBitrate = pNlApi->nlaGetU64(cur);
                    break;
                case IAF_ATTR_FPORT_LINK_WIDTH_DOWNGRADE_RX_ACTIVE:
                    degradedRxWidth = translateWidth(pNlApi->nlaGetU8(cur));
                    break;
                case IAF_ATTR_FPORT_LINK_WIDTH_DOWNGRADE_TX_ACTIVE:
                    degradedTxWidth = translateWidth(pNlApi->nlaGetU8(cur));
                    break;
                case IAF_ATTR_FPORT_BPS_LINK_SPEED_MAX:
                    maxBitrate = pNlApi->nlaGetU64(cur);
                    break;
                default:
                    break;
                }
            }
            if (-1 != degradedRxWidth) {
                pPortProperties->activeRxWidth = degradedRxWidth;
            } else {
                pPortProperties->activeRxWidth = activeWidth;
            }
            if (-1 != degradedTxWidth) {
                pPortProperties->activeTxWidth = degradedTxWidth;
            } else {
                pPortProperties->activeTxWidth = activeWidth;
            }
            if (0 != activeBitrate) {
                pPortProperties->activeRxBitrate = pPortProperties->activeTxBitrate = activeBitrate;
            }
            if (0 != maxBitrate) {
                pPortProperties->enabledRxBitrate = pPortProperties->enabledTxBitrate = maxBitrate;
            }
        }
    }

    return ZE_RESULT_SUCCESS;
}

int32_t IafNlApi::translateWidth(uint8_t width) {
    if (width & 0x8) {
        return 4;
    }
    if (width & 0x4) {
        return 3;
    }
    if (width & 0x2) {
        return 2;
    }
    if (width & 0x1) {
        return 1;
    }
    return -1;
}

ze_result_t IafNlApi::fPortStatusQuery(const IafPortId portId, IafPortState &state) {
    return issueRequest(IAF_CMD_OP_FPORT_STATUS_QUERY, portId.fabricId, portId.attachId, portId.portNumber, reinterpret_cast<void *>(&state));
}

ze_result_t IafNlApi::getThroughput(const IafPortId portId, IafPortThroughPut &throughput) {
    return issueRequest(IAF_CMD_OP_FPORT_XMIT_RECV_COUNTS, portId.fabricId, portId.attachId, portId.portNumber, reinterpret_cast<void *>(&throughput));
}

ze_result_t IafNlApi::portStateQuery(const IafPortId portId, bool &enabled) {
    return issueRequest(IAF_CMD_OP_PORT_STATE_QUERY, portId.fabricId, portId.attachId, portId.portNumber, reinterpret_cast<void *>(&enabled));
}

ze_result_t IafNlApi::portBeaconStateQuery(const IafPortId portId, bool &enabled) {
    return issueRequest(IAF_CMD_OP_PORT_BEACON_STATE_QUERY, portId.fabricId, portId.attachId, portId.portNumber, reinterpret_cast<void *>(&enabled));
}

ze_result_t IafNlApi::portBeaconEnable(const IafPortId portId) {
    return issueRequest(IAF_CMD_OP_PORT_BEACON_ENABLE, portId.fabricId, portId.attachId, portId.portNumber, nullptr);
}

ze_result_t IafNlApi::portBeaconDisable(const IafPortId portId) {
    return issueRequest(IAF_CMD_OP_PORT_BEACON_DISABLE, portId.fabricId, portId.attachId, portId.portNumber, nullptr);
}

ze_result_t IafNlApi::portEnable(const IafPortId portId) {
    return issueRequest(IAF_CMD_OP_PORT_ENABLE, portId.fabricId, portId.attachId, portId.portNumber, nullptr);
}

ze_result_t IafNlApi::portDisable(const IafPortId portId) {
    return issueRequest(IAF_CMD_OP_PORT_DISABLE, portId.fabricId, portId.attachId, portId.portNumber, nullptr);
}

ze_result_t IafNlApi::portUsageEnable(const IafPortId portId) {
    return issueRequest(IAF_CMD_OP_PORT_USAGE_ENABLE, portId.fabricId, portId.attachId, portId.portNumber, nullptr);
}

ze_result_t IafNlApi::portUsageDisable(const IafPortId portId) {
    return issueRequest(IAF_CMD_OP_PORT_USAGE_DISABLE, portId.fabricId, portId.attachId, portId.portNumber, nullptr);
}

ze_result_t IafNlApi::remRequest() {
    return issueRequest(IAF_CMD_OP_REM_REQUEST, nullptr);
}

ze_result_t IafNlApi::routingGenQuery(uint32_t &start, uint32_t &end) {
    Generation gen;
    gen.start = gen.end = 0;

    ze_result_t result = issueRequest(IAF_CMD_OP_ROUTING_GEN_QUERY, reinterpret_cast<void *>(&gen));
    if (ZE_RESULT_SUCCESS == result) {
        start = gen.start;
        end = gen.end;
    }
    return result;
}

ze_result_t IafNlApi::deviceEnum(std::vector<uint32_t> &fabricIds) {
    return issueRequest(IAF_CMD_OP_DEVICE_ENUM, reinterpret_cast<void *>(&fabricIds));
}

ze_result_t IafNlApi::fabricDeviceProperties(const uint32_t fabricId, uint32_t &numSubdevices) {
    return issueRequest(IAF_CMD_OP_FABRIC_DEVICE_PROPERTIES, fabricId, reinterpret_cast<void *>(&numSubdevices));
}

ze_result_t IafNlApi::subdevicePropertiesGet(const uint32_t fabricId, const uint32_t attachId, uint64_t &guid, std::vector<uint8_t> &ports) {
    Subdevice sd;
    sd.guid = 0;
    sd.ports.clear();

    ze_result_t result = issueRequest(IAF_CMD_OP_SUB_DEVICE_PROPERTIES_GET, fabricId, attachId, reinterpret_cast<void *>(&sd));
    if (ZE_RESULT_SUCCESS == result) {
        guid = sd.guid;
        ports = sd.ports;
    }
    return result;
}

ze_result_t IafNlApi::fportProperties(const IafPortId portId, uint64_t &neighborGuid, uint8_t &neighborPortNumber,
                                      IafPortSpeed &maxRxSpeed, IafPortSpeed &maxTxSpeed,
                                      IafPortSpeed &rxSpeed, IafPortSpeed &txSpeed) {
    PortProperties portProperties;
    portProperties.neighborGuid = 0UL;
    portProperties.neighborPortNumber = 0U;
    portProperties.enabledRxWidth = -1;
    portProperties.enabledRxBitrate = -1L;
    portProperties.activeRxWidth = -1;
    portProperties.activeRxBitrate = -1L;
    portProperties.enabledTxWidth = -1;
    portProperties.enabledTxBitrate = -1L;
    portProperties.activeTxWidth = -1;
    portProperties.activeTxBitrate = -1L;

    ze_result_t result = issueRequest(IAF_CMD_OP_FPORT_PROPERTIES, portId.fabricId, portId.attachId, portId.portNumber, reinterpret_cast<void *>(&portProperties));
    if (ZE_RESULT_SUCCESS == result) {
        neighborGuid = portProperties.neighborGuid;
        neighborPortNumber = portProperties.neighborPortNumber;
        maxRxSpeed.width = portProperties.enabledRxWidth;
        maxRxSpeed.bitRate = portProperties.enabledRxBitrate;
        rxSpeed.width = portProperties.activeRxWidth;
        rxSpeed.bitRate = portProperties.activeRxBitrate;
        maxTxSpeed.width = portProperties.enabledTxWidth;
        maxTxSpeed.bitRate = portProperties.enabledTxBitrate;
        txSpeed.width = portProperties.activeTxWidth;
        txSpeed.bitRate = portProperties.activeTxBitrate;
    }
    return result;
}

int IafNlApi::handleMsg(struct nl_msg *msg) {
    return pNlApi->genlHandleMsg(msg, reinterpret_cast<void *>(this));
}

int IafNlApi::nlOperation(struct nl_cache_ops *ops, struct genl_cmd *cmd, struct genl_info *info) {
    if (info->attrs[IAF_ATTR_CMD_OP_CONTEXT]) {
        uint64_t context = pNlApi->nlaGetU64(info->attrs[IAF_ATTR_CMD_OP_CONTEXT]);
        bool found = false;
        for (auto i : validContexts) {
            if (context == i) {
                found = true;
                break;
            }
        }
        if (!found) {
            return NL_STOP;
        }
        Operation *pOperation = reinterpret_cast<Operation *>(context);
        if (cmd->c_id == pOperation->cmdOp && info->attrs[IAF_ATTR_CMD_OP_RESULT] && info->attrs[IAF_ATTR_CMD_OP_MSG_TYPE]) {
            if ((pNlApi->nlaGetU8(info->attrs[IAF_ATTR_CMD_OP_MSG_TYPE]) == IAF_CMD_MSG_RESPONSE) && (pNlApi->nlaGetU8(info->attrs[IAF_ATTR_CMD_OP_RESULT]) == IAF_CMD_RSP_SUCCESS)) {
                pOperation->result = handleResponse(pOperation->cmdOp, info, pOperation->pOutput);
            }
        }
        pOperation->done = true;
    }
    return NL_OK;
}

ze_result_t IafNlApi::init() {
    if (!pNlApi) {
        return ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE;
    }
    if (!initted) {
        if (!pNlApi->loadEntryPoints()) {
            pNlApi.reset(nullptr);
            return ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE;
        }
        initted = true;
    }

    int retval = pNlApi->genlRegisterFamily(&ops);
    if (-NLE_EXIST == retval) {
        // Temporary error
        return ZE_RESULT_NOT_READY;
    } else if (!retval) {
        nlSock = pNlApi->nlSocketAlloc();
        if (nullptr != nlSock) {
            if (!pNlApi->genlConnect(nlSock)) {
                if (!pNlApi->genlOpsResolve(nlSock, &ops)) {
                    familyId = pNlApi->genlCtrlResolve(nlSock, std::string(ops.o_name).c_str());
                    if (0 <= familyId) {
                        if (!pNlApi->nlSocketModifyCb(nlSock, NL_CB_VALID, NL_CB_CUSTOM,
                                                      globalHandleMsg, reinterpret_cast<void *>(this))) {
                            pNlApi->nlSocketDisableSeqCheck(nlSock);
                            return ZE_RESULT_SUCCESS;
                        }
                    }
                }
            }
            pNlApi->nlSocketFree(nlSock);
            nlSock = nullptr;
        }
        pNlApi->genlUnregisterFamily(&ops);
    }
    return ZE_RESULT_ERROR_UNKNOWN;
}

void IafNlApi::cleanup() {
    pNlApi->nlSocketFree(nlSock);
    nlSock = nullptr;
    pNlApi->genlUnregisterFamily(&ops);
}

ze_result_t IafNlApi::initPorts(const uint32_t fabricId, std::vector<IafPort> &iafPorts) {
    uint32_t numSubdevices;

    if (ZE_RESULT_SUCCESS != fabricDeviceProperties(fabricId, numSubdevices)) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
    for (uint32_t subdevice = 0; subdevice < numSubdevices; subdevice++) {
        uint64_t guid;
        std::vector<uint8_t> ports;

        if (ZE_RESULT_SUCCESS != subdevicePropertiesGet(fabricId, subdevice, guid, ports)) {
            iafPorts.clear();
            return ZE_RESULT_ERROR_UNKNOWN;
        }
        for (auto port : ports) {
            IafPort p = {};
            IafPortSpeed rxSpeed, txSpeed;
            uint8_t neighbourPortNumber = 0;
            uint64_t neighbourGuid = 0;
            p.onSubdevice = numSubdevices > 1;
            p.portId.fabricId = fabricId;
            p.portId.attachId = subdevice;
            p.portId.portNumber = port;
            p.model = "XeLink";
            if (ZE_RESULT_SUCCESS != fportProperties(p.portId, neighbourGuid, neighbourPortNumber,
                                                     p.maxRxSpeed, p.maxTxSpeed, rxSpeed, txSpeed)) {
                iafPorts.clear();
                return ZE_RESULT_ERROR_UNKNOWN;
            }
            iafPorts.push_back(p);
        }
    }
    return ZE_RESULT_SUCCESS;
}

ze_result_t IafNlApi::getPorts(const std::string &devicePciPath, std::vector<IafPort> &ports) {

    std::string path;
    path.clear();
    std::vector<std::string> list = NEO::Directory::getFiles(devicePciPath);
    for (auto entry : list) {
        if ((entry.find(iafDirectory) != std::string::npos) ||
            (entry.find(iafDirectoryLegacy) != std::string::npos)) {
            path = entry + fabricIdFile;
            break;
        }
    }
    if (path.empty()) {
        // This device does not have a fabric
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }
    std::string fabricIdStr(64, '\0');
    int fd = NEO::SysCalls::open(path.c_str(), O_RDONLY);
    if (fd < 0) {
        return ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE;
    }

    ssize_t bytesRead = NEO::SysCalls::pread(fd, fabricIdStr.data(), fabricIdStr.size() - 1, 0);
    NEO::SysCalls::close(fd);
    if (bytesRead <= 0) {
        return ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE;
    }
    unsigned long myFabricId = 0UL;
    size_t end = 0;
    myFabricId = std::stoul(fabricIdStr, &end, 16);
    if (myFabricId > std::numeric_limits<uint32_t>::max()) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
    if (ZE_RESULT_SUCCESS != initPorts(static_cast<uint32_t>(myFabricId), ports)) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    return ZE_RESULT_SUCCESS;
}

IafNlApi::IafNlApi() {
    validContexts.clear();
    pNlApi.reset(new NlApi);

    memset(policy, 0, sizeof(nla_policy) * _IAF_ATTR_COUNT);

    policy[IAF_ATTR_CMD_OP_MSG_TYPE].type = NLA_U8;
    policy[IAF_ATTR_CMD_OP_CONTEXT].type = NLA_U64;
    policy[IAF_ATTR_CMD_OP_RESULT].type = NLA_U8;
    policy[IAF_ATTR_FABRIC_ID].type = NLA_U32;
    policy[IAF_ATTR_SD_INDEX].type = NLA_U8;
    policy[IAF_ATTR_ENTRIES].type = NLA_U16;
    policy[IAF_ATTR_FABRIC_DEVICE].type = NLA_NESTED;
    policy[IAF_ATTR_DEV_NAME].type = NLA_NUL_STRING;
    policy[IAF_ATTR_PARENT_DEV_NAME].type = NLA_NUL_STRING;
    policy[IAF_ATTR_SOCKET_ID].type = NLA_U8;
    policy[IAF_ATTR_PCI_SLOT_NUM].type = NLA_U8;
    policy[IAF_ATTR_SUBDEVICE_COUNT].type = NLA_U8;
    policy[IAF_ATTR_VERSION].type = NLA_U8;
    policy[IAF_ATTR_PRODUCT_TYPE].type = NLA_U8;
    policy[IAF_ATTR_SUB_DEVICE].type = NLA_NESTED;
    policy[IAF_ATTR_GUID].type = NLA_U64;
    policy[IAF_ATTR_EXTENDED_PORT_COUNT].type = NLA_U8;
    policy[IAF_ATTR_FABRIC_PORT_COUNT].type = NLA_U8;
    policy[IAF_ATTR_SWITCH_LIFETIME].type = NLA_U8;
    policy[IAF_ATTR_ROUTING_MODE_SUPPORTED].type = NLA_U8;
    policy[IAF_ATTR_ROUTING_MODE_ENABLED].type = NLA_U8;
    policy[IAF_ATTR_EHHANCED_PORT_0_PRESENT].type = NLA_U8;
    policy[IAF_ATTR_FABRIC_PORT].type = NLA_NESTED;
    policy[IAF_ATTR_FABRIC_PORT_NUMBER].type = NLA_U8;
    policy[IAF_ATTR_FABRIC_PORT_TYPE].type = NLA_U8;
    policy[IAF_ATTR_BRIDGE_PORT_NUMBER].type = NLA_U8;
    policy[IAF_ATTR_ENABLED_STATE].type = NLA_U8;
    policy[IAF_ATTR_ROUTING_GEN_START].type = NLA_U32;
    policy[IAF_ATTR_ROUTING_GEN_END].type = NLA_U32;
    policy[IAF_ATTR_FPORT_HEALTH].type = NLA_U8;
    policy[IAF_ATTR_FPORT_ISSUE_LQI].type = NLA_U8;
    policy[IAF_ATTR_FPORT_ISSUE_LWD].type = NLA_U8;
    policy[IAF_ATTR_FPORT_ISSUE_RATE].type = NLA_U8;
    policy[IAF_ATTR_FPORT_ERROR_FAILED].type = NLA_U8;
    policy[IAF_ATTR_FPORT_ERROR_ISOLATED].type = NLA_U8;
    policy[IAF_ATTR_FPORT_ERROR_FLAPPING].type = NLA_U8;
    policy[IAF_ATTR_FPORT_ERROR_LINK_DOWN].type = NLA_U8;
    policy[IAF_ATTR_FPORT_ERROR_DID_NOT_TRAIN].type = NLA_U8;
    policy[IAF_ATTR_SUB_DEVICE_TRAP_COUNT].type = NLA_U64;
    policy[IAF_ATTR_FPORT_PM_PORT_STATE].type = NLA_U8;
    policy[IAF_ATTR_FPORT_ROUTED].type = NLA_U8;
    policy[IAF_ATTR_FPORT_LOGICAL_STATE].type = NLA_U8;
    policy[IAF_ATTR_FPORT_PHYSICAL_STATE].type = NLA_U8;
    policy[IAF_ATTR_FPORT_FID].type = NLA_U32;
    policy[IAF_ATTR_FPORT_LINK_DOWN_COUNT].type = NLA_U32;
    policy[IAF_ATTR_FPORT_NEIGHBOR_GUID].type = NLA_U64;
    policy[IAF_ATTR_FPORT_PORT_ERROR_ACTION].type = NLA_U32;
    policy[IAF_ATTR_FPORT_NEIGHBOR_PORT_NUMBER].type = NLA_U8;
    policy[IAF_ATTR_FPORT_PORT_LINK_MODE_ACTIVE].type = NLA_U8;
    policy[IAF_ATTR_FPORT_NEIGHBOR_LINK_DOWN_REASON].type = NLA_U8;
    policy[IAF_ATTR_FPORT_H_O_Q_LIFETIME].type = NLA_U8;
    policy[IAF_ATTR_FPORT_VL_CAP].type = NLA_U8;
    policy[IAF_ATTR_FPORT_OPERATIONAL_VLS].type = NLA_U8;
    policy[IAF_ATTR_FPORT_NEIGHBOR_MTU].type = NLA_U8;
    policy[IAF_ATTR_FPORT_LTP_CRC_MODE_SUPPORTED].type = NLA_U8;
    policy[IAF_ATTR_FPORT_LTP_CRC_MODE_ENABLED].type = NLA_U8;
    policy[IAF_ATTR_FPORT_LTP_CRC_MODE_ACTIVE].type = NLA_U8;
    policy[IAF_ATTR_FPORT_LINK_WIDTH_SUPPORTED].type = NLA_U8;
    policy[IAF_ATTR_FPORT_LINK_WIDTH_ENABLED].type = NLA_U8;
    policy[IAF_ATTR_FPORT_LINK_WIDTH_ACTIVE].type = NLA_U8;
    policy[IAF_ATTR_FPORT_LINK_SPEED_SUPPORTED].type = NLA_U8;
    policy[IAF_ATTR_FPORT_LINK_SPEED_ENABLED].type = NLA_U8;
    policy[IAF_ATTR_FPORT_LINK_SPEED_ACTIVE].type = NLA_U8;
    policy[IAF_ATTR_FPORT_LINK_WIDTH_DOWNGRADE_RX_ACTIVE].type = NLA_U8;
    policy[IAF_ATTR_FPORT_LINK_WIDTH_DOWNGRADE_TX_ACTIVE].type = NLA_U8;
    policy[IAF_ATTR_FPORT_LINK_INIT_REASON].type = NLA_U8;
    policy[IAF_ATTR_FPORT_LINK_DOWN_REASON].type = NLA_U8;
    policy[IAF_ATTR_FPORT_LQI_OFFLINE_DISABLED_REASON].type = NLA_U8;
    policy[IAF_ATTR_FPORT_LQI_NEIGHBOR_NORMAL].type = NLA_U8;
    policy[IAF_ATTR_FPORT_LINK_QUALITY_INDICATOR].type = NLA_U8;
    policy[IAF_ATTR_FPORT_BPS_LINK_SPEED_ACTIVE].type = NLA_U64;
    policy[IAF_ATTR_TIMESTAMP].type = NLA_U64;
    policy[IAF_ATTR_FPORT_TX_BYTES].type = NLA_U64;
    policy[IAF_ATTR_FPORT_RX_BYTES].type = NLA_U64;
    policy[IAF_ATTR_FPORT_BPS_LINK_SPEED_MAX].type = NLA_U64;
    policy[IAF_ATTR_FPORT_LQI_CHANGE_COUNT].type = NLA_U32;

    int i;
    memset(cmds, 0, sizeof(genl_cmd) * _IAF_CMD_OP_COUNT);
    for (i = 0; nullptr != iafCmds[i].name; i++) {
        cmds[i].c_id = iafCmds[i].id;
        cmds[i].c_name = iafCmds[i].name;
        cmds[i].c_maxattr = _IAF_ATTR_COUNT - 1,
        cmds[i].c_msg_parser = &globalNlOperation,
        cmds[i].c_attr_policy = policy;
    }

    ops.o_name = const_cast<char *>("iaf_ze");
    ops.o_hdrsize = 0U;
    ops.o_cmds = cmds;
    ops.o_ncmds = i;
}

IafNlApi::~IafNlApi() {
    if (nullptr != nlSock) {
        pNlApi->nlSocketFree(nlSock);
        nlSock = nullptr;
    }
}

} // namespace L0
