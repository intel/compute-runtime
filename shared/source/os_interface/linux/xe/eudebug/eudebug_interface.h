/*
 * Copyright (C) 2024-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/os_interface/linux/xe/eudebug/eudebug_wrappers.h"

#include <memory>
#include <string>
namespace NEO {

enum class EuDebugInterfaceType : uint32_t {
    upstream,
    prelim,
    maxValue
};
class EuDebugInterface {
  public:
    static std::unique_ptr<EuDebugInterface> create(const std::string &sysFsPciPath);
    virtual uint32_t getParamValue(EuDebugParam param) const = 0;
    virtual bool isExecQueuePageFaultEnableSupported() { return false; };
    virtual uint64_t getDefaultClientHandle() const { return 0; }
    virtual EuDebugInterfaceType getInterfaceType() const = 0;
    virtual ~EuDebugInterface() = default;

    virtual std::unique_ptr<EuDebugEventEuAttention, void (*)(EuDebugEventEuAttention *)> toEuDebugEventEuAttention(const void *drmType) = 0;
    virtual EuDebugEventClient toEuDebugEventClient(const void *drmType) = 0;
    virtual EuDebugEventVm toEuDebugEventVm(const void *drmType) = 0;
    virtual std::unique_ptr<EuDebugEventExecQueue, void (*)(EuDebugEventExecQueue *)> toEuDebugEventExecQueue(const void *drmType) = 0;
    virtual std::unique_ptr<EuDebugEventExecQueuePlacements, void (*)(EuDebugEventExecQueuePlacements *)> toEuDebugEventExecQueuePlacements(const void *drmType) = 0;
    virtual EuDebugEventMetadata toEuDebugEventMetadata(const void *drmType) = 0;
    virtual EuDebugEventVmBind toEuDebugEventVmBind(const void *drmType) = 0;
    virtual EuDebugEventVmBindOp toEuDebugEventVmBindOp(const void *drmType) = 0;
    virtual EuDebugEventVmBindOpMetadata toEuDebugEventVmBindOpMetadata(const void *drmType) = 0;
    virtual EuDebugEventVmBindUfence toEuDebugEventVmBindUfence(const void *drmType) = 0;
    virtual std::unique_ptr<EuDebugEventPageFault, void (*)(EuDebugEventPageFault *)> toEuDebugEventPageFault(const void *drmType) = 0;
    virtual EuDebugEuControl toEuDebugEuControl(const void *drmType) = 0;
    virtual EuDebugConnect toEuDebugConnect(const void *drmType) = 0;

    virtual std::unique_ptr<void, void (*)(void *)> toDrmEuDebugConnect(const EuDebugConnect &connect) = 0;
    virtual std::unique_ptr<void, void (*)(void *)> toDrmEuDebugEuControl(const EuDebugEuControl &euControl) = 0;
    virtual std::unique_ptr<void, void (*)(void *)> toDrmEuDebugVmOpen(const EuDebugVmOpen &vmOpen) = 0;
    virtual std::unique_ptr<void, void (*)(void *)> toDrmEuDebugAckEvent(const EuDebugAckEvent &ackEvent) = 0;
};

using EuDebugInterfaceCreateFunctionType = std::unique_ptr<EuDebugInterface> (*)();
extern const char *eudebugSysfsEntry[static_cast<uint32_t>(EuDebugInterfaceType::maxValue)];
extern EuDebugInterfaceCreateFunctionType eudebugInterfaceFactory[static_cast<uint32_t>(EuDebugInterfaceType::maxValue)];

class EnableEuDebugInterface {
  public:
    EnableEuDebugInterface(EuDebugInterfaceType eudebugInterfaceType, const char *sysfsEntry, EuDebugInterfaceCreateFunctionType createFunc) {
        eudebugSysfsEntry[static_cast<uint32_t>(eudebugInterfaceType)] = sysfsEntry;
        eudebugInterfaceFactory[static_cast<uint32_t>(eudebugInterfaceType)] = createFunc;
    }
};
} // namespace NEO
