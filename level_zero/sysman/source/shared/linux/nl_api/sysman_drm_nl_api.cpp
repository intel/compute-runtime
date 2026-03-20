/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/shared/linux/nl_api/sysman_drm_nl_api.h"

#include "shared/source/helpers/debug_helpers.h"

namespace L0 {
namespace Sysman {

extern "C" {

static int globalDrmHandleMsg(struct nl_msg *msg, void *arg) {
    DrmNlApi *pDrmNlApi = reinterpret_cast<DrmNlApi *>(arg);
    return pDrmNlApi->handleMsg(msg);
}

static int drmNlOperationReadAll(struct nl_cache_ops *ops, struct genl_cmd *cmd, struct genl_info *info, void *arg) {
    DrmNlApi *drmNlApi = reinterpret_cast<DrmNlApi *>(arg);
    drmNlApi->getErrorListRsp(ops, cmd, info);
    return NL_OK;
}

static int drmNlOperationListNodes(struct nl_cache_ops *ops, struct genl_cmd *cmd, struct genl_info *info, void *arg) {
    DrmNlApi *drmNlApi = reinterpret_cast<DrmNlApi *>(arg);
    drmNlApi->listNodesRsp(ops, cmd, info);
    return NL_OK;
}
}

DrmNlApi::DrmNlApi() {
    pNlApi.reset(new NlApi);
    UNRECOVERABLE_IF(pNlApi.get() == nullptr);
    setupNlOperations();
    currentOperation = std::make_unique<Operation>(0, nullptr);
}

DrmNlApi::~DrmNlApi() {
    if (nullptr != nlSock) {
        pNlApi->nlSocketFree(nlSock);
        nlSock = nullptr;
    }
}

ze_result_t DrmNlApi::getErrorCounter(const uint32_t &nodeId,
                                      const uint32_t &errorId,
                                      DrmErrorCounter &errorCounter) {
    return issueRequestReadOne(DRM_RAS_CMD_GET_ERROR_COUNTER, nodeId, errorId, reinterpret_cast<void *>(&errorCounter));
}

ze_result_t DrmNlApi::getErrorsList(const uint32_t &nodeId,
                                    std::vector<DrmErrorCounter> &errorList) {
    return issueRequestQueryErrors(DRM_RAS_CMD_GET_ERROR_COUNTER, nodeId, reinterpret_cast<void *>(&errorList));
}

int DrmNlApi::handleMsg(struct nl_msg *msg) {
    return pNlApi->genlHandleMsg(msg, reinterpret_cast<void *>(this));
}

ze_result_t DrmNlApi::listNodes(std::vector<DrmRasNode> &nodeList) {
    return issueRequestListNodes(reinterpret_cast<void *>(&nodeList));
}

ze_result_t DrmNlApi::issueRequestListNodes(void *pOutput) {
    ze_result_t result = initConnection();
    if (ZE_RESULT_SUCCESS != result) {
        return result;
    }

    struct nl_msg *msg;
    result = allocMsg(DRM_RAS_CMD_LIST_NODES, true, msg);
    if (ZE_RESULT_SUCCESS == result) {
        result = performTransaction(DRM_RAS_CMD_LIST_NODES, msg, pOutput, false);
    }

    cleanupConnection(true);
    return result;
}

ze_result_t DrmNlApi::listNodesRsp(struct nl_cache_ops *ops,
                                   struct genl_cmd *cmd,
                                   struct genl_info *info) {
    std::vector<DrmRasNode> *nodeList = reinterpret_cast<std::vector<DrmRasNode> *>(currentOperation->pOutput);
    UNRECOVERABLE_IF(nodeList == nullptr);

    const struct nlmsghdr *nlh = info->nlh;
    auto nla = pNlApi->nlmsgAttrdata(nlh, GENL_HDRLEN);
    auto rem = pNlApi->nlmsgAttrlen(nlh, GENL_HDRLEN);

