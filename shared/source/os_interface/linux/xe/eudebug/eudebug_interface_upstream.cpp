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
        return DRM_XE_EUDEBUG_EVENT_METADATA;
    case EuDebugParam::eventTypeOpen:
        return DRM_XE_EUDEBUG_EVENT_OPEN;
    case EuDebugParam::eventTypePagefault:
        return DRM_XE_EUDEBUG_EVENT_PAGEFAULT;
    case EuDebugParam::eventTypeRead:
        return DRM_XE_EUDEBUG_EVENT_READ;
    case EuDebugParam::eventTypeVm:
        return DRM_XE_EUDEBUG_EVENT_VM;
    case EuDebugParam::eventTypeVmBind:
        return DRM_XE_EUDEBUG_EVENT_VM_BIND;
    case EuDebugParam::eventTypeVmBindOp:
        return DRM_XE_EUDEBUG_EVENT_VM_BIND_OP;
    case EuDebugParam::eventTypeVmBindOpMetadata:
        return DRM_XE_EUDEBUG_EVENT_VM_BIND_OP_METADATA;
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
        return DRM_XE_EUDEBUG_IOCTL_READ_METADATA;
    case EuDebugParam::ioctlVmOpen:
        return DRM_XE_EUDEBUG_IOCTL_VM_OPEN;
    case EuDebugParam::metadataCreate:
        return DRM_IOCTL_XE_DEBUG_METADATA_CREATE;
    case EuDebugParam::metadataDestroy:
        return DRM_IOCTL_XE_DEBUG_METADATA_DESTROY;
    case EuDebugParam::metadataElfBinary:
        return DRM_XE_DEBUG_METADATA_ELF_BINARY;
    case EuDebugParam::metadataModuleArea:
        return WORK_IN_PROGRESS_DRM_XE_DEBUG_METADATA_MODULE_AREA;
    case EuDebugParam::metadataProgramModule:
        return DRM_XE_DEBUG_METADATA_PROGRAM_MODULE;
    case EuDebugParam::metadataSbaArea:
        return WORK_IN_PROGRESS_DRM_XE_DEBUG_METADATA_SBA_AREA;
    case EuDebugParam::metadataSipArea:
        return WORK_IN_PROGRESS_DRM_XE_DEBUG_METADATA_SIP_AREA;
    case EuDebugParam::vmBindOpExtensionsAttachDebug:
        return XE_VM_BIND_OP_EXTENSIONS_ATTACH_DEBUG;
    }
    return 0;
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
    pEuAttentionEvent->clientHandle = event->client_handle;

    auto deleter = [](EuDebugEventEuAttention *ptr) {
        free(ptr);
    };

    return std::unique_ptr<EuDebugEventEuAttention, void (*)(EuDebugEventEuAttention *)>(pEuAttentionEvent, deleter);
}

EuDebugEventClient EuDebugInterfaceUpstream::toEuDebugEventClient(const void *drmType) {
    const drm_xe_eudebug_event_client *event = static_cast<const drm_xe_eudebug_event_client *>(drmType);
    EuDebugEventClient euClientEvent = {};

    euClientEvent.base.len = event->base.len;
    euClientEvent.base.type = event->base.type;
    euClientEvent.base.flags = event->base.flags;
    euClientEvent.base.seqno = event->base.seqno;
    euClientEvent.base.reserved = event->base.reserved;

    euClientEvent.clientHandle = event->client_handle;

    return euClientEvent;
}

EuDebugEventVm EuDebugInterfaceUpstream::toEuDebugEventVm(const void *drmType) {
    const drm_xe_eudebug_event_vm *event = static_cast<const drm_xe_eudebug_event_vm *>(drmType);
    EuDebugEventVm euVmEvent = {};

    euVmEvent.base.len = event->base.len;
    euVmEvent.base.type = event->base.type;
    euVmEvent.base.flags = event->base.flags;
    euVmEvent.base.seqno = event->base.seqno;
    euVmEvent.base.reserved = event->base.reserved;

    euVmEvent.clientHandle = event->client_handle;
    euVmEvent.vmHandle = event->vm_handle;

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
    pExecQueueEvent->clientHandle = event->client_handle;
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
    const drm_xe_eudebug_event_metadata *event = static_cast<const drm_xe_eudebug_event_metadata *>(drmType);
    EuDebugEventMetadata metadataEvent = {};

    metadataEvent.base.len = event->base.len;
    metadataEvent.base.type = event->base.type;
    metadataEvent.base.flags = event->base.flags;
    metadataEvent.base.seqno = event->base.seqno;
    metadataEvent.base.reserved = event->base.reserved;

    metadataEvent.clientHandle = event->client_handle;
    metadataEvent.len = event->len;
    metadataEvent.metadataHandle = event->metadata_handle;
    metadataEvent.type = event->type;

    return metadataEvent;
}

