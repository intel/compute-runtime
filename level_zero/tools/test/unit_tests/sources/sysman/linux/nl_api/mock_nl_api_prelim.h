/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/test/common/test_macros/mock_method_macros.h"

#include "level_zero/tools/source/sysman/linux/nl_api/iaf_nl_api.h"
#include "level_zero/tools/source/sysman/linux/nl_api/nl_api.h"
#include <level_zero/zes_api.h>

#include <map>

namespace L0 {
namespace ult {

class MyNlattr {
  public:
    uint16_t type = NLA_UNSPEC;
    uint64_t content;
    MyNlattr *next = nullptr;
    MyNlattr *nested = nullptr;
    MyNlattr *addNext() {
        next = new MyNlattr;
        return next;
    }
    MyNlattr() = default;
    ~MyNlattr() {
        if (nested)
            delete nested;
        if (next)
            delete next;
    }
};

class MockNlApi : public NlApi {
  public:
    std::vector<struct nl_sock *> mockNlSocketAllocReturnValue{};
    std::vector<struct genl_ops *> mockGenlRegisterFamilyArgumentValue{};
    std::vector<struct nl_msg *> mockNlmsgAllocReturnValue{};
    std::vector<void *> mockGenlmsgPutReturnValue{};
    std::vector<int> mockGenlRegisterFamilyReturnValue{};
    std::vector<int> mockGenlConnectReturnValue{};
    std::vector<int> mockGenlOpsResolveReturnValue{};
    std::vector<int> mockGenCtrlResolveReturnValue{};
    std::vector<int> mockNlSocketModifyCbReturnValue{};
    std::vector<int> mockNlRecvmsgsDefaultReturnValue{};
    std::vector<int> mockGenlUnregisterFamilyReturnValue{};
    std::vector<int> mockNlSendAutoReturnValue{};
    std::vector<bool> mockLoadEntryPointsReturnValue{};
    std::vector<bool> isMockGenlRegisterFamilyRepeatedCall{};
    bool isRepeated = false;

    int genlUnregisterFamily(struct genl_ops *ops) override;
    int genlHandleMsg(struct nl_msg *msg, void *arg) override;
    int genlOpsResolve(struct nl_sock *sock, struct genl_ops *ops) override;
    int genlCtrlResolve(struct nl_sock *sock, const char *name) override;
    void *genlmsgPut(struct nl_msg *msg, uint32_t port, uint32_t seq, int family, int hdrlen, int flags, uint8_t cmd, uint8_t version) override;
    int nlRecvmsgsDefault(struct nl_sock *sock) override;
    void *nlaData(const struct nlattr *attr) override;
    uint32_t nlaGetU32(const struct nlattr *attr) override;
    uint64_t nlaGetU64(const struct nlattr *attr) override;
    uint8_t nlaGetU8(const struct nlattr *attr) override;
    int nlaIsNested(const struct nlattr *attr) override;
    int nlaLen(const struct nlattr *attr) override;
    struct nlattr *nlaNext(const struct nlattr *attr, int *remaining) override;
    int nlaOk(const struct nlattr *attr, int remaining) override;
    int nlaPutU16(struct nl_msg *msg, int id, uint16_t data) override;
    int nlaPutU32(struct nl_msg *msg, int id, uint32_t data) override;
    int nlaPutU64(struct nl_msg *msg, int id, uint64_t data) override;
    int nlaPutU8(struct nl_msg *msg, int id, uint8_t data) override;
    int nlaType(const struct nlattr *attr) override;
    struct nl_msg *nlmsgAlloc() override;
    struct nlattr *nlmsgAttrdata(const struct nlmsghdr *hdr, int attr) override;
    int nlmsgAttrlen(const struct nlmsghdr *hdr, int attr) override;
    void nlmsgFree(struct nl_msg *msg) override;
    int nlSendAuto(struct nl_sock *sock, struct nl_msg *msg) override;
    bool loadEntryPoints() override;
    int genlRegisterFamily(struct genl_ops *ops) override;
    struct nl_sock *nlSocketAlloc() override;
    int genlConnect(struct nl_sock *sock) override;
    int nlSocketModifyCb(struct nl_sock *sock, enum nl_cb_type type, enum nl_cb_kind kind, nl_recvmsg_msg_cb_t cb, void *arg) override;
    struct nlattr *nlaNestStart(struct nl_msg *msg, int id) override;
    int nlaNestEnd(struct nl_msg *msg, struct nlattr *attr) override;

