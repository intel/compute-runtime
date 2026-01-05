/*
 * Copyright (C) 2024-2026 Intel Corporation
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
    EXPECT_EQ(static_cast<uint32_t>(0), euDebugInterface.getParamValue(EuDebugParam::eventTypeMetadata));
    EXPECT_EQ(static_cast<uint32_t>(0), euDebugInterface.getParamValue(EuDebugParam::eventTypeOpen));
    EXPECT_EQ(static_cast<uint32_t>(DRM_XE_EUDEBUG_EVENT_PAGEFAULT), euDebugInterface.getParamValue(EuDebugParam::eventTypePagefault));
    EXPECT_EQ(static_cast<uint32_t>(DRM_XE_EUDEBUG_EVENT_READ), euDebugInterface.getParamValue(EuDebugParam::eventTypeRead));
    EXPECT_EQ(static_cast<uint32_t>(DRM_XE_EUDEBUG_EVENT_VM), euDebugInterface.getParamValue(EuDebugParam::eventTypeVm));
    EXPECT_EQ(static_cast<uint32_t>(DRM_XE_EUDEBUG_EVENT_VM_BIND), euDebugInterface.getParamValue(EuDebugParam::eventTypeVmBind));
    EXPECT_EQ(static_cast<uint32_t>(0), euDebugInterface.getParamValue(EuDebugParam::eventTypeVmBindOp));
    EXPECT_EQ(static_cast<uint32_t>(DRM_XE_EUDEBUG_EVENT_VM_BIND_OP_DEBUG_DATA), euDebugInterface.getParamValue(EuDebugParam::eventTypeVmBindOpDebugData));
    EXPECT_EQ(static_cast<uint32_t>(0), euDebugInterface.getParamValue(EuDebugParam::eventTypeVmBindOpMetadata));
    EXPECT_EQ(static_cast<uint32_t>(DRM_XE_EUDEBUG_EVENT_VM_BIND_UFENCE), euDebugInterface.getParamValue(EuDebugParam::eventTypeVmBindUfence));
    EXPECT_EQ(static_cast<uint32_t>(DRM_XE_EUDEBUG_EVENT_VM_BIND_FLAG_UFENCE), euDebugInterface.getParamValue(EuDebugParam::eventVmBindFlagUfence));
    EXPECT_EQ(static_cast<uint32_t>(DRM_XE_EXEC_QUEUE_SET_PROPERTY_EUDEBUG), euDebugInterface.getParamValue(EuDebugParam::execQueueSetPropertyEuDebug));
    EXPECT_EQ(static_cast<uint32_t>(DRM_XE_EXEC_QUEUE_EUDEBUG_FLAG_ENABLE), euDebugInterface.getParamValue(EuDebugParam::execQueueSetPropertyValueEnable));
    EXPECT_EQ(static_cast<uint32_t>(DRM_XE_EUDEBUG_IOCTL_ACK_EVENT), euDebugInterface.getParamValue(EuDebugParam::ioctlAckEvent));
    EXPECT_EQ(static_cast<uint32_t>(DRM_XE_EUDEBUG_IOCTL_EU_CONTROL), euDebugInterface.getParamValue(EuDebugParam::ioctlEuControl));
    EXPECT_EQ(static_cast<uint32_t>(DRM_XE_EUDEBUG_IOCTL_READ_EVENT), euDebugInterface.getParamValue(EuDebugParam::ioctlReadEvent));
    EXPECT_EQ(static_cast<uint32_t>(0), euDebugInterface.getParamValue(EuDebugParam::ioctlReadMetadata));
    EXPECT_EQ(static_cast<uint32_t>(DRM_XE_EUDEBUG_IOCTL_VM_OPEN), euDebugInterface.getParamValue(EuDebugParam::ioctlVmOpen));
    EXPECT_EQ(static_cast<uint32_t>(0), euDebugInterface.getParamValue(EuDebugParam::metadataCreate));
    EXPECT_EQ(static_cast<uint32_t>(0), euDebugInterface.getParamValue(EuDebugParam::metadataDestroy));
    EXPECT_EQ(static_cast<uint32_t>(0), euDebugInterface.getParamValue(EuDebugParam::metadataElfBinary));
    EXPECT_EQ(static_cast<uint32_t>(0), euDebugInterface.getParamValue(EuDebugParam::metadataModuleArea));
    EXPECT_EQ(static_cast<uint32_t>(0), euDebugInterface.getParamValue(EuDebugParam::metadataProgramModule));
    EXPECT_EQ(static_cast<uint32_t>(0), euDebugInterface.getParamValue(EuDebugParam::metadataSbaArea));
    EXPECT_EQ(static_cast<uint32_t>(0), euDebugInterface.getParamValue(EuDebugParam::metadataSipArea));
    EXPECT_EQ(static_cast<uint32_t>(0), euDebugInterface.getParamValue(EuDebugParam::vmBindOpExtensionsAttachDebug));
}

TEST(EuDebugInterfaceUpstreamTest, whenGettingInterfaceTypeThenUpstreamIsReturned) {
    EuDebugInterfaceUpstream euDebugInterface{};

    EXPECT_EQ(EuDebugInterfaceType::upstream, euDebugInterface.getInterfaceType());
}

TEST(EuDebugInterfaceUpstreamTest, givenValidDrmEuAttentionWhenConvertingToInterfaceTypeThenFieldsAreCorrect) {
    EuDebugInterfaceUpstream euDebugInterface{};

    drm_xe_eudebug_event_eu_attention *drmEuAttention = (drm_xe_eudebug_event_eu_attention *)malloc(sizeof(drm_xe_eudebug_event_eu_attention) + 4 * sizeof(uint8_t));
    drmEuAttention->exec_queue_handle = 0x64;
    drmEuAttention->lrc_handle = 0x128;
    drmEuAttention->flags = 0x0F;
    drmEuAttention->bitmask_size = 4;
    drmEuAttention->bitmask[0] = 0x1;
    drmEuAttention->bitmask[1] = 0x2;
    drmEuAttention->bitmask[2] = 0x3;
    drmEuAttention->bitmask[3] = 0x4;

    auto event = euDebugInterface.toEuDebugEventEuAttention(drmEuAttention);
    EXPECT_EQ(0x64u, event->execQueueHandle);
    EXPECT_EQ(0x128u, event->lrcHandle);
    EXPECT_EQ(0x0Fu, event->flags);
    EXPECT_EQ(4u, event->bitmaskSize);
    EXPECT_EQ(0x1u, event->bitmask[0]);
    EXPECT_EQ(0x2u, event->bitmask[1]);
    EXPECT_EQ(0x3u, event->bitmask[2]);
    EXPECT_EQ(0x4u, event->bitmask[3]);

    free(drmEuAttention);
}

TEST(EuDebugInterfaceUpstreamTest, givenValidDrmVmWhenConvertingToInterfaceTypeThenFieldsAreCorrect) {
    EuDebugInterfaceUpstream euDebugInterface{};

    drm_xe_eudebug_event_vm drmVm = {};
    drmVm.vm_handle = 0x64;

    auto event = euDebugInterface.toEuDebugEventVm(&drmVm);
    EXPECT_EQ(0x64u, event.vmHandle);
}

TEST(EuDebugInterfaceUpstreamTest, givenValidDrmExecQueueWhenConvertingToInterfaceTypeThenFieldsAreCorrect) {
    EuDebugInterfaceUpstream euDebugInterface{};

    drm_xe_eudebug_event_exec_queue *drmExecQueue = (drm_xe_eudebug_event_exec_queue *)malloc(sizeof(drm_xe_eudebug_event_exec_queue) + 3 * sizeof(uint64_t));
    drmExecQueue->vm_handle = 0x64;
    drmExecQueue->exec_queue_handle = 0x128;
    drmExecQueue->engine_class = 0x256;
    drmExecQueue->width = 0x03;
    drmExecQueue->lrc_handle[0] = 0x1;
    drmExecQueue->lrc_handle[1] = 0x2;
    drmExecQueue->lrc_handle[2] = 0x3;

    auto event = euDebugInterface.toEuDebugEventExecQueue(drmExecQueue);
    EXPECT_EQ(0x64u, event->vmHandle);
    EXPECT_EQ(0x128u, event->execQueueHandle);
    EXPECT_EQ(0x256u, event->engineClass);
    EXPECT_EQ(0x3u, event->width);
    EXPECT_EQ(0x1u, event->lrcHandle[0]);
    EXPECT_EQ(0x2u, event->lrcHandle[1]);
    EXPECT_EQ(0x3u, event->lrcHandle[2]);

    free(drmExecQueue);
}

TEST(EuDebugInterfaceUpstreamTest, givenValidDrmVmBindWhenConvertingToInterfaceTypeThenFieldsAreCorrect) {
    EuDebugInterfaceUpstream euDebugInterface{};

    drm_xe_eudebug_event_vm_bind drmVmBind = {};
    drmVmBind.vm_handle = 0x64;
    drmVmBind.flags = 0x0F;
    drmVmBind.num_binds = 0x128;

    auto event = euDebugInterface.toEuDebugEventVmBind(&drmVmBind);
    EXPECT_EQ(0x64u, event.vmHandle);
    EXPECT_EQ(0x0Fu, event.flags);
    EXPECT_EQ(0x128u, event.numBinds);
}

TEST(EuDebugInterfaceUpstreamTest, givenValidDrmVmBindUfenceWhenConvertingToInterfaceTypeThenFieldsAreCorrect) {
    EuDebugInterfaceUpstream euDebugInterface{};

    drm_xe_eudebug_event_vm_bind_ufence drmVmBindUfence = {};
    drmVmBindUfence.vm_bind_ref_seqno = 0x32;

    auto event = euDebugInterface.toEuDebugEventVmBindUfence(&drmVmBindUfence);
    EXPECT_EQ(0x32u, event.vmBindRefSeqno);
}

TEST(EuDebugInterfaceUpstreamTest, givenValidDrmPageFaultWhenConvertingToInterfaceTypeThenFieldsAreCorrect) {
    EuDebugInterfaceUpstream euDebugInterface{};

    drm_xe_eudebug_event_pagefault *drmPageFault = (drm_xe_eudebug_event_pagefault *)malloc(sizeof(drm_xe_eudebug_event_pagefault) + 4 * sizeof(uint8_t));
    drmPageFault->exec_queue_handle = 0x64;
    drmPageFault->flags = 0x0F;
    drmPageFault->lrc_handle = 0x4096;
    drmPageFault->pagefault_address = 0x128;
    drmPageFault->bitmask_size = 4;
    drmPageFault->bitmask[0] = 0x1;
    drmPageFault->bitmask[1] = 0x2;
    drmPageFault->bitmask[2] = 0x3;
    drmPageFault->bitmask[3] = 0x4;

    auto event = euDebugInterface.toEuDebugEventPageFault(drmPageFault);
    EXPECT_EQ(0x64u, event->execQueueHandle);
    EXPECT_EQ(0x0Fu, event->flags);
    EXPECT_EQ(0x4096u, event->lrcHandle);
    EXPECT_EQ(0x128u, event->pagefaultAddress);
    EXPECT_EQ(4u, event->bitmaskSize);
    EXPECT_EQ(0x1u, event->bitmask[0]);
    EXPECT_EQ(0x2u, event->bitmask[1]);
    EXPECT_EQ(0x3u, event->bitmask[2]);
    EXPECT_EQ(0x4u, event->bitmask[3]);

    free(drmPageFault);
}

TEST(EuDebugInterfaceUpstreamTest, givenValidDrmEuControlWhenConvertingToInterfaceTypeThenFieldsAreCorrect) {
    EuDebugInterfaceUpstream euDebugInterface{};

    drm_xe_eudebug_eu_control drmEuControl = {};
    drmEuControl.cmd = 1;
    drmEuControl.flags = 2;
    drmEuControl.seqno = 3;
    drmEuControl.exec_queue_handle = 4;
    drmEuControl.lrc_handle = 5;
    drmEuControl.bitmask_size = 6;

    auto bitmask = std::make_unique<uint8_t[]>(drmEuControl.bitmask_size);
    for (uint32_t i = 0; i < drmEuControl.bitmask_size; i++) {
        bitmask[i] = static_cast<uint8_t>(i + 1);
    }
    drmEuControl.bitmask_ptr = reinterpret_cast<uintptr_t>(bitmask.get());

    auto event = euDebugInterface.toEuDebugEuControl(&drmEuControl);
    EXPECT_EQ(1u, event.cmd);
    EXPECT_EQ(2u, event.flags);
    EXPECT_EQ(3u, event.seqno);
    EXPECT_EQ(4u, event.execQueueHandle);
    EXPECT_EQ(5u, event.lrcHandle);
    EXPECT_EQ(6u, event.bitmaskSize);
    for (uint32_t i = 0; i < drmEuControl.bitmask_size; i++) {
        EXPECT_EQ(static_cast<uint8_t>(i + 1), reinterpret_cast<uint8_t *>(event.bitmaskPtr)[i]);
    }
}

TEST(EuDebugInterfaceUpstreamTest, givenValidDrmConnectwhenConvertingToInterfaceTypesThenFieldsAreCorrect) {
    EuDebugInterfaceUpstream euDebugInterface{};

    drm_xe_eudebug_connect drmConnect = {};
    drmConnect.extensions = 1;
    drmConnect.flags = 3;
    drmConnect.version = 4;

    auto connect = euDebugInterface.toEuDebugConnect(&drmConnect);

    EXPECT_EQ(1u, connect.extensions);
    EXPECT_EQ(3u, connect.flags);
    EXPECT_EQ(4u, connect.version);
}

TEST(EuDebugInterfaceUpstreamTest, givenValidDrmVmBindOpDebugDataWhenConvertingToInterfaceVmBindOpDebugDataThenFieldsAreCorrect) {
    EuDebugInterfaceUpstream euDebugInterface{};

    drm_xe_eudebug_event_vm_bind_op_debug_data drmVmBindOpDebugData = {};
    drmVmBindOpDebugData.vm_bind_ref_seqno = 0x32;
    drmVmBindOpDebugData.num_extensions = 0x64;
    drmVmBindOpDebugData.addr = 0x128;
    drmVmBindOpDebugData.range = 0x256;
    drmVmBindOpDebugData.flags = 0x512;
    drmVmBindOpDebugData.offset = 0x1024;
    drmVmBindOpDebugData.reserved = 0x2048;
    drmVmBindOpDebugData.pseudopath = 0x4096;

    auto event = euDebugInterface.toEuDebugEventVmBindOpDebugData(&drmVmBindOpDebugData);
    EXPECT_EQ(0x32u, event.vmBindRefSeqno);
    EXPECT_EQ(0x64u, event.numExtensions);
    EXPECT_EQ(0x128u, event.addr);
    EXPECT_EQ(0x256u, event.range);
    EXPECT_EQ(0x512u, event.flags);
    EXPECT_EQ(0x1024u, event.offset);
    EXPECT_EQ(0x2048u, event.reserved);
    EXPECT_EQ(0x4096u, event.pseudoPath);
}

TEST(EuDebugInterfaceUpstreamTest, givenInterfaceConnectWhenConvertingToDrmConnectThenDrmTypeIsCorrect) {
    EuDebugInterfaceUpstream euDebugInterface{};

    EuDebugConnect connect = {};
    connect.extensions = 1;

    auto wrappedDrmConnect = euDebugInterface.toDrmEuDebugConnect(connect);
    auto drmConnect = static_cast<drm_xe_eudebug_connect *>(wrappedDrmConnect.get());

    EXPECT_EQ(1u, drmConnect->extensions);
}

TEST(EuDebugInterfaceUpstreamTest, givenInterfaceControlWhenConvertingToDrmEuControlThenDrmTypeIsCorrect) {
    EuDebugInterfaceUpstream euDebugInterface{};

    EuDebugEuControl euControl = {};
    euControl.cmd = 2;
    euControl.flags = 3;
    euControl.seqno = 4;
    euControl.execQueueHandle = 5;
    euControl.lrcHandle = 6;
    euControl.bitmaskSize = 7;
    auto bitmask = std::make_unique<uint8_t[]>(euControl.bitmaskSize);

    for (uint32_t i = 0; i < euControl.bitmaskSize; i++) {
        bitmask[i] = static_cast<uint8_t>(i + 1);
    }
    euControl.bitmaskPtr = reinterpret_cast<uintptr_t>(bitmask.get());

    auto wrappedDrmEuControl = euDebugInterface.toDrmEuDebugEuControl(euControl);
    auto drmEuControl = static_cast<drm_xe_eudebug_eu_control *>(wrappedDrmEuControl.get());

    EXPECT_EQ(2u, drmEuControl->cmd);
    EXPECT_EQ(3u, drmEuControl->flags);
    EXPECT_EQ(4u, drmEuControl->seqno);
    EXPECT_EQ(5u, drmEuControl->exec_queue_handle);
    EXPECT_EQ(6u, drmEuControl->lrc_handle);
    EXPECT_EQ(7u, drmEuControl->bitmask_size);
    for (uint32_t i = 0; i < euControl.bitmaskSize; i++) {
        EXPECT_EQ(static_cast<uint8_t>(i + 1), reinterpret_cast<uint8_t *>(drmEuControl->bitmask_ptr)[i]);
    }
}

TEST(EuDebugInterfaceUpstreamTest, givenInterfaceVmOpenWhenConvertingToDrmVmOpenThenDrmTypeIsCorrect) {
    EuDebugInterfaceUpstream euDebugInterface{};

    EuDebugVmOpen vmOpen = {};
    vmOpen.extensions = 1;
    vmOpen.vmHandle = 3;
    vmOpen.flags = 4;
    vmOpen.timeoutNs = 5;

    auto wrappedDrmVmOpen = euDebugInterface.toDrmEuDebugVmOpen(vmOpen);
    auto drmVmOpen = static_cast<drm_xe_eudebug_vm_open *>(wrappedDrmVmOpen.get());

    EXPECT_EQ(1u, drmVmOpen->extensions);
    EXPECT_EQ(3u, drmVmOpen->vm_handle);
    EXPECT_EQ(4u, drmVmOpen->flags);
    EXPECT_EQ(5u, drmVmOpen->timeout_ns);
}

TEST(EuDebugInterfaceUpstreamTest, givenInterfaceAckEventWhenConvertingToDrmAckEventThenDrmTypeIsCorrect) {
    EuDebugInterfaceUpstream euDebugInterface{};

    EuDebugAckEvent ackEvent = {};
    ackEvent.type = 1;
    ackEvent.flags = 2;
    ackEvent.seqno = 3;

    auto wrappedDrmAckEvent = euDebugInterface.toDrmEuDebugAckEvent(ackEvent);
    auto drmAckEvent = static_cast<drm_xe_eudebug_ack_event *>(wrappedDrmAckEvent.get());

    EXPECT_EQ(1u, drmAckEvent->type);
    EXPECT_EQ(2u, drmAckEvent->flags);
    EXPECT_EQ(3u, drmAckEvent->seqno);
}