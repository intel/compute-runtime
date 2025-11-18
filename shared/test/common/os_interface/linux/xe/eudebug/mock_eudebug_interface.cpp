/*
 * Copyright (C) 2024-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/os_interface/linux/xe/eudebug/mock_eudebug_interface.h"

// clang-format off
#include "third_party/uapi-eudebug/drm/xe_drm.h"
#include "third_party/uapi/drm-uapi-helper/xe/xe_drm_prelim.h"
// clang-format on

#include <string.h>

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

std::unique_ptr<EuDebugEventEuAttention, void (*)(EuDebugEventEuAttention *)> MockEuDebugInterface::toEuDebugEventEuAttention(const void *drmType) {

    const prelim_drm_xe_eudebug_event_eu_attention *event = static_cast<const prelim_drm_xe_eudebug_event_eu_attention *>(drmType);
    EuDebugEventEuAttention *pEuAttentionEvent = static_cast<EuDebugEventEuAttention *>(malloc(sizeof(EuDebugEventEuAttention) + event->bitmask_size * sizeof(uint8_t)));

    pEuAttentionEvent->base.len = event->base.len;
    pEuAttentionEvent->base.type = event->base.type;
    pEuAttentionEvent->base.flags = event->base.flags;
    pEuAttentionEvent->base.seqno = event->base.seqno;
    pEuAttentionEvent->base.reserved = event->base.reserved;

    memcpy(pEuAttentionEvent->bitmask, event->bitmask, event->bitmask_size * sizeof(uint8_t));
    pEuAttentionEvent->bitmaskSize = event->bitmask_size;
    pEuAttentionEvent->flags = event->flags;
    pEuAttentionEvent->lrcHandle = event->lrc_handle;
    pEuAttentionEvent->execQueueHandle = event->exec_queue_handle;
    pEuAttentionEvent->clientHandle = event->client_handle;

    auto deleter = [](EuDebugEventEuAttention *ptr) { free(ptr); };

    return std::unique_ptr<EuDebugEventEuAttention, void (*)(EuDebugEventEuAttention *)>(pEuAttentionEvent, deleter);
}
EuDebugEventClient MockEuDebugInterface::toEuDebugEventClient(const void *drmType) {
    return *static_cast<const EuDebugEventClient *>(drmType);
}
EuDebugEventVm MockEuDebugInterface::toEuDebugEventVm(const void *drmType) {
    return *static_cast<const EuDebugEventVm *>(drmType);
}
std::unique_ptr<EuDebugEventExecQueue, void (*)(EuDebugEventExecQueue *)> MockEuDebugInterface::toEuDebugEventExecQueue(const void *drmType) {
    const prelim_drm_xe_eudebug_event_exec_queue *event = static_cast<const prelim_drm_xe_eudebug_event_exec_queue *>(drmType);
    EuDebugEventExecQueue *pExecQueueEvent = static_cast<EuDebugEventExecQueue *>(malloc(sizeof(EuDebugEventExecQueue) + event->width * sizeof(uint64_t)));

    pExecQueueEvent->base.len = event->base.len;
    pExecQueueEvent->base.type = event->base.type;
    pExecQueueEvent->base.flags = event->base.flags;
    pExecQueueEvent->base.seqno = event->base.seqno;
    pExecQueueEvent->base.reserved = event->base.reserved;

    pExecQueueEvent->execQueueHandle = event->exec_queue_handle;
    pExecQueueEvent->engineClass = event->engine_class;
    pExecQueueEvent->width = event->width;
    pExecQueueEvent->vmHandle = event->vm_handle;
    pExecQueueEvent->clientHandle = event->client_handle;
    memcpy(pExecQueueEvent->lrcHandle, event->lrc_handle, event->width * sizeof(uint64_t));

    auto deleter = [](EuDebugEventExecQueue *ptr) { free(ptr); };

    return std::unique_ptr<EuDebugEventExecQueue, void (*)(EuDebugEventExecQueue *)>(pExecQueueEvent, deleter);
}
std::unique_ptr<EuDebugEventExecQueuePlacements, void (*)(EuDebugEventExecQueuePlacements *)> MockEuDebugInterface::toEuDebugEventExecQueuePlacements(const void *drmType) {
    const prelim_drm_xe_eudebug_event_exec_queue_placements *event = static_cast<const prelim_drm_xe_eudebug_event_exec_queue_placements *>(drmType);
    EuDebugEventExecQueuePlacements *euExecQueuePlacementsEvent = static_cast<EuDebugEventExecQueuePlacements *>(malloc(sizeof(EuDebugEventExecQueuePlacements) + event->num_placements * sizeof(uint64_t)));

    euExecQueuePlacementsEvent->base.len = event->base.len;
    euExecQueuePlacementsEvent->base.type = event->base.type;
    euExecQueuePlacementsEvent->base.flags = event->base.flags;
    euExecQueuePlacementsEvent->base.seqno = event->base.seqno;
    euExecQueuePlacementsEvent->base.reserved = event->base.reserved;

    euExecQueuePlacementsEvent->clientHandle = event->client_handle;
    euExecQueuePlacementsEvent->execQueueHandle = event->exec_queue_handle;
    euExecQueuePlacementsEvent->lrcHandle = event->lrc_handle;
    euExecQueuePlacementsEvent->numPlacements = event->num_placements;
    euExecQueuePlacementsEvent->vmHandle = event->vm_handle;
    memcpy(euExecQueuePlacementsEvent->instances, event->instances, event->num_placements * sizeof(uint64_t));

    auto deleter = [](EuDebugEventExecQueuePlacements *ptr) {
        free(ptr);
    };

    return std::unique_ptr<EuDebugEventExecQueuePlacements, void (*)(EuDebugEventExecQueuePlacements *)>(euExecQueuePlacementsEvent, deleter);
}
EuDebugEventMetadata MockEuDebugInterface::toEuDebugEventMetadata(const void *drmType) {
    return *static_cast<const EuDebugEventMetadata *>(drmType);
}
EuDebugEventVmBind MockEuDebugInterface::toEuDebugEventVmBind(const void *drmType) {
    return *static_cast<const EuDebugEventVmBind *>(drmType);
}
EuDebugEventVmBindOp MockEuDebugInterface::toEuDebugEventVmBindOp(const void *drmType) {
    return *static_cast<const EuDebugEventVmBindOp *>(drmType);
}
EuDebugEventVmBindOpMetadata MockEuDebugInterface::toEuDebugEventVmBindOpMetadata(const void *drmType) {
    return *static_cast<const EuDebugEventVmBindOpMetadata *>(drmType);
}
EuDebugEventVmBindUfence MockEuDebugInterface::toEuDebugEventVmBindUfence(const void *drmType) {
    return *static_cast<const EuDebugEventVmBindUfence *>(drmType);
}
std::unique_ptr<EuDebugEventPageFault, void (*)(EuDebugEventPageFault *)> MockEuDebugInterface::toEuDebugEventPageFault(const void *drmType) {
    const prelim_drm_xe_eudebug_event_pagefault *event = static_cast<const prelim_drm_xe_eudebug_event_pagefault *>(drmType);
    EuDebugEventPageFault *pPageFaultEvent = static_cast<EuDebugEventPageFault *>(malloc(sizeof(EuDebugEventPageFault) + event->bitmask_size * sizeof(uint8_t)));

    pPageFaultEvent->base.len = event->base.len;
    pPageFaultEvent->base.type = event->base.type;
    pPageFaultEvent->base.flags = event->base.flags;
    pPageFaultEvent->base.seqno = event->base.seqno;
    pPageFaultEvent->base.reserved = event->base.reserved;

    memcpy(pPageFaultEvent->bitmask, event->bitmask, event->bitmask_size * sizeof(uint8_t));
    pPageFaultEvent->bitmaskSize = event->bitmask_size;
    pPageFaultEvent->clientHandle = event->client_handle;
    pPageFaultEvent->flags = event->flags;
    pPageFaultEvent->execQueueHandle = event->exec_queue_handle;
    pPageFaultEvent->lrcHandle = event->lrc_handle;
    pPageFaultEvent->pagefaultAddress = event->pagefault_address;

    auto deleter = [](EuDebugEventPageFault *ptr) {
        free(ptr);
    };

    return std::unique_ptr<EuDebugEventPageFault, void (*)(EuDebugEventPageFault *)>(pPageFaultEvent, deleter);
}

EuDebugEuControl MockEuDebugInterface::toEuDebugEuControl(const void *drmType) {
    return *static_cast<const EuDebugEuControl *>(drmType);
}

EuDebugConnect MockEuDebugInterface::toEuDebugConnect(const void *drmType) {
    return *static_cast<const EuDebugConnect *>(drmType);
}

std::unique_ptr<void, void (*)(void *)> MockEuDebugInterface::toDrmEuDebugConnect(const EuDebugConnect &connect) {
    struct prelim_drm_xe_eudebug_connect *pDrmConnect = new prelim_drm_xe_eudebug_connect();

    pDrmConnect->extensions = connect.extensions;
    pDrmConnect->pid = connect.pid;
    pDrmConnect->flags = connect.flags;
    pDrmConnect->version = connect.version;

    auto deleter = [](void *ptr) {
        delete static_cast<prelim_drm_xe_eudebug_connect *>(ptr);
    };
    return std::unique_ptr<void, void (*)(void *)>(pDrmConnect, deleter);
}
std::unique_ptr<void, void (*)(void *)> MockEuDebugInterface::toDrmEuDebugEuControl(const EuDebugEuControl &euControl) {
    struct prelim_drm_xe_eudebug_eu_control *pDrmEuControl = new prelim_drm_xe_eudebug_eu_control();

    pDrmEuControl->bitmask_ptr = euControl.bitmaskPtr;
    pDrmEuControl->bitmask_size = euControl.bitmaskSize;
    pDrmEuControl->client_handle = euControl.clientHandle;
    pDrmEuControl->cmd = euControl.cmd;
    pDrmEuControl->flags = euControl.flags;
    pDrmEuControl->exec_queue_handle = euControl.execQueueHandle;
    pDrmEuControl->lrc_handle = euControl.lrcHandle;
    pDrmEuControl->seqno = euControl.seqno;

    auto deleter = [](void *ptr) {
        delete static_cast<prelim_drm_xe_eudebug_eu_control *>(ptr);
    };
    return std::unique_ptr<void, void (*)(void *)>(pDrmEuControl, deleter);
}
std::unique_ptr<void, void (*)(void *)> MockEuDebugInterface::toDrmEuDebugVmOpen(const EuDebugVmOpen &vmOpen) {
    struct prelim_drm_xe_eudebug_vm_open *pDrmVmOpen = new prelim_drm_xe_eudebug_vm_open();

    pDrmVmOpen->client_handle = vmOpen.clientHandle;
    pDrmVmOpen->extensions = vmOpen.extensions;
    pDrmVmOpen->flags = vmOpen.flags;
    pDrmVmOpen->timeout_ns = vmOpen.timeoutNs;
    pDrmVmOpen->vm_handle = vmOpen.vmHandle;

    auto deleter = [](void *ptr) {
        delete static_cast<prelim_drm_xe_eudebug_vm_open *>(ptr);
    };
    return std::unique_ptr<void, void (*)(void *)>(pDrmVmOpen, deleter);
}
std::unique_ptr<void, void (*)(void *)> MockEuDebugInterface::toDrmEuDebugAckEvent(const EuDebugAckEvent &ackEvent) {
    struct prelim_drm_xe_eudebug_ack_event *pDrmAckEvent = new prelim_drm_xe_eudebug_ack_event();

    pDrmAckEvent->type = ackEvent.type;
    pDrmAckEvent->flags = ackEvent.flags;
    pDrmAckEvent->seqno = ackEvent.seqno;

    auto deleter = [](void *ptr) {
        delete static_cast<prelim_drm_xe_eudebug_ack_event *>(ptr);
    };
    return std::unique_ptr<void, void (*)(void *)>(pDrmAckEvent, deleter);
}

char MockEuDebugInterface::sysFsContent = '1';
[[maybe_unused]] static EnableEuDebugInterface enableMockEuDebug(EuDebugInterfaceType::upstream, MockEuDebugInterface::sysFsXeEuDebugFile, []() -> std::unique_ptr<EuDebugInterface> { return std::make_unique<MockEuDebugInterface>(); });
} // namespace NEO
