/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/shared/linux/fwctl/sysman_fwctl.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/preprocessor.h"

#include "level_zero/sysman/source/shared/linux/sysman_fs_access_interface.h"
#include "level_zero/sysman/source/shared/linux/sysman_sys_calls_wrapper.h"
#include "level_zero/sysman/source/shared/linux/zes_os_sysman_imp.h"

#include <cstdint>
#include <cstring>
#include <fcntl.h>
#include <string_view>

namespace L0 {
namespace Sysman {

constexpr std::string_view fwctlSysfsDirectoryName("/fwctl");
constexpr std::string_view fwctlDeviceDirectory("/dev/fwctl/");

std::unique_ptr<FwctlInterface> FwctlInterface::create(LinuxSysmanImp *pLinuxSysmanImp) {
    return std::make_unique<FwctlInterface>(pLinuxSysmanImp);
}

ze_result_t FwctlInterface::getFwctlResult(int err) {
    auto result = LinuxSysmanImp::getResult(err);
    if (ZE_RESULT_ERROR_UNKNOWN == result) {
        switch (err) {
        case ENOTTY:
        case EOPNOTSUPP:
            return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
        case E2BIG:
        case EMSGSIZE:
            return ZE_RESULT_ERROR_INVALID_SIZE;
        case EINVAL:
        case EBADMSG:
            return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        case ENOMEM:
            return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
        case ENODEV:
            return ZE_RESULT_ERROR_DEVICE_LOST;
        default:
            break;
        }
    }
    return result;
}

ze_result_t FwctlInterface::getNodeNames(std::vector<std::string> &nodeNames) {
    const std::string devicePciPath = pLinuxSysmanImp->getSysfsAccess().getDevicePciPath();
    if (devicePciPath.empty()) {
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }
    return pLinuxSysmanImp->getFsAccess().listDirectory(devicePciPath + std::string(fwctlSysfsDirectoryName), nodeNames);
}

void FwctlInterface::invalidateSysctrlNode() {
    const std::lock_guard<std::mutex> lock(nodeLock);
    sysctrlNodePath.clear();
    sysctrlUctxCaps = 0;
}

ze_result_t FwctlInterface::openSysctrlNode(int &fd, uint32_t &uctxCaps) {
    std::string nodePath;
    {
        const std::lock_guard<std::mutex> lock(nodeLock);
        nodePath = sysctrlNodePath;
        uctxCaps = sysctrlUctxCaps;
    }

    if (!nodePath.empty()) {
        int errorNum = 0;
        fd = SysmanSysCallsWrapper::open(nodePath.c_str(), O_RDWR, errorNum);
        if (fd < 0) {
            invalidateSysctrlNode();
            ze_result_t result = getFwctlResult(errorNum);
            PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Could not open cached node %s, errno %d (%s) and returning error:0x%x \n", NEO_FUNCTION_NAME, nodePath.c_str(), errorNum, strerror(errorNum), result);
            return result;
        }
        return ZE_RESULT_SUCCESS;
    }

    ze_result_t result = discoverSysctrlNode(fd, nodePath, uctxCaps);
    if (result != ZE_RESULT_SUCCESS) {
        return result;
    }

    const std::lock_guard<std::mutex> lock(nodeLock);
    sysctrlNodePath = nodePath;
    sysctrlUctxCaps = uctxCaps;
    return ZE_RESULT_SUCCESS;
}

ze_result_t FwctlInterface::discoverSysctrlNode(int &fd, std::string &nodePath, uint32_t &uctxCaps) {
    std::vector<std::string> nodeNames;
    ze_result_t result = getNodeNames(nodeNames);
    if (result != ZE_RESULT_SUCCESS) {
        if (result == ZE_RESULT_ERROR_NOT_AVAILABLE) {
            result = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
        }
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): No fwctl nodes found for the device and returning error:0x%x \n", NEO_FUNCTION_NAME, result);
        return result;
    }

