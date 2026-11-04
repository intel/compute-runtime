/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/sysman/source/shared/linux/fwctl/sysman_fwctl.h"

#include <cerrno>
#include <cstring>
#include <dirent.h>
#include <string>
#include <utility>
#include <vector>

namespace L0 {
namespace Sysman {
namespace ult {

inline const std::string mockFwctlNonSysctrlNodeName = "fwctl0";
inline const std::string mockFwctlSysctrlNodeName = "fwctl1";
inline const std::string mockFwctlNonSysctrlDevicePath = "/dev/fwctl/fwctl0";
inline const std::string mockFwctlSysctrlDevicePath = "/dev/fwctl/fwctl1";
constexpr int mockFwctlNonSysctrlFd = 7;
constexpr int mockFwctlSysctrlFd = 8;

constexpr uint32_t mockFwctlSysctrlDeviceType = FWCTL_DEVICE_TYPE_XE_SYSCTRL;
constexpr uint32_t mockFwctlNonSysctrlDeviceType = FWCTL_DEVICE_TYPE_PDS;

constexpr unsigned long fwctlInfoCmd = 0x9a00;
constexpr unsigned long fwctlRpcCmd = 0x9a01;
static_assert(fwctlInfoCmd == static_cast<unsigned long>(FWCTL_INFO), "FWCTL_INFO ioctl number changed");
static_assert(fwctlRpcCmd == static_cast<unsigned long>(FWCTL_RPC), "FWCTL_RPC ioctl number changed");

inline DIR *const mockFwctlDirHandle = reinterpret_cast<DIR *>(0xc001);

struct MockFwctlSysCalls {
    std::string openedDirectoryPath;
    std::vector<std::string> nodeNames;
    size_t readdirIndex = 0;
    bool opendirFails = false;
    int opendirErrno = ENOENT;

    std::vector<std::string> openedPaths;
    uint32_t openCount = 0;
    uint32_t closeCount = 0;

    uint32_t infoCount = 0;
    uint32_t capturedInfoSize = 0;
    uint32_t capturedInfoFlags = 0xffffffffu;
    uint32_t capturedInfoDeviceDataLen = 0;
    uint32_t uctxCaps = FWCTL_XE_SYSCTRL_CAP_ECC;

    uint32_t rpcCount = 0;
    uint32_t capturedRpcSize = 0;
    uint32_t capturedRpcScope = 0xffffffffu;
    uint32_t capturedRpcInLen = 0;
    uint32_t capturedRpcOutLen = 0;
    uint32_t capturedRpcOp = 0;
    uint32_t capturedRpcFlags = 0xffffffffu;
    bool capturedRpcInAliasesOut = false;

    uint32_t responseData0 = 0;
    uint32_t responseData1 = 0;
};

inline MockFwctlSysCalls mockFwctlSysCalls = {};

inline void clearMockFwctlSysCalls() {
    MockFwctlSysCalls released;
    std::swap(mockFwctlSysCalls, released);
}

inline void resetMockFwctlSysCalls() {
    clearMockFwctlSysCalls();
    mockFwctlSysCalls.nodeNames = {mockFwctlNonSysctrlNodeName, mockFwctlSysctrlNodeName};
}

inline DIR *mockFwctlOpendir(const char *name) {
    mockFwctlSysCalls.openedDirectoryPath = std::string(name);
    mockFwctlSysCalls.readdirIndex = 0;
    if (mockFwctlSysCalls.opendirFails) {
        errno = mockFwctlSysCalls.opendirErrno;
        return nullptr;
    }
    return mockFwctlDirHandle;
}

inline struct dirent *mockFwctlReaddir(DIR *dir) {
    if (mockFwctlSysCalls.readdirIndex >= mockFwctlSysCalls.nodeNames.size()) {
        return nullptr;
    }
    static struct dirent entry;
    entry = {};
    const std::string &name = mockFwctlSysCalls.nodeNames[mockFwctlSysCalls.readdirIndex++];
    std::strncpy(entry.d_name, name.c_str(), sizeof(entry.d_name) - 1);
    return &entry;
}

inline int mockFwctlClosedir(DIR *dir) {
    return 0;
}

inline int mockFwctlOpenSuccess(const char *pathname, int flags) {
    std::string path(pathname);
    mockFwctlSysCalls.openedPaths.push_back(path);
    mockFwctlSysCalls.openCount++;
    if (path == mockFwctlSysctrlDevicePath) {
        return mockFwctlSysctrlFd;
    }
    if (path == mockFwctlNonSysctrlDevicePath) {
        return mockFwctlNonSysctrlFd;
    }
    errno = ENOENT;
    return -1;
}

inline int mockFwctlCloseSuccess(int fd) {
    mockFwctlSysCalls.closeCount++;
    return 0;
}

inline bool isMockFwctlFd(int fd) {
    return (fd == mockFwctlSysctrlFd) || (fd == mockFwctlNonSysctrlFd);
}

inline int mockFwctlIoctlSuccess(int fd, unsigned long request, void *arg) {
    if (!isMockFwctlFd(fd)) {
        return -1;
    }

    if (request == fwctlInfoCmd) {
        mockFwctlSysCalls.infoCount++;
        auto pInfo = static_cast<fwctl_info *>(arg);
        mockFwctlSysCalls.capturedInfoSize = pInfo->size;
        mockFwctlSysCalls.capturedInfoFlags = pInfo->flags;
        mockFwctlSysCalls.capturedInfoDeviceDataLen = pInfo->device_data_len;

        pInfo->out_device_type = (fd == mockFwctlSysctrlFd) ? mockFwctlSysctrlDeviceType : mockFwctlNonSysctrlDeviceType;
        if ((pInfo->device_data_len >= sizeof(fwctl_info_xe_sysctrl)) && (pInfo->out_device_data != 0)) {
            auto pDeviceData = reinterpret_cast<fwctl_info_xe_sysctrl *>(static_cast<uintptr_t>(pInfo->out_device_data));
            *pDeviceData = {};
            pDeviceData->uctx_caps = mockFwctlSysCalls.uctxCaps;
        }
        pInfo->device_data_len = sizeof(fwctl_info_xe_sysctrl);
        return 0;
    }

    if (request == fwctlRpcCmd) {
        mockFwctlSysCalls.rpcCount++;
        auto pRpcCommand = static_cast<fwctl_rpc *>(arg);
        mockFwctlSysCalls.capturedRpcSize = pRpcCommand->size;
        mockFwctlSysCalls.capturedRpcScope = pRpcCommand->scope;
        mockFwctlSysCalls.capturedRpcInLen = pRpcCommand->in_len;
        mockFwctlSysCalls.capturedRpcOutLen = pRpcCommand->out_len;
        mockFwctlSysCalls.capturedRpcInAliasesOut = (pRpcCommand->in == pRpcCommand->out);

        if ((fd != mockFwctlSysctrlFd) || (pRpcCommand->in == 0)) {
            errno = EBADMSG;
            return -1;
        }
        auto pRpc = reinterpret_cast<fwctl_rpc_xe_sysctrl *>(static_cast<uintptr_t>(pRpcCommand->in));
        mockFwctlSysCalls.capturedRpcOp = pRpc->op;
        mockFwctlSysCalls.capturedRpcFlags = pRpc->flags;
        pRpc->data[0] = mockFwctlSysCalls.responseData0;
        pRpc->data[1] = mockFwctlSysCalls.responseData1;
        return 0;
    }

    return -1;
}

} // namespace ult
} // namespace Sysman
} // namespace L0
