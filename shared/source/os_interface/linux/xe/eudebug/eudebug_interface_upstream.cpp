/*
 * Copyright (C) 2024-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/xe/eudebug/eudebug_interface_upstream.h"

#include "shared/source/helpers/debug_helpers.h"

#include "third_party/uapi-eudebug/drm/xe_drm.h"

#include <string.h>
namespace NEO {
uint32_t EuDebugInterfaceUpstream::getParamValue(EuDebugParam param) const {
    switch (param) {
    case EuDebugParam::connect:
        return DRM_IOCTL_XE_EUDEBUG_CONNECT;
    case EuDebugParam::euControlCmdInterruptAll:
        return DRM_XE_EUDEBUG_EU_CONTROL_CMD_INTERRUPT_ALL;
    case EuDebugParam::euControlCmdResume:
        return DRM_XE_EUDEBUG_EU_CONTROL_CMD_RESUME;
    case EuDebugParam::euControlCmdStopped:
        return DRM_XE_EUDEBUG_EU_CONTROL_CMD_STOPPED;
    case EuDebugParam::eventBitCreate:
        return DRM_XE_EUDEBUG_EVENT_CREATE;
    case EuDebugParam::eventBitDestroy:
        return DRM_XE_EUDEBUG_EVENT_DESTROY;
    case EuDebugParam::eventBitNeedAck:
        return DRM_XE_EUDEBUG_EVENT_NEED_ACK;
    case EuDebugParam::eventBitStateChange:
        return DRM_XE_EUDEBUG_EVENT_STATE_CHANGE;
    case EuDebugParam::eventTypeEuAttention:
        return DRM_XE_EUDEBUG_EVENT_EU_ATTENTION;
    case EuDebugParam::eventTypeExecQueue:
        return DRM_XE_EUDEBUG_EVENT_EXEC_QUEUE;
    case EuDebugParam::eventTypeExecQueuePlacements:
        return 0;
    case EuDebugParam::eventTypeMetadata:
        return 0;
    case EuDebugParam::eventTypeOpen:
        return 0;
    case EuDebugParam::eventTypePagefault:
        return DRM_XE_EUDEBUG_EVENT_PAGEFAULT;
    case EuDebugParam::eventTypeRead:
        return DRM_XE_EUDEBUG_EVENT_READ;
    case EuDebugParam::eventTypeVm:
        return DRM_XE_EUDEBUG_EVENT_VM;
    case EuDebugParam::eventTypeVmBind:
        return DRM_XE_EUDEBUG_EVENT_VM_BIND;
    case EuDebugParam::eventTypeVmBindOp:
        return 0;
    case EuDebugParam::eventTypeVmBindOpMetadata:
        return 0;
    case EuDebugParam::eventTypeVmBindUfence:
        return DRM_XE_EUDEBUG_EVENT_VM_BIND_UFENCE;
    case EuDebugParam::eventVmBindFlagUfence:
        return DRM_XE_EUDEBUG_EVENT_VM_BIND_FLAG_UFENCE;
    case EuDebugParam::execQueueSetPropertyEuDebug:
        return DRM_XE_EXEC_QUEUE_SET_PROPERTY_EUDEBUG;
    case EuDebugParam::execQueueSetPropertyValueEnable:
        return DRM_XE_EXEC_QUEUE_EUDEBUG_FLAG_ENABLE;
    case EuDebugParam::execQueueSetPropertyValuePageFaultEnable:
        return 0;
    case EuDebugParam::ioctlAckEvent:
        return DRM_XE_EUDEBUG_IOCTL_ACK_EVENT;
    case EuDebugParam::ioctlEuControl:
        return DRM_XE_EUDEBUG_IOCTL_EU_CONTROL;
    case EuDebugParam::ioctlReadEvent:
        return DRM_XE_EUDEBUG_IOCTL_READ_EVENT;
    case EuDebugParam::ioctlReadMetadata:
        return 0;
    case EuDebugParam::ioctlVmOpen:
        return DRM_XE_EUDEBUG_IOCTL_VM_OPEN;
    case EuDebugParam::metadataCreate:
        return 0;
    case EuDebugParam::metadataDestroy:
        return 0;
    case EuDebugParam::metadataElfBinary:
        return 0;
    case EuDebugParam::metadataModuleArea:
        return 0;
    case EuDebugParam::metadataProgramModule:
        return 0;
    case EuDebugParam::metadataSbaArea:
        return 0;
    case EuDebugParam::metadataSipArea:
        return 0;
    case EuDebugParam::vmBindOpExtensionsAttachDebug:
        return 0;
    }
    return 0;
}

EuDebugInterfaceType EuDebugInterfaceUpstream::getInterfaceType() const {
    return EuDebugInterfaceType::upstream;
}

uint64_t EuDebugInterfaceUpstream::getDefaultClientHandle() const {
    return defaultClientHandle;
}

std::unique_ptr<EuDebugEventEuAttention, void (*)(EuDebugEventEuAttention *)> EuDebugInterfaceUpstream::toEuDebugEventEuAttention(const void *drmType) {
    const drm_xe_eudebug_event_eu_attention *event = static_cast<const drm_xe_eudebug_event_eu_attention *>(drmType);
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
    pEuAttentionEvent->clientHandle = defaultClientHandle;

    auto deleter = [](EuDebugEventEuAttention *ptr) {
        free(ptr);
    };

    return std::unique_ptr<EuDebugEventEuAttention, void (*)(EuDebugEventEuAttention *)>(pEuAttentionEvent, deleter);
}

EuDebugEventClient EuDebugInterfaceUpstream::toEuDebugEventClient(const void *drmType) {
    UNRECOVERABLE_IF(true);
    return {};
}

EuDebugEventVm EuDebugInterfaceUpstream::toEuDebugEventVm(const void *drmType) {
    const drm_xe_eudebug_event_vm *event = static_cast<const drm_xe_eudebug_event_vm *>(drmType);
    EuDebugEventVm euVmEvent = {};

    euVmEvent.base.len = event->base.len;
    euVmEvent.base.type = event->base.type;
    euVmEvent.base.flags = event->base.flags;
    euVmEvent.base.seqno = event->base.seqno;
    euVmEvent.base.reserved = event->base.reserved;
    euVmEvent.vmHandle = event->vm_handle;
    euVmEvent.clientHandle = defaultClientHandle;

    return euVmEvent;
}

std::unique_ptr<EuDebugEventExecQueue, void (*)(EuDebugEventExecQueue *)> EuDebugInterfaceUpstream::toEuDebugEventExecQueue(const void *drmType) {
    const drm_xe_eudebug_event_exec_queue *event = static_cast<const drm_xe_eudebug_event_exec_queue *>(drmType);
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
    pExecQueueEvent->clientHandle = defaultClientHandle;
    memcpy(pExecQueueEvent->lrcHandle, event->lrc_handle, event->width * sizeof(uint64_t));

    auto deleter = [](EuDebugEventExecQueue *ptr) {
        free(ptr);
    };

    return std::unique_ptr<EuDebugEventExecQueue, void (*)(EuDebugEventExecQueue *)>(pExecQueueEvent, deleter);
}

std::unique_ptr<EuDebugEventExecQueuePlacements, void (*)(EuDebugEventExecQueuePlacements *)> EuDebugInterfaceUpstream::toEuDebugEventExecQueuePlacements(const void *drmType) {

    UNRECOVERABLE_IF(true);
    return std::unique_ptr<EuDebugEventExecQueuePlacements, void (*)(EuDebugEventExecQueuePlacements *)>(nullptr, [](EuDebugEventExecQueuePlacements *ptr) {});
}

EuDebugEventMetadata EuDebugInterfaceUpstream::toEuDebugEventMetadata(const void *drmType) {

    UNRECOVERABLE_IF(true);
    return {};
}

EuDebugEventVmBind EuDebugInterfaceUpstream::toEuDebugEventVmBind(const void *drmType) {
    const drm_xe_eudebug_event_vm_bind *event = static_cast<const drm_xe_eudebug_event_vm_bind *>(drmType);
    EuDebugEventVmBind vmBindEvent = {};

    vmBindEvent.base.len = event->base.len;
    vmBindEvent.base.type = event->base.type;
    vmBindEvent.base.flags = event->base.flags;
    vmBindEvent.base.seqno = event->base.seqno;
    vmBindEvent.base.reserved = event->base.reserved;
    vmBindEvent.flags = event->flags;
    vmBindEvent.numBinds = event->num_binds;
    vmBindEvent.vmHandle = event->vm_handle;
    vmBindEvent.clientHandle = defaultClientHandle;

    return vmBindEvent;
}

NEO::EuDebugEventVmBindOp EuDebugInterfaceUpstream::toEuDebugEventVmBindOp(const void *drmType) {

    UNRECOVERABLE_IF(true);
    return {};
}

EuDebugEventVmBindOpMetadata EuDebugInterfaceUpstream::toEuDebugEventVmBindOpMetadata(const void *drmType) {

    UNRECOVERABLE_IF(true);
    return {};
}

EuDebugEventVmBindUfence EuDebugInterfaceUpstream::toEuDebugEventVmBindUfence(const void *drmType) {
    const drm_xe_eudebug_event_vm_bind_ufence *event = static_cast<const drm_xe_eudebug_event_vm_bind_ufence *>(drmType);
    EuDebugEventVmBindUfence vmBindUfenceEvent = {};

    vmBindUfenceEvent.base.len = event->base.len;
    vmBindUfenceEvent.base.type = event->base.type;
    vmBindUfenceEvent.base.flags = event->base.flags;
    vmBindUfenceEvent.base.seqno = event->base.seqno;
    vmBindUfenceEvent.base.reserved = event->base.reserved;

    vmBindUfenceEvent.vmBindRefSeqno = event->vm_bind_ref_seqno;

    return vmBindUfenceEvent;
}

std::unique_ptr<EuDebugEventPageFault, void (*)(EuDebugEventPageFault *)> EuDebugInterfaceUpstream::toEuDebugEventPageFault(const void *drmType) {
    const drm_xe_eudebug_event_pagefault *event = static_cast<const drm_xe_eudebug_event_pagefault *>(drmType);
    EuDebugEventPageFault *pPageFaultEvent = static_cast<EuDebugEventPageFault *>(malloc(sizeof(EuDebugEventPageFault) + event->bitmask_size * sizeof(uint8_t)));

    pPageFaultEvent->base.len = event->base.len;
    pPageFaultEvent->base.type = event->base.type;
    pPageFaultEvent->base.flags = event->base.flags;
    pPageFaultEvent->base.seqno = event->base.seqno;
    pPageFaultEvent->base.reserved = event->base.reserved;

    memcpy(pPageFaultEvent->bitmask, event->bitmask, event->bitmask_size * sizeof(uint8_t));
    pPageFaultEvent->bitmaskSize = event->bitmask_size;
    pPageFaultEvent->flags = event->flags;
    pPageFaultEvent->execQueueHandle = event->exec_queue_handle;
    pPageFaultEvent->lrcHandle = event->lrc_handle;
    pPageFaultEvent->pagefaultAddress = event->pagefault_address;
    pPageFaultEvent->clientHandle = defaultClientHandle;

    auto deleter = [](EuDebugEventPageFault *ptr) {
        free(ptr);
    };

    return std::unique_ptr<EuDebugEventPageFault, void (*)(EuDebugEventPageFault *)>(pPageFaultEvent, deleter);
}

EuDebugEuControl EuDebugInterfaceUpstream::toEuDebugEuControl(const void *drmType) {
    const drm_xe_eudebug_eu_control *euControl = static_cast<const drm_xe_eudebug_eu_control *>(drmType);
    EuDebugEuControl control = {};

    control.bitmaskPtr = euControl->bitmask_ptr;
    control.bitmaskSize = euControl->bitmask_size;
    control.cmd = euControl->cmd;
    control.flags = euControl->flags;
    control.execQueueHandle = euControl->exec_queue_handle;
    control.lrcHandle = euControl->lrc_handle;
    control.seqno = euControl->seqno;
    control.clientHandle = defaultClientHandle;

    return control;
}

EuDebugConnect EuDebugInterfaceUpstream::toEuDebugConnect(const void *drmType) {
    const drm_xe_eudebug_connect *event = static_cast<const drm_xe_eudebug_connect *>(drmType);
    EuDebugConnect connectEvent = {};

    connectEvent.extensions = event->extensions;
    connectEvent.flags = event->flags;
    connectEvent.version = event->version;

    return connectEvent;
}

std::unique_ptr<void, void (*)(void *)> EuDebugInterfaceUpstream::toDrmEuDebugConnect(const EuDebugConnect &connect) {
    struct drm_xe_eudebug_connect *pDrmConnect = new drm_xe_eudebug_connect();

    pDrmConnect->extensions = connect.extensions;
    pDrmConnect->flags = connect.flags;
    pDrmConnect->version = connect.version;

    auto deleter = [](void *ptr) {
        delete static_cast<drm_xe_eudebug_connect *>(ptr);
    };
    return std::unique_ptr<void, void (*)(void *)>(pDrmConnect, deleter);
}

std::unique_ptr<void, void (*)(void *)> EuDebugInterfaceUpstream::toDrmEuDebugEuControl(const EuDebugEuControl &euControl) {
    struct drm_xe_eudebug_eu_control *pDrmEuControl = new drm_xe_eudebug_eu_control();

    pDrmEuControl->bitmask_ptr = euControl.bitmaskPtr;
    pDrmEuControl->bitmask_size = euControl.bitmaskSize;
    pDrmEuControl->cmd = euControl.cmd;
    pDrmEuControl->flags = euControl.flags;
    pDrmEuControl->exec_queue_handle = euControl.execQueueHandle;
    pDrmEuControl->lrc_handle = euControl.lrcHandle;
    pDrmEuControl->seqno = euControl.seqno;

    auto deleter = [](void *ptr) {
        delete static_cast<drm_xe_eudebug_eu_control *>(ptr);
    };
    return std::unique_ptr<void, void (*)(void *)>(pDrmEuControl, deleter);
}

std::unique_ptr<void, void (*)(void *)> EuDebugInterfaceUpstream::toDrmEuDebugVmOpen(const EuDebugVmOpen &vmOpen) {
    struct drm_xe_eudebug_vm_open *pDrmVmOpen = new drm_xe_eudebug_vm_open();

    pDrmVmOpen->extensions = vmOpen.extensions;
    pDrmVmOpen->flags = vmOpen.flags;
    pDrmVmOpen->timeout_ns = vmOpen.timeoutNs;
    pDrmVmOpen->vm_handle = vmOpen.vmHandle;

    auto deleter = [](void *ptr) {
        delete static_cast<drm_xe_eudebug_vm_open *>(ptr);
    };
    return std::unique_ptr<void, void (*)(void *)>(pDrmVmOpen, deleter);
}

std::unique_ptr<void, void (*)(void *)> EuDebugInterfaceUpstream::toDrmEuDebugAckEvent(const EuDebugAckEvent &ackEvent) {
    struct drm_xe_eudebug_ack_event *pDrmAckEvent = new drm_xe_eudebug_ack_event();

    pDrmAckEvent->type = ackEvent.type;
    pDrmAckEvent->flags = ackEvent.flags;
    pDrmAckEvent->seqno = ackEvent.seqno;

    auto deleter = [](void *ptr) {
        delete static_cast<drm_xe_eudebug_ack_event *>(ptr);
    };
    return std::unique_ptr<void, void (*)(void *)>(pDrmAckEvent, deleter);
}

static_assert(offsetof(EuDebugEvent, len) == offsetof(drm_xe_eudebug_event, len));
static_assert(offsetof(EuDebugEvent, type) == offsetof(drm_xe_eudebug_event, type));
static_assert(offsetof(EuDebugEvent, flags) == offsetof(drm_xe_eudebug_event, flags));
static_assert(offsetof(EuDebugEvent, seqno) == offsetof(drm_xe_eudebug_event, seqno));
static_assert(offsetof(EuDebugEvent, reserved) == offsetof(drm_xe_eudebug_event, reserved));

static_assert(sizeof(XeUserExtension) == sizeof(drm_xe_user_extension));
static_assert(offsetof(XeUserExtension, nextExtension) == offsetof(drm_xe_user_extension, next_extension));
static_assert(offsetof(XeUserExtension, name) == offsetof(drm_xe_user_extension, name));
static_assert(offsetof(XeUserExtension, pad) == offsetof(drm_xe_user_extension, pad));

} // namespace NEO