    DrmRasNode currentNode = {};
    while (pNlApi->nlaOk(nla, rem)) {
        switch (pNlApi->nlaType(nla)) {
        case DRM_RAS_A_NODE_ATTRS_NODE_ID:
            currentNode.nodeId = pNlApi->nlaGetU32(nla);
            break;
        case DRM_RAS_A_NODE_ATTRS_DEVICE_NAME:
            currentNode.deviceName = pNlApi->nlaGetString(nla);
            break;
        case DRM_RAS_A_NODE_ATTRS_NODE_NAME:
            currentNode.nodeName = pNlApi->nlaGetString(nla);
            break;
        case DRM_RAS_A_NODE_ATTRS_NODE_TYPE:
            currentNode.nodeType = pNlApi->nlaGetU32(nla);
            nodeList->push_back(currentNode);
            currentNode = {};
            break;
        }
        nla = pNlApi->nlaNext(nla, &rem);
    }

    currentOperation->done = true;
    return ZE_RESULT_SUCCESS;
}

ze_result_t DrmNlApi::getErrorListRsp(struct nl_cache_ops *ops, struct genl_cmd *cmd, struct genl_info *info) {
    if (currentOperation->parseSingleErrorCounter) {
        return getSingleErrorCounterRsp(ops, cmd, info);
    }

    std::vector<DrmErrorCounter> *errorList =
        reinterpret_cast<std::vector<DrmErrorCounter> *>(currentOperation->pOutput);
    UNRECOVERABLE_IF(errorList == nullptr);

    const struct nlmsghdr *nlh = info->nlh;
    auto nla = pNlApi->nlmsgAttrdata(nlh, GENL_HDRLEN);
    auto rem = pNlApi->nlmsgAttrlen(nlh, GENL_HDRLEN);

    // FLAT structure - one message = one error counter
    DrmErrorCounter currentError = {};
    while (pNlApi->nlaOk(nla, rem)) {
        switch (pNlApi->nlaType(nla)) {
        case DRM_RAS_A_ERROR_COUNTER_ATTRS_NODE_ID:
            currentError.nodeId = pNlApi->nlaGetU32(nla);
            break;
        case DRM_RAS_A_ERROR_COUNTER_ATTRS_ERROR_ID:
            currentError.errorId = pNlApi->nlaGetU32(nla);
            break;
        case DRM_RAS_A_ERROR_COUNTER_ATTRS_ERROR_NAME:
            currentError.errorName = pNlApi->nlaGetString(nla);
            break;
        case DRM_RAS_A_ERROR_COUNTER_ATTRS_ERROR_VALUE:
            currentError.errorValue = pNlApi->nlaGetU32(nla);
            // When we get VALUE, error counter is complete
            errorList->push_back(currentError);
            currentError = {};
            break;
        }
        nla = pNlApi->nlaNext(nla, &rem);
    }

    currentOperation->done = true;
    return ZE_RESULT_SUCCESS;
}

ze_result_t DrmNlApi::getSingleErrorCounterRsp(struct nl_cache_ops *ops, struct genl_cmd *cmd, struct genl_info *info) {
    DrmErrorCounter *errorCounter = reinterpret_cast<DrmErrorCounter *>(currentOperation->pOutput);
    UNRECOVERABLE_IF(errorCounter == nullptr);

    if (info->attrs[DRM_RAS_A_ERROR_COUNTER_ATTRS_NODE_ID]) {
        errorCounter->nodeId = pNlApi->nlaGetU32(
            info->attrs[DRM_RAS_A_ERROR_COUNTER_ATTRS_NODE_ID]);
    }
    if (info->attrs[DRM_RAS_A_ERROR_COUNTER_ATTRS_ERROR_ID]) {
        errorCounter->errorId = pNlApi->nlaGetU32(
            info->attrs[DRM_RAS_A_ERROR_COUNTER_ATTRS_ERROR_ID]);
    }
    if (info->attrs[DRM_RAS_A_ERROR_COUNTER_ATTRS_ERROR_NAME]) {
        errorCounter->errorName = pNlApi->nlaGetString(
            info->attrs[DRM_RAS_A_ERROR_COUNTER_ATTRS_ERROR_NAME]);
    }
    if (info->attrs[DRM_RAS_A_ERROR_COUNTER_ATTRS_ERROR_VALUE]) {
        errorCounter->errorValue = pNlApi->nlaGetU32(
            info->attrs[DRM_RAS_A_ERROR_COUNTER_ATTRS_ERROR_VALUE]);
    }

    currentOperation->done = true;
    return ZE_RESULT_SUCCESS;
}

ze_result_t DrmNlApi::initConnection() {

    if (!initted) {
        if (!pNlApi->loadEntryPoints()) {
            return ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE;
        }
        initted = true;
    }

    nlSock = pNlApi->nlSocketAlloc();
    if (nullptr == nlSock) {
        cleanupConnection(false);
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    int retval = pNlApi->genlConnect(nlSock);
    if (retval < 0) {
        cleanupConnection(true);
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    retval = pNlApi->genlRegisterFamily(&ops);
    if (retval != 0) {
        pNlApi->nlSocketFree(nlSock);
        nlSock = nullptr;
        if (-NLE_EXIST == retval) {
            return ZE_RESULT_NOT_READY;
        }
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    retval = pNlApi->genlOpsResolve(nlSock, &ops);
    if (retval < 0) {
        cleanupConnection(true);
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    familyId = pNlApi->genlCtrlResolve(nlSock, std::string(ops.o_name).c_str());
    if (0 > familyId) {
        cleanupConnection(true);
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    retval = pNlApi->nlSocketModifyCb(nlSock, NL_CB_VALID, NL_CB_CUSTOM,
                                      globalDrmHandleMsg, reinterpret_cast<void *>(this));
    if (retval < 0) {
        cleanupConnection(true);
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    return ZE_RESULT_SUCCESS;
}

void DrmNlApi::cleanupConnection(bool freeSocket) {
    if (freeSocket) {
        pNlApi->nlSocketFree(nlSock);
        nlSock = nullptr;
    }
    pNlApi->genlUnregisterFamily(&ops);
}

ze_result_t DrmNlApi::allocMsg(const uint16_t &cmdOp, const bool useDumpFlags, struct nl_msg *&msg) {
    msg = pNlApi->nlmsgAlloc();
    if (nullptr == msg) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    int flags = NLM_F_REQUEST | NLM_F_ACK;
    if (useDumpFlags) {
        flags |= NLM_F_ROOT | NLM_F_MATCH;
    }

    if (nullptr == pNlApi->genlmsgPut(msg, NL_AUTO_PORT, NL_AUTO_SEQ, familyId, 0, flags, cmdOp, 1)) {
        pNlApi->nlmsgFree(msg);
        msg = nullptr;
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    return ZE_RESULT_SUCCESS;
}

ze_result_t DrmNlApi::performTransaction(const uint16_t &cmdOp, struct nl_msg *msg, void *pOutput, const bool parseSingleErrorCounter) {
    UNRECOVERABLE_IF(currentOperation == nullptr);
    currentOperation->cmdOp = cmdOp;
    currentOperation->pOutput = pOutput;
    currentOperation->parseSingleErrorCounter = parseSingleErrorCounter;
    currentOperation->done = false;
    currentOperation->result = ZE_RESULT_SUCCESS;

    if (pNlApi->nlSendAuto(nlSock, msg) < 0) {
        pNlApi->nlmsgFree(msg);
        return ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS;
    }
    pNlApi->nlmsgFree(msg);

    while (!currentOperation->done) {
        int res = pNlApi->nlRecvmsgsDefault(nlSock);
        if (res < 0) {
            currentOperation->result = (res == -NLE_PERM) ? ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS : ZE_RESULT_ERROR_UNKNOWN;
            currentOperation->done = true;
        }
    }

    return currentOperation->result;
}

ze_result_t DrmNlApi::issueRequestReadOne(const uint16_t cmdOp, const uint32_t &nodeId, const uint32_t &errorId, void *pOutput) {
    ze_result_t result = initConnection();
    if (ZE_RESULT_SUCCESS != result) {
        return result;
    }

    struct nl_msg *msg;
    result = allocMsg(cmdOp, false, msg);
    if (ZE_RESULT_SUCCESS == result) {
        pNlApi->nlaPutU32(msg, DRM_RAS_A_ERROR_COUNTER_ATTRS_NODE_ID, nodeId);
        pNlApi->nlaPutU32(msg, DRM_RAS_A_ERROR_COUNTER_ATTRS_ERROR_ID, errorId);
        result = performTransaction(cmdOp, msg, pOutput, true);
    }

    cleanupConnection(true);
    return result;
}

ze_result_t DrmNlApi::issueRequestQueryErrors(const uint16_t &cmdOp, const uint32_t &nodeId, void *pOutput) {
    ze_result_t result = initConnection();
    if (ZE_RESULT_SUCCESS != result) {
        return result;
    }

    struct nl_msg *msg;
    result = allocMsg(cmdOp, true, msg);
    if (ZE_RESULT_SUCCESS == result) {
        pNlApi->nlaPutU32(msg, DRM_RAS_A_ERROR_COUNTER_ATTRS_NODE_ID, nodeId);
        result = performTransaction(cmdOp, msg, pOutput, false);
    }

    cleanupConnection(true);
    return result;
}

void DrmNlApi::setupNlOperations() {
    // Setup node attribute policy
    memset(nodePolicy, 0, sizeof(nla_policy) * (DRM_RAS_A_NODE_ATTRS_MAX + 1));
    nodePolicy[DRM_RAS_A_NODE_ATTRS_NODE_ID].type = NLA_U32;
    nodePolicy[DRM_RAS_A_NODE_ATTRS_DEVICE_NAME].type = NLA_NUL_STRING;
    nodePolicy[DRM_RAS_A_NODE_ATTRS_NODE_NAME].type = NLA_NUL_STRING;
    nodePolicy[DRM_RAS_A_NODE_ATTRS_NODE_TYPE].type = NLA_U32;

    // Setup error counter attribute policy
    memset(errorPolicy, 0, sizeof(nla_policy) * (DRM_RAS_A_ERROR_COUNTER_ATTRS_MAX + 1));
    errorPolicy[DRM_RAS_A_ERROR_COUNTER_ATTRS_NODE_ID].type = NLA_U32;
    errorPolicy[DRM_RAS_A_ERROR_COUNTER_ATTRS_ERROR_ID].type = NLA_U32;
    errorPolicy[DRM_RAS_A_ERROR_COUNTER_ATTRS_ERROR_NAME].type = NLA_NUL_STRING;
    errorPolicy[DRM_RAS_A_ERROR_COUNTER_ATTRS_ERROR_VALUE].type = NLA_U32;

    // Setup commands
    memset(newCmds, 0, sizeof(genl_cmd) * DRM_RAS_CMD_MAX);

    newCmds[0].c_id = DRM_RAS_CMD_LIST_NODES;
    newCmds[0].c_name = const_cast<char *>("LIST_NODES");
    newCmds[0].c_maxattr = DRM_RAS_A_NODE_ATTRS_MAX;
    newCmds[0].c_attr_policy = nodePolicy;
    newCmds[0].c_msg_parser = &drmNlOperationListNodes;

    newCmds[1].c_id = DRM_RAS_CMD_GET_ERROR_COUNTER;
    newCmds[1].c_name = const_cast<char *>("GET_ERROR_COUNTER");
    newCmds[1].c_maxattr = DRM_RAS_A_ERROR_COUNTER_ATTRS_MAX;
    newCmds[1].c_attr_policy = errorPolicy;
    newCmds[1].c_msg_parser = &drmNlOperationReadAll;

    // Use fixed family name
    ops.o_name = const_cast<char *>(DRM_RAS_FAMILY_NAME); // "drm-ras"
    ops.o_hdrsize = 0U;
    ops.o_cmds = newCmds;
    ops.o_ncmds = 2;
}

} // namespace Sysman
} // namespace L0
