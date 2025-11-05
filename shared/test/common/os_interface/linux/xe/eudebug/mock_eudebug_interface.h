/*
 * Copyright (C) 2024-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/os_interface/linux/xe/eudebug/eudebug_interface.h"

namespace NEO {
class MockEuDebugInterface : public EuDebugInterface {
  public:
    static char sysFsContent;
    static constexpr const char *sysFsXeEuDebugFile = "/mock_eudebug";
    static constexpr uintptr_t sysFsFd = 0xE0DEB0;
    static constexpr EuDebugInterfaceType euDebugInterfaceType = EuDebugInterfaceType::upstream;
    bool isExecQueuePageFaultEnableSupported() override { return pageFaultEnableSupported; };
    uint32_t getParamValue(EuDebugParam param) const override;
    EuDebugInterfaceType getInterfaceType() const override { return euDebugInterfaceType; };
    uint64_t getDefaultClientHandle() const override {
        return 1; // EuDebugInterfaceUpstream::defaultClientHandle
    };

    std::unique_ptr<EuDebugEventEuAttention, void (*)(EuDebugEventEuAttention *)> toEuDebugEventEuAttention(const void *drmType) override;
    EuDebugEventClient toEuDebugEventClient(const void *drmType) override;
    EuDebugEventVm toEuDebugEventVm(const void *drmType) override;
    std::unique_ptr<EuDebugEventExecQueue, void (*)(EuDebugEventExecQueue *)> toEuDebugEventExecQueue(const void *drmType) override;
    std::unique_ptr<EuDebugEventExecQueuePlacements, void (*)(EuDebugEventExecQueuePlacements *)> toEuDebugEventExecQueuePlacements(const void *drmType) override;
    EuDebugEventMetadata toEuDebugEventMetadata(const void *drmType) override;
    EuDebugEventVmBind toEuDebugEventVmBind(const void *drmType) override;
    EuDebugEventVmBindOp toEuDebugEventVmBindOp(const void *drmType) override;
    EuDebugEventVmBindOpMetadata toEuDebugEventVmBindOpMetadata(const void *drmType) override;
    EuDebugEventVmBindUfence toEuDebugEventVmBindUfence(const void *drmType) override;
    std::unique_ptr<EuDebugEventPageFault, void (*)(EuDebugEventPageFault *)> toEuDebugEventPageFault(const void *drmType) override;
    EuDebugEuControl toEuDebugEuControl(const void *drmType) override;
    EuDebugConnect toEuDebugConnect(const void *drmType) override;

    std::unique_ptr<void, void (*)(void *)> toDrmEuDebugConnect(const EuDebugConnect &connect) override;
    std::unique_ptr<void, void (*)(void *)> toDrmEuDebugEuControl(const EuDebugEuControl &euControl) override;
    std::unique_ptr<void, void (*)(void *)> toDrmEuDebugVmOpen(const EuDebugVmOpen &vmOpen) override;
    std::unique_ptr<void, void (*)(void *)> toDrmEuDebugAckEvent(const EuDebugAckEvent &ackEvent) override;

    bool pageFaultEnableSupported = false;
};

} // namespace NEO
