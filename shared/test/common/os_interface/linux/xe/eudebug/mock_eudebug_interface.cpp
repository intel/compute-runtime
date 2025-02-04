/*
 * Copyright (C) 2024-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/os_interface/linux/xe/eudebug/mock_eudebug_interface.h"

#define RETURN_AS_IS(X) \
    case X:             \
        return static_cast<uint32_t>(X)
#define RETURN_AS_BIT_UINT16(X)                        \
    case X:                                            \
        static_assert(static_cast<uint32_t>(X) < 16u); \
        return static_cast<uint32_t>(1 << static_cast<uint32_t>(X))
namespace NEO {
uint32_t MockEuDebugInterface::getParamValue(EuDebugParam param) const {
    switch (param) {
        RETURN_AS_BIT_UINT16(EuDebugParam::eventBitCreate);
        RETURN_AS_BIT_UINT16(EuDebugParam::eventBitDestroy);
        RETURN_AS_BIT_UINT16(EuDebugParam::eventBitStateChange);
        RETURN_AS_BIT_UINT16(EuDebugParam::eventBitNeedAck);
        RETURN_AS_IS(EuDebugParam::connect);
        RETURN_AS_IS(EuDebugParam::euControlCmdInterruptAll);
        RETURN_AS_IS(EuDebugParam::euControlCmdResume);
        RETURN_AS_IS(EuDebugParam::euControlCmdStopped);
        RETURN_AS_IS(EuDebugParam::execQueueSetPropertyEuDebug);
        RETURN_AS_IS(EuDebugParam::execQueueSetPropertyValueEnable);
        RETURN_AS_IS(EuDebugParam::execQueueSetPropertyValuePageFaultEnable);
        RETURN_AS_IS(EuDebugParam::eventTypeEuAttention);
        RETURN_AS_IS(EuDebugParam::eventTypeExecQueue);
        RETURN_AS_IS(EuDebugParam::eventTypeExecQueuePlacements);
        RETURN_AS_IS(EuDebugParam::eventTypeMetadata);
        RETURN_AS_IS(EuDebugParam::eventTypeOpen);
        RETURN_AS_IS(EuDebugParam::eventTypePagefault);
        RETURN_AS_IS(EuDebugParam::eventTypeRead);
        RETURN_AS_IS(EuDebugParam::eventTypeVm);
        RETURN_AS_IS(EuDebugParam::eventTypeVmBind);
        RETURN_AS_IS(EuDebugParam::eventTypeVmBindOp);
        RETURN_AS_IS(EuDebugParam::eventTypeVmBindOpMetadata);
        RETURN_AS_IS(EuDebugParam::eventTypeVmBindUfence);
        RETURN_AS_IS(EuDebugParam::eventVmBindFlagUfence);
        RETURN_AS_IS(EuDebugParam::ioctlAckEvent);
        RETURN_AS_IS(EuDebugParam::ioctlEuControl);
        RETURN_AS_IS(EuDebugParam::ioctlReadEvent);
        RETURN_AS_IS(EuDebugParam::ioctlReadMetadata);
        RETURN_AS_IS(EuDebugParam::ioctlVmOpen);
        RETURN_AS_IS(EuDebugParam::metadataCreate);
        RETURN_AS_IS(EuDebugParam::metadataDestroy);
        RETURN_AS_IS(EuDebugParam::metadataElfBinary);
        RETURN_AS_IS(EuDebugParam::metadataModuleArea);
        RETURN_AS_IS(EuDebugParam::metadataProgramModule);
        RETURN_AS_IS(EuDebugParam::metadataSbaArea);
        RETURN_AS_IS(EuDebugParam::metadataSipArea);
        RETURN_AS_IS(EuDebugParam::vmBindOpExtensionsAttachDebug);
    }
    return 0;
}

char MockEuDebugInterface::sysFsContent = '1';
[[maybe_unused]] static EnableEuDebugInterface enableMockEuDebug(MockEuDebugInterface::euDebugInterfaceType, MockEuDebugInterface::sysFsXeEuDebugFile, []() -> std::unique_ptr<EuDebugInterface> { return std::make_unique<MockEuDebugInterface>(); });
} // namespace NEO
