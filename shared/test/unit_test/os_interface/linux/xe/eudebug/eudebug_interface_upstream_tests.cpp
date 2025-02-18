/*
 * Copyright (C) 2024-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/xe/eudebug/eudebug_interface_upstream.h"
#include "shared/test/common/test_macros/test.h"

#include "third_party/uapi-eudebug/drm/xe_drm.h"

using namespace NEO;
TEST(EuDebugInterfaceUpstreamTest, whenGettingParamValueThenCorrectValueIsReturned) {
    EuDebugInterfaceUpstream euDebugInterface{};

    EXPECT_EQ(static_cast<uint32_t>(DRM_IOCTL_XE_EUDEBUG_CONNECT), euDebugInterface.getParamValue(EuDebugParam::connect));
    EXPECT_EQ(static_cast<uint32_t>(DRM_XE_EUDEBUG_EU_CONTROL_CMD_INTERRUPT_ALL), euDebugInterface.getParamValue(EuDebugParam::euControlCmdInterruptAll));
    EXPECT_EQ(static_cast<uint32_t>(DRM_XE_EUDEBUG_EU_CONTROL_CMD_RESUME), euDebugInterface.getParamValue(EuDebugParam::euControlCmdResume));
    EXPECT_EQ(static_cast<uint32_t>(DRM_XE_EUDEBUG_EU_CONTROL_CMD_STOPPED), euDebugInterface.getParamValue(EuDebugParam::euControlCmdStopped));
    EXPECT_EQ(static_cast<uint32_t>(DRM_XE_EUDEBUG_EVENT_CREATE), euDebugInterface.getParamValue(EuDebugParam::eventBitCreate));
    EXPECT_EQ(static_cast<uint32_t>(DRM_XE_EUDEBUG_EVENT_DESTROY), euDebugInterface.getParamValue(EuDebugParam::eventBitDestroy));
    EXPECT_EQ(static_cast<uint32_t>(DRM_XE_EUDEBUG_EVENT_NEED_ACK), euDebugInterface.getParamValue(EuDebugParam::eventBitNeedAck));
    EXPECT_EQ(static_cast<uint32_t>(DRM_XE_EUDEBUG_EVENT_STATE_CHANGE), euDebugInterface.getParamValue(EuDebugParam::eventBitStateChange));
    EXPECT_EQ(static_cast<uint32_t>(DRM_XE_EUDEBUG_EVENT_EU_ATTENTION), euDebugInterface.getParamValue(EuDebugParam::eventTypeEuAttention));
    EXPECT_EQ(static_cast<uint32_t>(DRM_XE_EUDEBUG_EVENT_EXEC_QUEUE), euDebugInterface.getParamValue(EuDebugParam::eventTypeExecQueue));
    EXPECT_EQ(static_cast<uint32_t>(0), euDebugInterface.getParamValue(EuDebugParam::eventTypeExecQueuePlacements));
    EXPECT_EQ(static_cast<uint32_t>(DRM_XE_EUDEBUG_EVENT_METADATA), euDebugInterface.getParamValue(EuDebugParam::eventTypeMetadata));
    EXPECT_EQ(static_cast<uint32_t>(DRM_XE_EUDEBUG_EVENT_OPEN), euDebugInterface.getParamValue(EuDebugParam::eventTypeOpen));
    EXPECT_EQ(static_cast<uint32_t>(DRM_XE_EUDEBUG_EVENT_PAGEFAULT), euDebugInterface.getParamValue(EuDebugParam::eventTypePagefault));
    EXPECT_EQ(static_cast<uint32_t>(DRM_XE_EUDEBUG_EVENT_READ), euDebugInterface.getParamValue(EuDebugParam::eventTypeRead));
    EXPECT_EQ(static_cast<uint32_t>(DRM_XE_EUDEBUG_EVENT_VM), euDebugInterface.getParamValue(EuDebugParam::eventTypeVm));
    EXPECT_EQ(static_cast<uint32_t>(DRM_XE_EUDEBUG_EVENT_VM_BIND), euDebugInterface.getParamValue(EuDebugParam::eventTypeVmBind));
    EXPECT_EQ(static_cast<uint32_t>(DRM_XE_EUDEBUG_EVENT_VM_BIND_OP), euDebugInterface.getParamValue(EuDebugParam::eventTypeVmBindOp));
    EXPECT_EQ(static_cast<uint32_t>(DRM_XE_EUDEBUG_EVENT_VM_BIND_OP_METADATA), euDebugInterface.getParamValue(EuDebugParam::eventTypeVmBindOpMetadata));
    EXPECT_EQ(static_cast<uint32_t>(DRM_XE_EUDEBUG_EVENT_VM_BIND_UFENCE), euDebugInterface.getParamValue(EuDebugParam::eventTypeVmBindUfence));
    EXPECT_EQ(static_cast<uint32_t>(DRM_XE_EUDEBUG_EVENT_VM_BIND_FLAG_UFENCE), euDebugInterface.getParamValue(EuDebugParam::eventVmBindFlagUfence));
    EXPECT_EQ(static_cast<uint32_t>(DRM_XE_EXEC_QUEUE_SET_PROPERTY_EUDEBUG), euDebugInterface.getParamValue(EuDebugParam::execQueueSetPropertyEuDebug));
    EXPECT_EQ(static_cast<uint32_t>(DRM_XE_EXEC_QUEUE_EUDEBUG_FLAG_ENABLE), euDebugInterface.getParamValue(EuDebugParam::execQueueSetPropertyValueEnable));
    EXPECT_EQ(static_cast<uint32_t>(DRM_XE_EUDEBUG_IOCTL_ACK_EVENT), euDebugInterface.getParamValue(EuDebugParam::ioctlAckEvent));
    EXPECT_EQ(static_cast<uint32_t>(DRM_XE_EUDEBUG_IOCTL_EU_CONTROL), euDebugInterface.getParamValue(EuDebugParam::ioctlEuControl));
    EXPECT_EQ(static_cast<uint32_t>(DRM_XE_EUDEBUG_IOCTL_READ_EVENT), euDebugInterface.getParamValue(EuDebugParam::ioctlReadEvent));
    EXPECT_EQ(static_cast<uint32_t>(DRM_XE_EUDEBUG_IOCTL_READ_METADATA), euDebugInterface.getParamValue(EuDebugParam::ioctlReadMetadata));
    EXPECT_EQ(static_cast<uint32_t>(DRM_XE_EUDEBUG_IOCTL_VM_OPEN), euDebugInterface.getParamValue(EuDebugParam::ioctlVmOpen));
    EXPECT_EQ(static_cast<uint32_t>(DRM_IOCTL_XE_DEBUG_METADATA_CREATE), euDebugInterface.getParamValue(EuDebugParam::metadataCreate));
    EXPECT_EQ(static_cast<uint32_t>(DRM_IOCTL_XE_DEBUG_METADATA_DESTROY), euDebugInterface.getParamValue(EuDebugParam::metadataDestroy));
    EXPECT_EQ(static_cast<uint32_t>(DRM_XE_DEBUG_METADATA_ELF_BINARY), euDebugInterface.getParamValue(EuDebugParam::metadataElfBinary));
    EXPECT_EQ(static_cast<uint32_t>(WORK_IN_PROGRESS_DRM_XE_DEBUG_METADATA_MODULE_AREA), euDebugInterface.getParamValue(EuDebugParam::metadataModuleArea));
    EXPECT_EQ(static_cast<uint32_t>(DRM_XE_DEBUG_METADATA_PROGRAM_MODULE), euDebugInterface.getParamValue(EuDebugParam::metadataProgramModule));
    EXPECT_EQ(static_cast<uint32_t>(WORK_IN_PROGRESS_DRM_XE_DEBUG_METADATA_SBA_AREA), euDebugInterface.getParamValue(EuDebugParam::metadataSbaArea));
    EXPECT_EQ(static_cast<uint32_t>(WORK_IN_PROGRESS_DRM_XE_DEBUG_METADATA_SIP_AREA), euDebugInterface.getParamValue(EuDebugParam::metadataSipArea));
    EXPECT_EQ(static_cast<uint32_t>(XE_VM_BIND_OP_EXTENSIONS_ATTACH_DEBUG), euDebugInterface.getParamValue(EuDebugParam::vmBindOpExtensionsAttachDebug));
}