    ze_result_t lastError = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    for (const auto &nodeName : nodeNames) {
        const std::string devicePath = std::string(fwctlDeviceDirectory) + nodeName;
        int errorNum = 0;
        int localFd = SysmanSysCallsWrapper::open(devicePath.c_str(), O_RDWR, errorNum);
        if (localFd < 0) {
            lastError = getFwctlResult(errorNum);
            PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Could not open %s, errno %d (%s) and returning error:0x%x \n", NEO_FUNCTION_NAME, devicePath.c_str(), errorNum, strerror(errorNum), lastError);
            continue;
        }

        fwctl_info_xe_sysctrl deviceData = {};
        fwctl_info infoCommand = {};
        infoCommand.size = sizeof(infoCommand);
        infoCommand.device_data_len = sizeof(deviceData);
        infoCommand.out_device_data = static_cast<__u64>(reinterpret_cast<uintptr_t>(&deviceData));

        if (SysmanSysCallsWrapper::ioctl(localFd, FWCTL_INFO, &infoCommand, errorNum) < 0) {
            int savedErrno = errorNum;
            SysmanSysCallsWrapper::close(localFd, errorNum);
            lastError = getFwctlResult(savedErrno);
            PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): FWCTL_INFO failed on %s, errno %d (%s) and returning error:0x%x \n", NEO_FUNCTION_NAME, devicePath.c_str(), savedErrno, strerror(savedErrno), lastError);
            continue;
        }

        if (infoCommand.out_device_type != FWCTL_DEVICE_TYPE_XE_SYSCTRL) {
            SysmanSysCallsWrapper::close(localFd, errorNum);
            continue;
        }

        fd = localFd;
        nodePath = devicePath;
        uctxCaps = deviceData.uctx_caps;
        return ZE_RESULT_SUCCESS;
    }

    PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): No System Controller fwctl node found among %zu nodes and returning error:0x%x \n", NEO_FUNCTION_NAME, nodeNames.size(), lastError);
    return lastError;
}

ze_result_t FwctlInterface::sendSysctrlRpc(fwctl_xe_sysctrl_uctx_caps requiredCap, fwctl_rpc_scope scope, fwctl_rpc_xe_sysctrl &rpc) {
    int fd = -1;
    uint32_t uctxCaps = 0;
    ze_result_t result = openSysctrlNode(fd, uctxCaps);
    if (result != ZE_RESULT_SUCCESS) {
        return result;
    }

    int errorNum = 0;

    if ((uctxCaps & requiredCap) == 0) {
        SysmanSysCallsWrapper::close(fd, errorNum);
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Capability 0x%x is not available, uctx caps 0x%x and returning error:0x%x \n", NEO_FUNCTION_NAME, requiredCap, uctxCaps, ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }

    fwctl_rpc rpcCommand = {};
    rpcCommand.size = sizeof(rpcCommand);
    rpcCommand.scope = scope;
    rpcCommand.in_len = sizeof(rpc);
    rpcCommand.out_len = sizeof(rpc);
    rpcCommand.in = static_cast<__u64>(reinterpret_cast<uintptr_t>(&rpc));
    rpcCommand.out = static_cast<__u64>(reinterpret_cast<uintptr_t>(&rpc));

    int status = SysmanSysCallsWrapper::ioctl(fd, FWCTL_RPC, &rpcCommand, errorNum);
    int savedErrno = errorNum;
    SysmanSysCallsWrapper::close(fd, errorNum);

    if (status < 0) {
        if (savedErrno == ENODEV) {
            invalidateSysctrlNode();
        }
        result = getFwctlResult(savedErrno);
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): FWCTL_RPC op %u failed, errno %d (%s) and returning error:0x%x \n", NEO_FUNCTION_NAME, rpc.op, savedErrno, strerror(savedErrno), result);
        return result;
    }

    return ZE_RESULT_SUCCESS;
}

} // namespace Sysman
} // namespace L0