    ADDMETHOD_NOBASE(nlmsgHdr, struct nlmsghdr *, nullptr, (struct nl_msg * msg));
    ADDMETHOD_NOBASE_VOIDRETURN(nlSocketDisableSeqCheck, (struct nl_sock * sock));
    ADDMETHOD_NOBASE_VOIDRETURN(nlSocketFree, (struct nl_sock * sock));

    MockNlApi() = default;
    ~MockNlApi() override = default;

    nl_recvmsg_msg_cb_t myCallback;
    void *myArgP;

    static constexpr IafPortId testPortId{0x10000000U, 0x1U, 7};
    const uint64_t testGuid = 0x1234567887654321;

    const uint8_t linkSpeed12g = 1;
    const uint8_t linkSpeed25g = 2;
    const uint8_t linkSpeed53g = 4;
    const uint8_t linkSpeed90g = 8;

    const uint8_t linkWidth1x = 1;
    const uint8_t linkWidth2x = 2;
    const uint8_t linkWidth3x = 4;
    const uint8_t linkWidth4x = 8;

    bool useInvalidWidth = false;
    uint8_t linkSpeed = linkSpeed90g;
    bool addRxTxCounters = true;
    bool addRoutingGenStartEnd = true;
    bool addSubDeviceCount = true;
    bool addGUID = true;
    bool addPortZeroAndTypeDisconnected = false;
    enum iaf_fport_health fportHealth = IAF_FPORT_HEALTH_HEALTHY;
    uint8_t fportIssueLqi = 0;
    uint8_t fportIssueLwd = 0;
    uint8_t fportIssueRate = 0;
    uint8_t fportReasonFailed = 0;
    uint8_t fportReasonIsolated = 0;
    uint8_t fportReasonLinkDown = 0;
    uint8_t fportReasonDidNotTrain = 0;
    uint8_t fportReasonFlapping = 0;
    uint8_t testMultiPortThroughputCount = 1;

    const uint8_t testLinkWidthUnknown = -1;
    const uint8_t testLinkWidth1x = 1;
    const uint8_t testLinkWidth2x = 2;
    const uint8_t testLinkWidth3x = 3;
    const uint8_t testLinkWidth4x = 4;

    uint32_t genStart = 1U; // Any non-zero, non-maxint value
    uint32_t genEnd = 2U;   // Any non-zero, non-maxint value

    uint64_t rxCounter = 100000000UL; // Any non-zero, non-maxlongint value
    uint64_t txCounter = 200000000UL; // Any non-zero, non-maxlongint value

    bool cmdMakeContextNotPresent = false;
    bool cmdMakeContextNotMatch = false;
    uint8_t cmdMsgType = IAF_CMD_MSG_RESPONSE;
    uint8_t cmdRspRslt = IAF_CMD_RSP_SUCCESS;

    std::vector<uint32_t> testFabricIds;

  private:
    int cmdIndex = 0;
    std::map<int, uint64_t> attribs;
    struct genl_ops *pOps = nullptr;
    bool isNestedAttr = false;

    void validateId(bool checkFabricId, bool checkAttachId, bool checkPortNumber);
    MyNlattr *copyId(struct genl_info &info, MyNlattr *next, bool checkFabricId, bool checkAttachId, bool checkPortNumber);
    MyNlattr *addAttrib(struct genl_info &info, MyNlattr *next, uint16_t type, uint64_t content);
    MyNlattr *addNested(struct genl_info &info, MyNlattr *next, uint16_t type, MyNlattr *nested);
    MyNlattr *addDefaultAttribs(struct genl_info &info, MyNlattr *next);
    MyNlattr *addPort(struct genl_info &info, MyNlattr *next, zes_fabric_port_id_t *port);
};

} // namespace ult
} // namespace L0