EuDebugEventVmBind EuDebugInterfaceUpstream::toEuDebugEventVmBind(const void *drmType) {
    const drm_xe_eudebug_event_vm_bind *event = static_cast<const drm_xe_eudebug_event_vm_bind *>(drmType);
    EuDebugEventVmBind vmBindEvent = {};

    vmBindEvent.base.len = event->base.len;
    vmBindEvent.base.type = event->base.type;
    vmBindEvent.base.flags = event->base.flags;
    vmBindEvent.base.seqno = event->base.seqno;
    vmBindEvent.base.reserved = event->base.reserved;

    vmBindEvent.clientHandle = event->client_handle;
    vmBindEvent.flags = event->flags;
    vmBindEvent.numBinds = event->num_binds;
    vmBindEvent.vmHandle = event->vm_handle;

    return vmBindEvent;
}

NEO::EuDebugEventVmBindOp EuDebugInterfaceUpstream::toEuDebugEventVmBindOp(const void *drmType) {
    const drm_xe_eudebug_event_vm_bind_op *event = static_cast<const drm_xe_eudebug_event_vm_bind_op *>(drmType);
    EuDebugEventVmBindOp vmBindOpEvent = {};

    vmBindOpEvent.base.len = event->base.len;
    vmBindOpEvent.base.type = event->base.type;
    vmBindOpEvent.base.flags = event->base.flags;
    vmBindOpEvent.base.seqno = event->base.seqno;
    vmBindOpEvent.base.reserved = event->base.reserved;

    vmBindOpEvent.vmBindRefSeqno = event->vm_bind_ref_seqno;
    vmBindOpEvent.numExtensions = event->num_extensions;
    vmBindOpEvent.addr = event->addr;
    vmBindOpEvent.range = event->range;

    return vmBindOpEvent;
}

