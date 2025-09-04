/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "sysman_nl_api.h"

#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/os_interface/os_library.h"

namespace L0 {
namespace Sysman {

static constexpr std::string_view libgenlFile = "libnl-genl-3.so.200";
static constexpr std::string_view genlConnectRoutine = "genl_connect";
static constexpr std::string_view genlCtrlResolveRoutine = "genl_ctrl_resolve";
static constexpr std::string_view genlHandleMsgRoutine = "genl_handle_msg";
static constexpr std::string_view genlmsgPutRoutine = "genlmsg_put";
static constexpr std::string_view genlOpsResolveRoutine = "genl_ops_resolve";
static constexpr std::string_view genlRegisterFamilyRoutine = "genl_register_family";
static constexpr std::string_view genlUnregisterFamilyRoutine = "genl_unregister_family";
static constexpr std::string_view nlRecvmsgsDefaultRoutine = "nl_recvmsgs_default";
static constexpr std::string_view nlSendAutoRoutine = "nl_send_auto";
static constexpr std::string_view nlSocketAllocRoutine = "nl_socket_alloc";
static constexpr std::string_view nlSocketDisableSeqCheckRoutine = "nl_socket_disable_seq_check";
static constexpr std::string_view nlSocketFreeRoutine = "nl_socket_free";
static constexpr std::string_view nlSocketModifyCbRoutine = "nl_socket_modify_cb";
static constexpr std::string_view nlaDataRoutine = "nla_data";
static constexpr std::string_view nlaGetU32Routine = "nla_get_u32";
static constexpr std::string_view nlaGetU64Routine = "nla_get_u64";
static constexpr std::string_view nlaGetU8Routine = "nla_get_u8";
static constexpr std::string_view nlaIsNestedRoutine = "nla_is_nested";
static constexpr std::string_view nlaLenRoutine = "nla_len";
static constexpr std::string_view nlaNextRoutine = "nla_next";
static constexpr std::string_view nlaOkRoutine = "nla_ok";
static constexpr std::string_view nlaPutU16Routine = "nla_put_u16";
static constexpr std::string_view nlaPutU32Routine = "nla_put_u32";
static constexpr std::string_view nlaPutU64Routine = "nla_put_u64";
static constexpr std::string_view nlaPutU8Routine = "nla_put_u8";
static constexpr std::string_view nlaTypeRoutine = "nla_type";
static constexpr std::string_view nlmsgAllocRoutine = "nlmsg_alloc";
static constexpr std::string_view nlmsgAttrdataRoutine = "nlmsg_attrdata";
static constexpr std::string_view nlmsgAttrlenRoutine = "nlmsg_attrlen";
static constexpr std::string_view nlmsgFreeRoutine = "nlmsg_free";
static constexpr std::string_view nlmsgHdrRoutine = "nlmsg_hdr";
static constexpr std::string_view nlaNestStartRoutine = "nla_nest_start";
static constexpr std::string_view nlaNestEndRoutine = "nla_nest_end";

template <class T>
bool NlApi::getSymbolAddr(std::string_view name, T &sym) {
    sym = reinterpret_cast<T>(genlLibraryHandle->getProcAddress(std::string(name)));
    return nullptr != sym;
}

bool NlApi::loadEntryPoints() {
    if (!isAvailable())
        return false;

    bool ok = true;
    ok = getSymbolAddr(genlConnectRoutine, genlConnectEntry);
    ok = ok && getSymbolAddr(genlCtrlResolveRoutine, genlCtrlResolveEntry);
    ok = ok && getSymbolAddr(genlHandleMsgRoutine, genlHandleMsgEntry);
    ok = ok && getSymbolAddr(genlmsgPutRoutine, genlmsgPutEntry);
    ok = ok && getSymbolAddr(genlOpsResolveRoutine, genlOpsResolveEntry);
    ok = ok && getSymbolAddr(genlRegisterFamilyRoutine, genlRegisterFamilyEntry);
    ok = ok && getSymbolAddr(genlUnregisterFamilyRoutine, genlUnregisterFamilyEntry);
    ok = ok && getSymbolAddr(nlRecvmsgsDefaultRoutine, nlRecvmsgsDefaultEntry);
    ok = ok && getSymbolAddr(nlSendAutoRoutine, nlSendAutoEntry);
    ok = ok && getSymbolAddr(nlSocketAllocRoutine, nlSocketAllocEntry);
    ok = ok && getSymbolAddr(nlSocketDisableSeqCheckRoutine, nlSocketDisableSeqCheckEntry);
    ok = ok && getSymbolAddr(nlSocketFreeRoutine, nlSocketFreeEntry);
    ok = ok && getSymbolAddr(nlSocketModifyCbRoutine, nlSocketModifyCbEntry);
    ok = ok && getSymbolAddr(nlaDataRoutine, nlaDataEntry);
    ok = ok && getSymbolAddr(nlaGetU32Routine, nlaGetU32Entry);
    ok = ok && getSymbolAddr(nlaGetU64Routine, nlaGetU64Entry);
    ok = ok && getSymbolAddr(nlaGetU8Routine, nlaGetU8Entry);
    ok = ok && getSymbolAddr(nlaIsNestedRoutine, nlaIsNestedEntry);
    ok = ok && getSymbolAddr(nlaLenRoutine, nlaLenEntry);
    ok = ok && getSymbolAddr(nlaNextRoutine, nlaNextEntry);
    ok = ok && getSymbolAddr(nlaOkRoutine, nlaOkEntry);
    ok = ok && getSymbolAddr(nlaPutU16Routine, nlaPutU16Entry);
    ok = ok && getSymbolAddr(nlaPutU32Routine, nlaPutU32Entry);
    ok = ok && getSymbolAddr(nlaPutU64Routine, nlaPutU64Entry);
    ok = ok && getSymbolAddr(nlaPutU8Routine, nlaPutU8Entry);
    ok = ok && getSymbolAddr(nlaTypeRoutine, nlaTypeEntry);
    ok = ok && getSymbolAddr(nlmsgAllocRoutine, nlmsgAllocEntry);
    ok = ok && getSymbolAddr(nlmsgAttrdataRoutine, nlmsgAttrdataEntry);
    ok = ok && getSymbolAddr(nlmsgAttrlenRoutine, nlmsgAttrlenEntry);
    ok = ok && getSymbolAddr(nlmsgFreeRoutine, nlmsgFreeEntry);
    ok = ok && getSymbolAddr(nlmsgHdrRoutine, nlmsgHdrEntry);
    ok = ok && getSymbolAddr(nlaNestStartRoutine, nlaNestStartEntry);
    ok = ok && getSymbolAddr(nlaNestEndRoutine, nlaNestEndEntry);

    return ok;
}

int NlApi::genlConnect(struct nl_sock *sock) {
    UNRECOVERABLE_IF(nullptr == genlConnectEntry);
    return (*genlConnectEntry)(sock);
}

int NlApi::genlCtrlResolve(struct nl_sock *sock, const char *name) {
    UNRECOVERABLE_IF(nullptr == genlCtrlResolveEntry);
    return (*genlCtrlResolveEntry)(sock, name);
}

int NlApi::genlHandleMsg(struct nl_msg *msg, void *arg) {
    UNRECOVERABLE_IF(nullptr == genlHandleMsgEntry);
    return (*genlHandleMsgEntry)(msg, arg);
}

void *NlApi::genlmsgPut(struct nl_msg *msg, uint32_t port, uint32_t seq, int family, int hdrlen, int flags, uint8_t cmd, uint8_t version) {
    UNRECOVERABLE_IF(nullptr == genlmsgPutEntry);
    return (*genlmsgPutEntry)(msg, port, seq, family, hdrlen, flags, cmd, version);
}

int NlApi::genlOpsResolve(struct nl_sock *sock, struct genl_ops *ops) {
    UNRECOVERABLE_IF(nullptr == genlOpsResolveEntry);
    return (*genlOpsResolveEntry)(sock, ops);
}

int NlApi::genlRegisterFamily(struct genl_ops *ops) {
    UNRECOVERABLE_IF(nullptr == genlRegisterFamilyEntry);
    return (*genlRegisterFamilyEntry)(ops);
}

int NlApi::genlUnregisterFamily(struct genl_ops *ops) {
    UNRECOVERABLE_IF(nullptr == genlUnregisterFamilyEntry);
    return (*genlUnregisterFamilyEntry)(ops);
}

int NlApi::nlRecvmsgsDefault(struct nl_sock *sock) {
    UNRECOVERABLE_IF(nullptr == nlRecvmsgsDefaultEntry);
    return (*nlRecvmsgsDefaultEntry)(sock);
}

int NlApi::nlSendAuto(struct nl_sock *sock, struct nl_msg *msg) {
    UNRECOVERABLE_IF(nullptr == nlSendAutoEntry);
    return (*nlSendAutoEntry)(sock, msg);
}

struct nl_sock *NlApi::nlSocketAlloc() {
    UNRECOVERABLE_IF(nullptr == nlSocketAllocEntry);
    return (*nlSocketAllocEntry)();
}

void NlApi::nlSocketDisableSeqCheck(struct nl_sock *sock) {
    UNRECOVERABLE_IF(nullptr == nlSocketDisableSeqCheckEntry);
    (*nlSocketDisableSeqCheckEntry)(sock);
    return;
}

void NlApi::nlSocketFree(struct nl_sock *sock) {
    UNRECOVERABLE_IF(nullptr == nlSocketFreeEntry);
    (*nlSocketFreeEntry)(sock);
    return;
}

int NlApi::nlSocketModifyCb(struct nl_sock *sock, enum nl_cb_type type, enum nl_cb_kind kind, nl_recvmsg_msg_cb_t cb, void *arg) {
    UNRECOVERABLE_IF(nullptr == nlSocketModifyCbEntry);
    return (*nlSocketModifyCbEntry)(sock, type, kind, cb, arg);
}

void *NlApi::nlaData(const struct nlattr *attr) {
    UNRECOVERABLE_IF(nullptr == nlaDataEntry);
    return (*nlaDataEntry)(attr);
}

uint32_t NlApi::nlaGetU32(const struct nlattr *attr) {
    UNRECOVERABLE_IF(nullptr == nlaGetU32Entry);
    return (*nlaGetU32Entry)(attr);
}

uint64_t NlApi::nlaGetU64(const struct nlattr *attr) {
    UNRECOVERABLE_IF(nullptr == nlaGetU64Entry);
    return (*nlaGetU64Entry)(attr);
}

uint8_t NlApi::nlaGetU8(const struct nlattr *attr) {
    UNRECOVERABLE_IF(nullptr == nlaGetU8Entry);
    return (*nlaGetU8Entry)(attr);
}

int NlApi::nlaIsNested(const struct nlattr *attr) {
    UNRECOVERABLE_IF(nullptr == nlaIsNestedEntry);
    return (*nlaIsNestedEntry)(attr);
}

int NlApi::nlaLen(const struct nlattr *attr) {
    UNRECOVERABLE_IF(nullptr == nlaLenEntry);
    return (*nlaLenEntry)(attr);
}

struct nlattr *NlApi::nlaNext(const struct nlattr *attr, int *remaining) {
    UNRECOVERABLE_IF(nullptr == nlaNextEntry);
    return (*nlaNextEntry)(attr, remaining);
}

int NlApi::nlaOk(const struct nlattr *attr, int remaining) {
    UNRECOVERABLE_IF(nullptr == nlaOkEntry);
    return (*nlaOkEntry)(attr, remaining);
}

int NlApi::nlaPutU16(struct nl_msg *msg, int id, uint16_t data) {
    UNRECOVERABLE_IF(nullptr == nlaPutU16Entry);
    return (*nlaPutU16Entry)(msg, id, data);
}

int NlApi::nlaPutU32(struct nl_msg *msg, int id, uint32_t data) {
    UNRECOVERABLE_IF(nullptr == nlaPutU32Entry);
    return (*nlaPutU32Entry)(msg, id, data);
}

int NlApi::nlaPutU64(struct nl_msg *msg, int id, uint64_t data) {
    UNRECOVERABLE_IF(nullptr == nlaPutU64Entry);
    return (*nlaPutU64Entry)(msg, id, data);
}

int NlApi::nlaPutU8(struct nl_msg *msg, int id, uint8_t data) {
    UNRECOVERABLE_IF(nullptr == nlaPutU8Entry);
    return (*nlaPutU8Entry)(msg, id, data);
}

int NlApi::nlaType(const struct nlattr *attr) {
    UNRECOVERABLE_IF(nullptr == nlaTypeEntry);
    return (*nlaTypeEntry)(attr);
}

struct nl_msg *NlApi::nlmsgAlloc() {
    UNRECOVERABLE_IF(nullptr == nlmsgAllocEntry);
    return (*nlmsgAllocEntry)();
}

struct nlattr *NlApi::nlmsgAttrdata(const struct nlmsghdr *hdr, int attr) {
    UNRECOVERABLE_IF(nullptr == nlmsgAttrdataEntry);
    return (*nlmsgAttrdataEntry)(hdr, attr);
}

int NlApi::nlmsgAttrlen(const struct nlmsghdr *hdr, int attr) {
    UNRECOVERABLE_IF(nullptr == nlmsgAttrlenEntry);
    return (*nlmsgAttrlenEntry)(hdr, attr);
}

void NlApi::nlmsgFree(struct nl_msg *msg) {
    UNRECOVERABLE_IF(nullptr == nlmsgFreeEntry);
    (*nlmsgFreeEntry)(msg);
    return;
}

struct nlmsghdr *NlApi::nlmsgHdr(struct nl_msg *msg) {
    UNRECOVERABLE_IF(nullptr == nlmsgHdrEntry);
    return (*nlmsgHdrEntry)(msg);
}

struct nlattr *NlApi::nlaNestStart(struct nl_msg *msg, int id) {
    UNRECOVERABLE_IF(nullptr == nlaNestStartEntry);
    return (*nlaNestStartEntry)(msg, id);
}

int NlApi::nlaNestEnd(struct nl_msg *msg, struct nlattr *attr) {
    UNRECOVERABLE_IF(nullptr == nlaNestEndEntry);
    return (*nlaNestEndEntry)(msg, attr);
}

NlApi::NlApi() {
    genlLibraryHandle.reset(NEO::OsLibrary::loadFunc(std::string(libgenlFile)));
}

NlApi::~NlApi() = default;

} // namespace Sysman
} // namespace L0
