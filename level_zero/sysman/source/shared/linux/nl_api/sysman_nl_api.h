/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/non_copyable_or_moveable.h"

#include <memory>
#include <netlink/attr.h>
#include <netlink/genl/ctrl.h>
#include <netlink/genl/family.h>
#include <netlink/genl/genl.h>
#include <netlink/genl/mngt.h>
#include <string>
#include <sys/socket.h>

namespace NEO {
class OsLibrary;
} // namespace NEO

namespace L0 {
namespace Sysman {

typedef int (*pGenlConnect)(struct nl_sock *);
typedef int (*pGenlCtrlResolve)(struct nl_sock *, const char *);
typedef int (*pGenlHandleMsg)(struct nl_msg *, void *);
typedef int (*pGenlOpsResolve)(struct nl_sock *, struct genl_ops *);
typedef int (*pGenlRegisterFamily)(struct genl_ops *);
typedef int (*pGenlUnregisterFamily)(struct genl_ops *);
typedef void *(*pGenlmsgPut)(struct nl_msg *, uint32_t, uint32_t, int, int, int, uint8_t, uint8_t);
typedef int (*pNlRecvmsgsDefault)(struct nl_sock *);
typedef int (*pNlSendAuto)(struct nl_sock *, struct nl_msg *);
typedef struct nl_sock *(*pNlSocketAlloc)();
typedef void (*pNlSocketDisableSeqCheck)(struct nl_sock *);
typedef void (*pNlSocketFree)(struct nl_sock *);
typedef int (*pNlSocketModifyCb)(struct nl_sock *, enum nl_cb_type, enum nl_cb_kind, nl_recvmsg_msg_cb_t, void *);
typedef void *(*pNlaData)(const struct nlattr *);
typedef uint32_t (*pNlaGetU32)(const struct nlattr *);
typedef uint64_t (*pNlaGetU64)(const struct nlattr *);
typedef uint8_t (*pNlaGetU8)(const struct nlattr *);
typedef int (*pNlaIsNested)(const struct nlattr *);
typedef int (*pNlaLen)(const struct nlattr *);
typedef struct nlattr *(*pNlaNext)(const struct nlattr *, int *);
typedef int (*pNlaOk)(const struct nlattr *, int);
typedef int (*pNlaPutU16)(struct nl_msg *, int, uint16_t);
typedef int (*pNlaPutU32)(struct nl_msg *, int, uint32_t);
typedef int (*pNlaPutU64)(struct nl_msg *, int, uint64_t);
typedef int (*pNlaPutU8)(struct nl_msg *, int, uint8_t);
typedef int (*pNlaType)(const struct nlattr *);
typedef struct nl_msg *(*pNlmsgAlloc)();
typedef struct nlattr *(*pNlmsgAttrdata)(const struct nlmsghdr *, int);
typedef int (*pNlmsgAttrlen)(const struct nlmsghdr *, int);
typedef void (*pNlmsgFree)(struct nl_msg *);
typedef struct nlmsghdr *(*pNlmsgHdr)(struct nl_msg *);
typedef struct nlattr *(*pNlaNestStart)(struct nl_msg *, int);
typedef int (*pNlaNestEnd)(struct nl_msg *, struct nlattr *);

class NlApi : public NEO::NonCopyableAndNonMovableClass {
  public:
    MOCKABLE_VIRTUAL int genlConnect(struct nl_sock *sock);
    MOCKABLE_VIRTUAL int genlCtrlResolve(struct nl_sock *sock, const char *name);
    MOCKABLE_VIRTUAL int genlHandleMsg(struct nl_msg *msg, void *arg);
    MOCKABLE_VIRTUAL int genlOpsResolve(struct nl_sock *sock, struct genl_ops *ops);
    MOCKABLE_VIRTUAL int genlRegisterFamily(struct genl_ops *ops);
    MOCKABLE_VIRTUAL int genlUnregisterFamily(struct genl_ops *ops);
    MOCKABLE_VIRTUAL void *genlmsgPut(struct nl_msg *msg, uint32_t port, uint32_t seq, int family, int hdrlen, int flags, uint8_t cmd, uint8_t version);
    MOCKABLE_VIRTUAL int nlRecvmsgsDefault(struct nl_sock *sock);
    MOCKABLE_VIRTUAL int nlSendAuto(struct nl_sock *sock, struct nl_msg *msg);
    MOCKABLE_VIRTUAL struct nl_sock *nlSocketAlloc();
    MOCKABLE_VIRTUAL void nlSocketDisableSeqCheck(struct nl_sock *sock);
    MOCKABLE_VIRTUAL void nlSocketFree(struct nl_sock *sock);
    MOCKABLE_VIRTUAL int nlSocketModifyCb(struct nl_sock *sock, enum nl_cb_type type, enum nl_cb_kind kind, nl_recvmsg_msg_cb_t cb, void *arg);
    MOCKABLE_VIRTUAL void *nlaData(const struct nlattr *attr);
    MOCKABLE_VIRTUAL uint32_t nlaGetU32(const struct nlattr *attr);
    MOCKABLE_VIRTUAL uint64_t nlaGetU64(const struct nlattr *attr);
    MOCKABLE_VIRTUAL uint8_t nlaGetU8(const struct nlattr *attr);
    MOCKABLE_VIRTUAL int nlaIsNested(const struct nlattr *attr);
    MOCKABLE_VIRTUAL int nlaLen(const struct nlattr *attr);
    MOCKABLE_VIRTUAL struct nlattr *nlaNext(const struct nlattr *attr, int *remaining);
    MOCKABLE_VIRTUAL int nlaOk(const struct nlattr *attr, int remaining);
    MOCKABLE_VIRTUAL int nlaPutU16(struct nl_msg *msg, int id, uint16_t data);
    MOCKABLE_VIRTUAL int nlaPutU32(struct nl_msg *msg, int id, uint32_t data);
    MOCKABLE_VIRTUAL int nlaPutU64(struct nl_msg *msg, int id, uint64_t data);
    MOCKABLE_VIRTUAL int nlaPutU8(struct nl_msg *msg, int id, uint8_t data);
    MOCKABLE_VIRTUAL int nlaType(const struct nlattr *attr);
    MOCKABLE_VIRTUAL struct nl_msg *nlmsgAlloc();
    MOCKABLE_VIRTUAL struct nlattr *nlmsgAttrdata(const struct nlmsghdr *hdr, int attr);
    MOCKABLE_VIRTUAL int nlmsgAttrlen(const struct nlmsghdr *hdr, int attr);
    MOCKABLE_VIRTUAL void nlmsgFree(struct nl_msg *msg);
    MOCKABLE_VIRTUAL struct nlmsghdr *nlmsgHdr(struct nl_msg *msg);
    MOCKABLE_VIRTUAL struct nlattr *nlaNestStart(struct nl_msg *msg, int id);
    MOCKABLE_VIRTUAL int nlaNestEnd(struct nl_msg *msg, struct nlattr *attr);

