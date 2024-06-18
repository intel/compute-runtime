/*
 * Copyright (C) 2021-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/test/unit_tests/sources/linux/nl_api/mock_nl_api.h"

#include "level_zero/sysman/source/shared/linux/nl_api/sysman_iaf_nl_api.h"

#include "gtest/gtest.h"

namespace L0 {
namespace Sysman {
namespace ult {

void MockNlApi::validateId(bool checkFabricId, bool checkAttachId, bool checkPortNumber) {
    uint64_t fabricId = 0UL;
    uint64_t attachId = 0UL;
    uint64_t portNumber = 0UL;
    if (checkFabricId) {
        fabricId = MockNlApi::testPortId.fabricId;
        EXPECT_EQ(attribs[IAF_ATTR_FABRIC_ID], fabricId);
    }
    if (checkAttachId) {
        attachId = MockNlApi::testPortId.attachId;
        EXPECT_EQ(attribs[IAF_ATTR_SD_INDEX], attachId);
    }
    if (checkPortNumber) {
        portNumber = MockNlApi::testPortId.portNumber;
        EXPECT_EQ(attribs[IAF_ATTR_FABRIC_PORT_NUMBER], portNumber);
    }
}

MyNlattr *MockNlApi::copyId(struct genl_info &info, MyNlattr *next, bool copyFabricId, bool copyAttachId, bool copyPortNumber) {
    if (copyFabricId) {
        next = addAttrib(info, next, IAF_ATTR_FABRIC_ID, attribs[IAF_ATTR_FABRIC_ID]);
    }
    if (copyAttachId) {
        next = addAttrib(info, next, IAF_ATTR_SD_INDEX, attribs[IAF_ATTR_SD_INDEX]);
    }
    if (copyPortNumber) {
        next = addAttrib(info, next, IAF_ATTR_FABRIC_PORT_NUMBER, attribs[IAF_ATTR_FABRIC_PORT_NUMBER]);
    }
    return next;
}

MyNlattr *MockNlApi::addAttrib(struct genl_info &info, MyNlattr *next, uint16_t type, uint64_t content) {
    next->type = type;
    next->content = content;
    if (nullptr == info.attrs[type]) {
        info.attrs[type] = reinterpret_cast<struct nlattr *>(next);
    }
    return next->addNext();
}

MyNlattr *MockNlApi::addNested(struct genl_info &info, MyNlattr *next, uint16_t type, MyNlattr *nested) {
    next->type = type;
    next->nested = nested;
    return next->addNext();
}

MyNlattr *MockNlApi::addDefaultAttribs(struct genl_info &info, MyNlattr *next) {
    if (true == cmdMakeContextNotPresent) {
        cmdMakeContextNotPresent = false;
    } else {
        next = addAttrib(info, next, IAF_ATTR_CMD_OP_CONTEXT, cmdMakeContextNotMatch ? 0 : attribs[IAF_ATTR_CMD_OP_CONTEXT]);
    }

    if (_IAF_CMD_MSG_TYPE_COUNT != cmdMsgType) {
        next = addAttrib(info, next, IAF_ATTR_CMD_OP_MSG_TYPE, cmdMsgType);
    }

    if (_IAF_CMD_RSP_COUNT != cmdRspRslt) {
        next = addAttrib(info, next, IAF_ATTR_CMD_OP_RESULT, cmdRspRslt);
    }
    return next;
}

MyNlattr *MockNlApi::addPort(struct genl_info &info, MyNlattr *next, zes_fabric_port_id_t *port) {
    next = addAttrib(info, next, IAF_ATTR_FABRIC_ID, port->fabricId);
    next = addAttrib(info, next, IAF_ATTR_SD_INDEX, port->attachId);
    next = addAttrib(info, next, IAF_ATTR_FABRIC_PORT_NUMBER, port->portNumber);
    return next;
}

void *MockNlApi::genlmsgPut(struct nl_msg *msg, uint32_t port, uint32_t seq, int family, int hdrlen, int flags, uint8_t cmd, uint8_t version) {
    if (!mockGenlmsgPutReturnValue.empty()) {
        void *returnPtr = mockGenlmsgPutReturnValue.front();
        if (isRepeated != true) {
            mockGenlmsgPutReturnValue.erase(mockGenlmsgPutReturnValue.begin());
        }
        return returnPtr;
    }

    for (int i = 0; i < pOps->o_ncmds; i++) {
        if (pOps->o_cmds[i].c_id == cmd) {
            cmdIndex = i;
        }
    }
    return &pOps->o_cmds[cmdIndex];
}

int MockNlApi::genlHandleMsg(struct nl_msg *msg, void *arg) {
    struct genl_info info;
    MyNlattr *head = new MyNlattr;
    MyNlattr *next = head;

    info.attrs = new struct nlattr *[_IAF_ATTR_COUNT];
    for (auto i = 0; i < _IAF_ATTR_COUNT; i++) {
        info.attrs[i] = nullptr;
    }
    next = addDefaultAttribs(info, next);

    switch (pOps->o_cmds[cmdIndex].c_id) {
    case IAF_CMD_OP_FPORT_STATUS_QUERY:
        validateId(true, true, true);

        for (auto i = 0U; i < 1; i++) {
            MyNlattr *nested = new MyNlattr;
            MyNlattr *nextNested = nested;
            nextNested = addAttrib(info, nextNested, IAF_ATTR_FPORT_HEALTH, fportHealth);
            nextNested = addAttrib(info, nextNested, IAF_ATTR_FPORT_ISSUE_LQI, fportIssueLqi);
            nextNested = addAttrib(info, nextNested, IAF_ATTR_FPORT_ISSUE_LWD, fportIssueLwd);
            nextNested = addAttrib(info, nextNested, IAF_ATTR_FPORT_ISSUE_RATE, fportIssueRate);
            nextNested = addAttrib(info, nextNested, IAF_ATTR_FPORT_ERROR_FAILED, fportReasonFailed);
            nextNested = addAttrib(info, nextNested, IAF_ATTR_FPORT_ERROR_ISOLATED, fportReasonIsolated);
            nextNested = addAttrib(info, nextNested, IAF_ATTR_FPORT_ERROR_LINK_DOWN, fportReasonLinkDown);
            nextNested = addAttrib(info, nextNested, IAF_ATTR_FPORT_ERROR_FLAPPING, fportReasonFlapping);
            addAttrib(info, nextNested, IAF_ATTR_FPORT_ERROR_DID_NOT_TRAIN, fportReasonDidNotTrain);
            next = addNested(info, next, IAF_ATTR_FABRIC_PORT, nested);
        }
        break;
    case IAF_CMD_OP_FABRIC_DEVICE_PROPERTIES:
        validateId(true, false, false);

        if (true == addSubDeviceCount) {
            addAttrib(info, next, IAF_ATTR_SUBDEVICE_COUNT, 2);
        }
        break;
    case IAF_CMD_OP_FPORT_PROPERTIES:
        validateId(true, false, false);

        for (auto i = 0U; i < 1; i++) {
            MyNlattr *nested = new MyNlattr;
            MyNlattr *nextNested = nested;
            nextNested = addAttrib(info, nextNested, IAF_ATTR_FPORT_NEIGHBOR_GUID, testGuid);
            nextNested = addAttrib(info, nextNested, IAF_ATTR_FPORT_NEIGHBOR_PORT_NUMBER, testPortId.portNumber);
            nextNested = addAttrib(info, nextNested, IAF_ATTR_FPORT_LINK_WIDTH_ENABLED, useInvalidWidth ? 0 : linkWidth1x);
            nextNested = addAttrib(info, nextNested, IAF_ATTR_FPORT_LINK_WIDTH_ACTIVE, useInvalidWidth ? 0 : linkWidth2x);
            nextNested = addAttrib(info, nextNested, IAF_ATTR_FPORT_BPS_LINK_SPEED_ACTIVE, linkSpeed);
            nextNested = addAttrib(info, nextNested, IAF_ATTR_FPORT_LINK_WIDTH_DOWNGRADE_RX_ACTIVE, useInvalidWidth ? 0 : linkWidth3x);
            nextNested = addAttrib(info, nextNested, IAF_ATTR_FPORT_LINK_WIDTH_DOWNGRADE_TX_ACTIVE, useInvalidWidth ? 0 : linkWidth4x);
            addAttrib(info, nextNested, IAF_ATTR_FPORT_BPS_LINK_SPEED_MAX, linkSpeed);
            next = addNested(info, next, IAF_ATTR_FABRIC_PORT, nested);
        }
        break;
    case IAF_CMD_OP_SUB_DEVICE_PROPERTIES_GET:
        validateId(true, false, false);

        if (true == addGUID) {
            next = addAttrib(info, next, IAF_ATTR_GUID, testGuid);
        }
        next = addAttrib(info, next, IAF_ATTR_EXTENDED_PORT_COUNT, 13);
        next = addAttrib(info, next, IAF_ATTR_FABRIC_PORT_COUNT, 8);
        next = addAttrib(info, next, IAF_ATTR_SWITCH_LIFETIME, 12);
        next = addAttrib(info, next, IAF_ATTR_ROUTING_MODE_SUPPORTED, 1);
        next = addAttrib(info, next, IAF_ATTR_ROUTING_MODE_ENABLED, 1);
        next = addAttrib(info, next, IAF_ATTR_EHHANCED_PORT_0_PRESENT, 1);
        for (auto i = addPortZeroAndTypeDisconnected ? 0U : 1U; i <= 8U; i++) {
            MyNlattr *nested = new MyNlattr;
            MyNlattr *nextNested = nested;
            nextNested = addAttrib(info, nextNested, IAF_ATTR_FABRIC_PORT_NUMBER, i);
            addAttrib(info, nextNested, IAF_ATTR_FABRIC_PORT_TYPE, addPortZeroAndTypeDisconnected ? IAF_FPORT_TYPE_DISCONNECTED : IAF_FPORT_TYPE_FIXED);
            next = addNested(info, next, IAF_ATTR_FABRIC_PORT, nested);
        }
        for (auto i = 9U; i <= 12U; i++) {
            next = addAttrib(info, next, IAF_ATTR_BRIDGE_PORT_NUMBER, i);
        }

        break;
    case IAF_CMD_OP_FPORT_XMIT_RECV_COUNTS:
        validateId(true, true, true);

        if (true == addRxTxCounters) {
            next = addAttrib(info, next, IAF_ATTR_FPORT_TX_BYTES, txCounter);
            addAttrib(info, next, IAF_ATTR_FPORT_RX_BYTES, rxCounter);
        }
        break;
    case IAF_CMD_OP_FPORT_THROUGHPUT:
        validateId(false, false, false);

        if (true == addRxTxCounters) {
            for (auto i = 0U; i < testMultiPortThroughputCount; i++) {
                MyNlattr *nested = new MyNlattr;
                MyNlattr *nextNested = nested;
                nextNested = addAttrib(info, nextNested, IAF_ATTR_FPORT_TX_BYTES, txCounter);
                nextNested = addAttrib(info, nextNested, IAF_ATTR_FPORT_RX_BYTES, rxCounter);
                nextNested = addAttrib(info, nextNested, IAF_ATTR_FABRIC_ID, testPortId.fabricId);
                nextNested = addAttrib(info, nextNested, IAF_ATTR_SD_INDEX, testPortId.attachId);
                addAttrib(info, nextNested, IAF_ATTR_FABRIC_PORT_NUMBER, testPortId.portNumber + i);
                next = addNested(info, next, IAF_ATTR_FABRIC_PORT_THROUGHPUT, nested);
            }
        }
        break;
    case IAF_CMD_OP_PORT_STATE_QUERY:
        validateId(true, true, true);

        for (auto i = 0U; i < 1; i++) {
            MyNlattr *nested = new MyNlattr;
            MyNlattr *nextNested = nested;
            nextNested = addAttrib(info, nextNested, IAF_ATTR_FABRIC_PORT_NUMBER, testPortId.portNumber);
            addAttrib(info, nextNested, IAF_ATTR_ENABLED_STATE, 1);
            next = addNested(info, next, IAF_ATTR_FABRIC_PORT, nested);
        }
        break;
    case IAF_CMD_OP_PORT_BEACON_STATE_QUERY:
        validateId(true, true, true);

        for (auto i = 0U; i < 1; i++) {
            MyNlattr *nested = new MyNlattr;
            MyNlattr *nextNested = nested;
            addAttrib(info, nextNested, IAF_ATTR_ENABLED_STATE, 1);
            next = addNested(info, next, IAF_ATTR_FABRIC_PORT, nested);
        }
        break;
    case IAF_CMD_OP_ROUTING_GEN_QUERY:
        validateId(false, false, false);

        if (true == addRoutingGenStartEnd) {
            next = addAttrib(info, next, IAF_ATTR_ROUTING_GEN_START, genStart);
            addAttrib(info, next, IAF_ATTR_ROUTING_GEN_END, genEnd);
        }
        break;
    case IAF_CMD_OP_DEVICE_ENUM:
        validateId(false, false, false);

        next = addAttrib(info, next, IAF_ATTR_ENTRIES, testFabricIds.size());
        for (auto i = 0U; i < testFabricIds.size(); i++) {
            MyNlattr *nested = new MyNlattr;
            MyNlattr *nextNested = nested;
            nextNested = addAttrib(info, nextNested, IAF_ATTR_FABRIC_ID, testFabricIds[i]);
            nextNested = addAttrib(info, nextNested, IAF_ATTR_DEV_NAME, 0UL);
            nextNested = addAttrib(info, nextNested, IAF_ATTR_PARENT_DEV_NAME, 0UL);
            nextNested = addAttrib(info, nextNested, IAF_ATTR_SOCKET_ID, 0UL);
            nextNested = addAttrib(info, nextNested, IAF_ATTR_PCI_SLOT_NUM, 0UL);
            nextNested = addAttrib(info, nextNested, IAF_ATTR_SUBDEVICE_COUNT, 0UL);
            nextNested = addAttrib(info, nextNested, IAF_ATTR_VERSION, 0UL);
            addAttrib(info, nextNested, IAF_ATTR_PRODUCT_TYPE, 0UL);
            next = addNested(info, next, IAF_ATTR_FABRIC_DEVICE, nested);
        }
        break;
    case IAF_CMD_OP_PORT_BEACON_ENABLE:
    case IAF_CMD_OP_PORT_BEACON_DISABLE:
    case IAF_CMD_OP_PORT_ENABLE:
    case IAF_CMD_OP_PORT_DISABLE:
    case IAF_CMD_OP_PORT_USAGE_ENABLE:
    case IAF_CMD_OP_PORT_USAGE_DISABLE:
        validateId(true, true, true);
        copyId(info, next, true, true, true);
        break;
    case IAF_CMD_OP_REM_REQUEST:
        validateId(false, false, false);
        break;
    }

    info.nlh = reinterpret_cast<struct nlmsghdr *>(head);
    bool succeeded = false;
    if (0 == (pOps->o_cmds[cmdIndex].c_msg_parser)(reinterpret_cast<struct nl_cache_ops *>(this), &pOps->o_cmds[cmdIndex], &info, arg)) {
        succeeded = true;
    }

    delete head;
    delete info.attrs;
    if (succeeded) {
        return NLE_SUCCESS;
    } else {
        return -NLE_FAILURE;
    }
}

int MockNlApi::genlOpsResolve(struct nl_sock *sock, struct genl_ops *ops) {

    int returnValue = 0;
    if (!mockGenlOpsResolveReturnValue.empty()) {
        returnValue = mockGenlOpsResolveReturnValue.front();
        if (isRepeated != true) {
            mockGenlOpsResolveReturnValue.erase(mockGenlOpsResolveReturnValue.begin());
        }
        return returnValue;
    }

    EXPECT_EQ(0U, ops->o_hdrsize);
    EXPECT_NE(nullptr, ops->o_cmds);

    attribs.clear();
    pOps = ops;

    return returnValue;
}

int MockNlApi::genlCtrlResolve(struct nl_sock *sock, const char *name) {
    int returnValue = 0;
    if (!mockGenCtrlResolveReturnValue.empty()) {
        returnValue = mockGenCtrlResolveReturnValue.front();
        if (isRepeated != true) {
            mockGenCtrlResolveReturnValue.erase(mockGenCtrlResolveReturnValue.begin());
        }
    }
    return returnValue;
}

int MockNlApi::nlRecvmsgsDefault(struct nl_sock *sock) {
    int returnValue = 0;
    if (!mockNlRecvmsgsDefaultReturnValue.empty()) {
        returnValue = mockNlRecvmsgsDefaultReturnValue.front();
        if (isRepeated != true) {
            mockNlRecvmsgsDefaultReturnValue.erase(mockNlRecvmsgsDefaultReturnValue.begin());
        }
        return returnValue;
    }

    struct nl_msg *msg = nlmsgAlloc();
    returnValue = myCallback(msg, myArgP);
    nlmsgFree(msg);
    return returnValue;
}

void *MockNlApi::nlaData(const struct nlattr *attr) {
    const MyNlattr *pAttr = reinterpret_cast<const MyNlattr *>(attr);
    return pAttr->nested;
}

uint32_t MockNlApi::nlaGetU32(const struct nlattr *attr) {
    const MyNlattr *pAttr = reinterpret_cast<const MyNlattr *>(attr);
    return pAttr->content & 0xFFFFFFFFUL;
}

uint64_t MockNlApi::nlaGetU64(const struct nlattr *attr) {
    const MyNlattr *pAttr = reinterpret_cast<const MyNlattr *>(attr);
    return pAttr->content;
}

uint8_t MockNlApi::nlaGetU8(const struct nlattr *attr) {
    const MyNlattr *pAttr = reinterpret_cast<const MyNlattr *>(attr);
    return pAttr->content & 0xFFUL;
}

int MockNlApi::nlaIsNested(const struct nlattr *attr) {
    const MyNlattr *pAttr = reinterpret_cast<const MyNlattr *>(attr);
    return nullptr != pAttr->nested;
}

int MockNlApi::nlaLen(const struct nlattr *attr) {
    if (nullptr == attr)
        return 0;
    const MyNlattr *pAttr = reinterpret_cast<const MyNlattr *>(attr);
    return sizeof(MyNlattr) + nlaLen(reinterpret_cast<const struct nlattr *>(pAttr->next)) + nlaLen(reinterpret_cast<const struct nlattr *>(pAttr->nested));
}

struct nlattr *MockNlApi::nlaNext(const struct nlattr *attr, int *remaining) {
    const MyNlattr *pAttr = reinterpret_cast<const MyNlattr *>(attr);
    *remaining = nlaLen(reinterpret_cast<const nlattr *>(pAttr->next));
    return reinterpret_cast<nlattr *>(pAttr->next);
}

int MockNlApi::nlaOk(const struct nlattr *attr, int remaining) {
    return nullptr != attr;
}

int MockNlApi::nlaPutU16(struct nl_msg *msg, int id, uint16_t data) {
    if (!isNestedAttr) {
        EXPECT_EQ(attribs.end(), attribs.find(id));
        attribs[id] = data;
    }
    EXPECT_EQ(NLA_U16, pOps->o_cmds[cmdIndex].c_attr_policy[id].type);
    return 0;
}

int MockNlApi::nlaPutU32(struct nl_msg *msg, int id, uint32_t data) {
    if (!isNestedAttr) {
        EXPECT_EQ(attribs.end(), attribs.find(id));
        attribs[id] = data;
    }
    EXPECT_EQ(NLA_U32, pOps->o_cmds[cmdIndex].c_attr_policy[id].type);
    return 0;
}

int MockNlApi::nlaPutU64(struct nl_msg *msg, int id, uint64_t data) {
    if (!isNestedAttr) {
        EXPECT_EQ(attribs.end(), attribs.find(id));
        attribs[id] = data;
    }
    EXPECT_EQ(NLA_U64, pOps->o_cmds[cmdIndex].c_attr_policy[id].type);
    return 0;
}

int MockNlApi::nlaPutU8(struct nl_msg *msg, int id, uint8_t data) {
    if (!isNestedAttr) {
        EXPECT_EQ(attribs.end(), attribs.find(id));
        attribs[id] = data;
    }
    EXPECT_EQ(NLA_U8, pOps->o_cmds[cmdIndex].c_attr_policy[id].type);
    return 0;
}

int MockNlApi::nlaType(const struct nlattr *attr) {
    const MyNlattr *pAttr = reinterpret_cast<const MyNlattr *>(attr);
    return pAttr->type;
}

struct nl_msg *MockNlApi::nlmsgAlloc() {
    struct nl_msg *returnPtr = nullptr;
    if (!mockNlmsgAllocReturnValue.empty()) {
        returnPtr = mockNlmsgAllocReturnValue.front();
        if (isRepeated != true) {
            mockNlmsgAllocReturnValue.erase(mockNlmsgAllocReturnValue.begin());
        }

        return returnPtr;
    }

    return reinterpret_cast<nl_msg *>(new char[128]);
}

struct nlattr *MockNlApi::nlmsgAttrdata(const struct nlmsghdr *hdr, int attr) {
    return reinterpret_cast<nlattr *>(const_cast<struct nlmsghdr *>(hdr));
}

struct nlattr *MockNlApi::nlaNestStart(struct nl_msg *msg, int id) {
    isNestedAttr = true;
    return reinterpret_cast<nlattr *>(msg);
}

int MockNlApi::nlaNestEnd(struct nl_msg *msg, struct nlattr *attr) {
    isNestedAttr = false;
    return 0;
}

int MockNlApi::nlmsgAttrlen(const struct nlmsghdr *hdr, int attr) {
    return nlaLen(nlmsgAttrdata(hdr, attr));
}

void MockNlApi::nlmsgFree(struct nl_msg *msg) {
    delete reinterpret_cast<char *>(msg); // NOLINT(clang-analyzer-unix.MismatchedDeallocator)
}

int MockNlApi::nlSendAuto(struct nl_sock *sock, struct nl_msg *msg) {
    int returnValue = 0;
    if (!mockNlSendAutoReturnValue.empty()) {
        returnValue = mockNlSendAutoReturnValue.front();
        mockNlSendAutoReturnValue.erase(mockNlSendAutoReturnValue.begin());
    }
    return returnValue;
}

bool MockNlApi::loadEntryPoints() {
    bool returnValue = true;
    if (!mockLoadEntryPointsReturnValue.empty()) {
        returnValue = mockLoadEntryPointsReturnValue.front();
        if (isRepeated != true) {
            mockLoadEntryPointsReturnValue.erase(mockLoadEntryPointsReturnValue.begin());
        }
    }
    return returnValue;
}

int MockNlApi::genlRegisterFamily(struct genl_ops *ops) {
    int returnValue = 0;
    if (!mockGenlRegisterFamilyReturnValue.empty()) {
        returnValue = mockGenlRegisterFamilyReturnValue.front();
        if (!mockGenlRegisterFamilyArgumentValue.empty()) {
            pOps = mockGenlRegisterFamilyArgumentValue.front();
        }

        if (isMockGenlRegisterFamilyRepeatedCall.front() != true) {
            if (!mockGenlRegisterFamilyArgumentValue.empty()) {
                mockGenlRegisterFamilyArgumentValue.erase(mockGenlRegisterFamilyArgumentValue.begin());
            }
            mockGenlRegisterFamilyReturnValue.erase(mockGenlRegisterFamilyReturnValue.begin());
            isMockGenlRegisterFamilyRepeatedCall.erase(isMockGenlRegisterFamilyRepeatedCall.begin());
        }
    }
    return returnValue;
}

struct nl_sock *MockNlApi::nlSocketAlloc() {
    struct nl_sock *returnPtr = nullptr;
    if (!mockNlSocketAllocReturnValue.empty()) {
        returnPtr = mockNlSocketAllocReturnValue.front();
        if (isRepeated != true) {
            mockNlSocketAllocReturnValue.erase(mockNlSocketAllocReturnValue.begin());
        }
    }
    return returnPtr;
}

int MockNlApi::genlConnect(struct nl_sock *sock) {
    int returnValue = 0;
    if (!mockGenlConnectReturnValue.empty()) {
        returnValue = mockGenlConnectReturnValue.front();
        if (isRepeated != true) {
            mockGenlConnectReturnValue.erase(mockGenlConnectReturnValue.begin());
        }
    }
    return returnValue;
}

int MockNlApi::nlSocketModifyCb(struct nl_sock *sock, enum nl_cb_type type, enum nl_cb_kind kind, nl_recvmsg_msg_cb_t cb, void *arg) {
    int returnValue = 0;
    myCallback = cb;
    myArgP = arg;
    if (!mockNlSocketModifyCbReturnValue.empty()) {
        returnValue = mockNlSocketModifyCbReturnValue.front();
        if (isRepeated != true) {
            mockNlSocketModifyCbReturnValue.erase(mockNlSocketModifyCbReturnValue.begin());
        }
    }
    return returnValue;
}

int MockNlApi::genlUnregisterFamily(struct genl_ops *ops) {
    int returnValue = 0;
    if (!mockGenlUnregisterFamilyReturnValue.empty()) {
        returnValue = mockGenlUnregisterFamilyReturnValue.front();
        if (isRepeated != true) {
            mockGenlUnregisterFamilyReturnValue.erase(mockGenlUnregisterFamilyReturnValue.begin());
        }
    }
    return returnValue;
}

} // namespace ult
} // namespace Sysman
} // namespace L0
