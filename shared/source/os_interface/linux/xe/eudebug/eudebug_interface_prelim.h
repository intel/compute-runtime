/*
 * Copyright (C) 2024-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/os_interface/linux/xe/eudebug/eudebug_interface.h"
namespace NEO {
class EuDebugInterfacePrelim : public EuDebugInterface {
  public:
    static constexpr const char *sysFsXeEuDebugFile = "/device/prelim_enable_eudebug";
    uint32_t getParamValue(EuDebugParam param) const override;
    bool isExecQueuePageFaultEnableSupported() override;
    uint32_t getAdditionalParamValue(EuDebugParam param) const;

    std::unique_ptr<EuDebugEventEuAttention> toEuDebugEventEuAttention(const void *drmType) override;
    EuDebugEventClient toEuDebugEventClient(const void *drmType) override;
    EuDebugEventVm toEuDebugEventVm(const void *drmType) override;
    std::unique_ptr<EuDebugEventExecQueue> toEuDebugEventExecQueue(const void *drmType) override;
    std::unique_ptr<EuDebugEventExecQueuePlacements> toEuDebugEventExecQueuePlacements(const void *drmType) override;
    EuDebugEventMetadata toEuDebugEventMetadata(const void *drmType) override;
    EuDebugEventVmBind toEuDebugEventVmBind(const void *drmType) override;
    EuDebugEventVmBindOp toEuDebugEventVmBindOp(const void *drmType) override;
    EuDebugEventVmBindOpMetadata toEuDebugEventVmBindOpMetadata(const void *drmType) override;
    EuDebugEventVmBindUfence toEuDebugEventVmBindUfence(const void *drmType) override;
    std::unique_ptr<EuDebugEventPageFault> toEuDebugEventPageFault(const void *drmType) override;
    EuDebugEuControl toEuDebugEuControl(const void *drmType) override;
    EuDebugConnect toEuDebugConnect(const void *drmType) override;

    std::unique_ptr<void, void (*)(void *)> toDrmEuDebugConnect(const EuDebugConnect &connect) override;
    std::unique_ptr<void, void (*)(void *)> toDrmEuDebugEuControl(const EuDebugEuControl &euControl) override;
    std::unique_ptr<void, void (*)(void *)> toDrmEuDebugVmOpen(const EuDebugVmOpen &vmOpen) override;
    std::unique_ptr<void, void (*)(void *)> toDrmEuDebugAckEvent(const EuDebugAckEvent &ackEvent) override;
};
} // namespace NEO