    bool isAvailable() { return nullptr != genlLibraryHandle.get(); }

    MOCKABLE_VIRTUAL bool loadEntryPoints();

    NlApi();
    MOCKABLE_VIRTUAL ~NlApi();

  protected:
    template <class T>
    bool getSymbolAddr(const std::string_view &name, T &sym);

    std::unique_ptr<NEO::OsLibrary> genlLibraryHandle;

    pGenlConnect genlConnectEntry = nullptr;
    pGenlCtrlResolve genlCtrlResolveEntry = nullptr;
    pGenlHandleMsg genlHandleMsgEntry = nullptr;
    pGenlOpsResolve genlOpsResolveEntry = nullptr;
    pGenlRegisterFamily genlRegisterFamilyEntry = nullptr;
    pGenlUnregisterFamily genlUnregisterFamilyEntry = nullptr;
    pGenlmsgPut genlmsgPutEntry = nullptr;
    pNlRecvmsgsDefault nlRecvmsgsDefaultEntry = nullptr;
    pNlSendAuto nlSendAutoEntry = nullptr;
    pNlSocketAlloc nlSocketAllocEntry = nullptr;
    pNlSocketDisableSeqCheck nlSocketDisableSeqCheckEntry = nullptr;
    pNlSocketFree nlSocketFreeEntry = nullptr;
    pNlSocketModifyCb nlSocketModifyCbEntry = nullptr;
    pNlaData nlaDataEntry = nullptr;
    pNlaGetU32 nlaGetU32Entry = nullptr;
    pNlaGetU64 nlaGetU64Entry = nullptr;
    pNlaGetU8 nlaGetU8Entry = nullptr;
    pNlaIsNested nlaIsNestedEntry = nullptr;
    pNlaLen nlaLenEntry = nullptr;
    pNlaNext nlaNextEntry = nullptr;
    pNlaOk nlaOkEntry = nullptr;
    pNlaPutU16 nlaPutU16Entry = nullptr;
    pNlaPutU32 nlaPutU32Entry = nullptr;
    pNlaPutU64 nlaPutU64Entry = nullptr;
    pNlaPutU8 nlaPutU8Entry = nullptr;
    pNlaType nlaTypeEntry = nullptr;
    pNlmsgAlloc nlmsgAllocEntry = nullptr;
    pNlmsgAttrdata nlmsgAttrdataEntry = nullptr;
    pNlmsgAttrlen nlmsgAttrlenEntry = nullptr;
    pNlmsgFree nlmsgFreeEntry = nullptr;
    pNlmsgHdr nlmsgHdrEntry = nullptr;
    pNlaNestStart nlaNestStartEntry = nullptr;
    pNlaNestEnd nlaNestEndEntry = nullptr;
};

} // namespace Sysman
} // namespace L0