EuDebugEventVmBindOpMetadata EuDebugInterfaceUpstream::toEuDebugEventVmBindOpMetadata(const void *drmType) {
    const drm_xe_eudebug_event_vm_bind_op_metadata *event = static_cast<const drm_xe_eudebug_event_vm_bind_op_metadata *>(drmType);
    EuDebugEventVmBindOpMetadata vmBindOpMetadataEvent = {};

    vmBindOpMetadataEvent.base.len = event->base.len;
    vmBindOpMetadataEvent.base.type = event->base.type;
    vmBindOpMetadataEvent.base.flags = event->base.flags;
    vmBindOpMetadataEvent.base.seqno = event->base.seqno;
    vmBindOpMetadataEvent.base.reserved = event->base.reserved;

    vmBindOpMetadataEvent.vmBindOpRefSeqno = event->vm_bind_op_ref_seqno;
    vmBindOpMetadataEvent.metadataHandle = event->metadata_handle;
    vmBindOpMetadataEvent.metadataCookie = event->metadata_cookie;

    return vmBindOpMetadataEvent;
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

EuDebugEuControl EuDebugInterfaceUpstream::toEuDebugEuControl(const void *drmType) {
    const drm_xe_eudebug_eu_control *euControl = static_cast<const drm_xe_eudebug_eu_control *>(drmType);
    EuDebugEuControl control = {};

    control.bitmaskPtr = euControl->bitmask_ptr;
    control.bitmaskSize = euControl->bitmask_size;
    control.clientHandle = euControl->client_handle;
    control.cmd = euControl->cmd;
    control.flags = euControl->flags;
    control.execQueueHandle = euControl->exec_queue_handle;
    control.lrcHandle = euControl->lrc_handle;
    control.seqno = euControl->seqno;

    return control;
}

EuDebugConnect EuDebugInterfaceUpstream::toEuDebugConnect(const void *drmType) {
    const drm_xe_eudebug_connect *event = static_cast<const drm_xe_eudebug_connect *>(drmType);
    EuDebugConnect connectEvent = {};

    connectEvent.extensions = event->extensions;
    connectEvent.flags = event->flags;
    connectEvent.pid = event->pid;
    connectEvent.version = event->version;

    return connectEvent;
}

std::unique_ptr<void, void (*)(void *)> EuDebugInterfaceUpstream::toDrmEuDebugConnect(const EuDebugConnect &connect) {
    struct drm_xe_eudebug_connect *pDrmConnect = new drm_xe_eudebug_connect();

    pDrmConnect->extensions = connect.extensions;
    pDrmConnect->pid = connect.pid;
    pDrmConnect->flags = connect.flags;
    pDrmConnect->version = connect.version;

    auto deleter = [](void *ptr) {
        delete static_cast<drm_xe_eudebug_connect *>(ptr);
    };
    return std::unique_ptr<void, void (*)(void *)>(pDrmConnect, deleter);
}

std::unique_ptr<void, void (*)(void *)> EuDebugInterfaceUpstream::toDrmEuDebugEuControl(const EuDebugEuControl &euControl) {
    struct drm_xe_eudebug_eu_control *pDrmEuControl = new drm_xe_eudebug_eu_control();

    auto bitmaskData = new uint8_t[euControl.bitmaskSize];
    memcpy(bitmaskData, reinterpret_cast<uint8_t *>(euControl.bitmaskPtr), euControl.bitmaskSize * sizeof(uint8_t));
    pDrmEuControl->bitmask_ptr = reinterpret_cast<uintptr_t>(bitmaskData);
    pDrmEuControl->bitmask_size = euControl.bitmaskSize;
    pDrmEuControl->client_handle = euControl.clientHandle;
    pDrmEuControl->cmd = euControl.cmd;
    pDrmEuControl->flags = euControl.flags;
    pDrmEuControl->exec_queue_handle = euControl.execQueueHandle;
    pDrmEuControl->lrc_handle = euControl.lrcHandle;
    pDrmEuControl->seqno = euControl.seqno;

    auto deleter = [](void *ptr) {
        delete[] reinterpret_cast<uint8_t *>(static_cast<drm_xe_eudebug_eu_control *>(ptr)->bitmask_ptr);
        delete static_cast<drm_xe_eudebug_eu_control *>(ptr);
    };
    return std::unique_ptr<void, void (*)(void *)>(pDrmEuControl, deleter);
}

std::unique_ptr<void, void (*)(void *)> EuDebugInterfaceUpstream::toDrmEuDebugVmOpen(const EuDebugVmOpen &vmOpen) {
    struct drm_xe_eudebug_vm_open *pDrmVmOpen = new drm_xe_eudebug_vm_open();

    pDrmVmOpen->client_handle = vmOpen.clientHandle;
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

static_assert(sizeof(EuDebugReadMetadata) == sizeof(drm_xe_eudebug_read_metadata));
static_assert(offsetof(EuDebugReadMetadata, clientHandle) == offsetof(drm_xe_eudebug_read_metadata, client_handle));
static_assert(offsetof(EuDebugReadMetadata, metadataHandle) == offsetof(drm_xe_eudebug_read_metadata, metadata_handle));
static_assert(offsetof(EuDebugReadMetadata, flags) == offsetof(drm_xe_eudebug_read_metadata, flags));
static_assert(offsetof(EuDebugReadMetadata, reserved) == offsetof(drm_xe_eudebug_read_metadata, reserved));
static_assert(offsetof(EuDebugReadMetadata, ptr) == offsetof(drm_xe_eudebug_read_metadata, ptr));
static_assert(offsetof(EuDebugReadMetadata, size) == offsetof(drm_xe_eudebug_read_metadata, size));

static_assert(sizeof(DebugMetadataCreate) == sizeof(drm_xe_debug_metadata_create));
static_assert(offsetof(DebugMetadataCreate, extensions) == offsetof(drm_xe_debug_metadata_create, extensions));
static_assert(offsetof(DebugMetadataCreate, type) == offsetof(drm_xe_debug_metadata_create, type));
static_assert(offsetof(DebugMetadataCreate, userAddr) == offsetof(drm_xe_debug_metadata_create, user_addr));
static_assert(offsetof(DebugMetadataCreate, len) == offsetof(drm_xe_debug_metadata_create, len));
static_assert(offsetof(DebugMetadataCreate, metadataId) == offsetof(drm_xe_debug_metadata_create, metadata_id));

static_assert(sizeof(DebugMetadataDestroy) == sizeof(drm_xe_debug_metadata_destroy));
static_assert(offsetof(DebugMetadataDestroy, extensions) == offsetof(drm_xe_debug_metadata_destroy, extensions));
static_assert(offsetof(DebugMetadataDestroy, metadataId) == offsetof(drm_xe_debug_metadata_destroy, metadata_id));

static_assert(sizeof(XeUserExtension) == sizeof(drm_xe_user_extension));
static_assert(offsetof(XeUserExtension, nextExtension) == offsetof(drm_xe_user_extension, next_extension));
static_assert(offsetof(XeUserExtension, name) == offsetof(drm_xe_user_extension, name));
static_assert(offsetof(XeUserExtension, pad) == offsetof(drm_xe_user_extension, pad));

static_assert(sizeof(VmBindOpExtAttachDebug) == sizeof(drm_xe_vm_bind_op_ext_attach_debug));
static_assert(offsetof(VmBindOpExtAttachDebug, base) == offsetof(drm_xe_vm_bind_op_ext_attach_debug, base));
static_assert(offsetof(VmBindOpExtAttachDebug, metadataId) == offsetof(drm_xe_vm_bind_op_ext_attach_debug, metadata_id));
static_assert(offsetof(VmBindOpExtAttachDebug, flags) == offsetof(drm_xe_vm_bind_op_ext_attach_debug, flags));
static_assert(offsetof(VmBindOpExtAttachDebug, cookie) == offsetof(drm_xe_vm_bind_op_ext_attach_debug, cookie));
static_assert(offsetof(VmBindOpExtAttachDebug, reserved) == offsetof(drm_xe_vm_bind_op_ext_attach_debug, reserved));

} // namespace NEO
