/*
 * Copyright (C) 2024-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/xe/eudebug/eudebug_interface_prelim.h"

#include "shared/source/os_interface/linux/xe/xedrm_prelim.h"

namespace NEO {
uint32_t EuDebugInterfacePrelim::getParamValue(EuDebugParam param) const {
    switch (param) {
    case EuDebugParam::connect:
        return PRELIM_DRM_IOCTL_XE_EUDEBUG_CONNECT;
    case EuDebugParam::euControlCmdInterruptAll:
        return PRELIM_DRM_XE_EUDEBUG_EU_CONTROL_CMD_INTERRUPT_ALL;
    case EuDebugParam::euControlCmdResume:
        return PRELIM_DRM_XE_EUDEBUG_EU_CONTROL_CMD_RESUME;
    case EuDebugParam::euControlCmdStopped:
        return PRELIM_DRM_XE_EUDEBUG_EU_CONTROL_CMD_STOPPED;
    case EuDebugParam::eventBitCreate:
        return PRELIM_DRM_XE_EUDEBUG_EVENT_CREATE;
    case EuDebugParam::eventBitDestroy:
        return PRELIM_DRM_XE_EUDEBUG_EVENT_DESTROY;
    case EuDebugParam::eventBitNeedAck:
        return PRELIM_DRM_XE_EUDEBUG_EVENT_NEED_ACK;
    case EuDebugParam::eventBitStateChange:
        return PRELIM_DRM_XE_EUDEBUG_EVENT_STATE_CHANGE;
    case EuDebugParam::eventTypeEuAttention:
        return PRELIM_DRM_XE_EUDEBUG_EVENT_EU_ATTENTION;
    case EuDebugParam::eventTypeExecQueue:
        return PRELIM_DRM_XE_EUDEBUG_EVENT_EXEC_QUEUE;
    case EuDebugParam::eventTypeExecQueuePlacements:
        return PRELIM_DRM_XE_EUDEBUG_EVENT_EXEC_QUEUE_PLACEMENTS;
    case EuDebugParam::eventTypeMetadata:
        return PRELIM_DRM_XE_EUDEBUG_EVENT_METADATA;
    case EuDebugParam::eventTypeOpen:
        return PRELIM_DRM_XE_EUDEBUG_EVENT_OPEN;
    case EuDebugParam::eventTypePagefault:
        return PRELIM_DRM_XE_EUDEBUG_EVENT_PAGEFAULT;
    case EuDebugParam::eventTypeRead:
        return PRELIM_DRM_XE_EUDEBUG_EVENT_READ;
    case EuDebugParam::eventTypeVm:
        return PRELIM_DRM_XE_EUDEBUG_EVENT_VM;
    case EuDebugParam::eventTypeVmBind:
        return PRELIM_DRM_XE_EUDEBUG_EVENT_VM_BIND;
    case EuDebugParam::eventTypeVmBindOp:
        return PRELIM_DRM_XE_EUDEBUG_EVENT_VM_BIND_OP;
    case EuDebugParam::eventTypeVmBindOpMetadata:
        return PRELIM_DRM_XE_EUDEBUG_EVENT_VM_BIND_OP_METADATA;
    case EuDebugParam::eventTypeVmBindUfence:
        return PRELIM_DRM_XE_EUDEBUG_EVENT_VM_BIND_UFENCE;
    case EuDebugParam::eventVmBindFlagUfence:
        return PRELIM_DRM_XE_EUDEBUG_EVENT_VM_BIND_FLAG_UFENCE;
    case EuDebugParam::execQueueSetPropertyEuDebug:
        return PRELIM_DRM_XE_EXEC_QUEUE_SET_PROPERTY_EUDEBUG;
    case EuDebugParam::execQueueSetPropertyValueEnable:
        return PRELIM_DRM_XE_EXEC_QUEUE_EUDEBUG_FLAG_ENABLE;
    case EuDebugParam::ioctlAckEvent:
        return PRELIM_DRM_XE_EUDEBUG_IOCTL_ACK_EVENT;
    case EuDebugParam::ioctlEuControl:
        return PRELIM_DRM_XE_EUDEBUG_IOCTL_EU_CONTROL;
    case EuDebugParam::ioctlReadEvent:
        return PRELIM_DRM_XE_EUDEBUG_IOCTL_READ_EVENT;
    case EuDebugParam::ioctlReadMetadata:
        return PRELIM_DRM_XE_EUDEBUG_IOCTL_READ_METADATA;
    case EuDebugParam::ioctlVmOpen:
        return PRELIM_DRM_XE_EUDEBUG_IOCTL_VM_OPEN;
    case EuDebugParam::metadataCreate:
        return PRELIM_DRM_IOCTL_XE_DEBUG_METADATA_CREATE;
    case EuDebugParam::metadataDestroy:
        return PRELIM_DRM_IOCTL_XE_DEBUG_METADATA_DESTROY;
    case EuDebugParam::metadataElfBinary:
        return PRELIM_DRM_XE_DEBUG_METADATA_ELF_BINARY;
    case EuDebugParam::metadataModuleArea:
        return PRELIM_WORK_IN_PROGRESS_DRM_XE_DEBUG_METADATA_MODULE_AREA;
    case EuDebugParam::metadataProgramModule:
        return PRELIM_DRM_XE_DEBUG_METADATA_PROGRAM_MODULE;
    case EuDebugParam::metadataSbaArea:
        return PRELIM_WORK_IN_PROGRESS_DRM_XE_DEBUG_METADATA_SBA_AREA;
    case EuDebugParam::metadataSipArea:
        return PRELIM_WORK_IN_PROGRESS_DRM_XE_DEBUG_METADATA_SIP_AREA;
    case EuDebugParam::vmBindOpExtensionsAttachDebug:
        return PRELIM_XE_VM_BIND_OP_EXTENSIONS_ATTACH_DEBUG;
    default:
        return getAdditionalParamValue(param);
    }
    return 0;
}

static_assert(sizeof(EuDebugConnect) == sizeof(prelim_drm_xe_eudebug_connect));
static_assert(offsetof(EuDebugConnect, extensions) == offsetof(prelim_drm_xe_eudebug_connect, extensions));
static_assert(offsetof(EuDebugConnect, pid) == offsetof(prelim_drm_xe_eudebug_connect, pid));
static_assert(offsetof(EuDebugConnect, flags) == offsetof(prelim_drm_xe_eudebug_connect, flags));
static_assert(offsetof(EuDebugConnect, version) == offsetof(prelim_drm_xe_eudebug_connect, version));

static_assert(sizeof(EuDebugEvent) == sizeof(prelim_drm_xe_eudebug_event));
static_assert(offsetof(EuDebugEvent, len) == offsetof(prelim_drm_xe_eudebug_event, len));
static_assert(offsetof(EuDebugEvent, type) == offsetof(prelim_drm_xe_eudebug_event, type));
static_assert(offsetof(EuDebugEvent, flags) == offsetof(prelim_drm_xe_eudebug_event, flags));
static_assert(offsetof(EuDebugEvent, seqno) == offsetof(prelim_drm_xe_eudebug_event, seqno));
static_assert(offsetof(EuDebugEvent, reserved) == offsetof(prelim_drm_xe_eudebug_event, reserved));

static_assert(sizeof(EuDebugEventEuAttention) == sizeof(prelim_drm_xe_eudebug_event_eu_attention));
static_assert(offsetof(EuDebugEventEuAttention, base) == offsetof(prelim_drm_xe_eudebug_event_eu_attention, base));
static_assert(offsetof(EuDebugEventEuAttention, clientHandle) == offsetof(prelim_drm_xe_eudebug_event_eu_attention, client_handle));
static_assert(offsetof(EuDebugEventEuAttention, execQueueHandle) == offsetof(prelim_drm_xe_eudebug_event_eu_attention, exec_queue_handle));
static_assert(offsetof(EuDebugEventEuAttention, lrcHandle) == offsetof(prelim_drm_xe_eudebug_event_eu_attention, lrc_handle));
static_assert(offsetof(EuDebugEventEuAttention, flags) == offsetof(prelim_drm_xe_eudebug_event_eu_attention, flags));
static_assert(offsetof(EuDebugEventEuAttention, bitmaskSize) == offsetof(prelim_drm_xe_eudebug_event_eu_attention, bitmask_size));
static_assert(offsetof(EuDebugEventEuAttention, bitmask) == offsetof(prelim_drm_xe_eudebug_event_eu_attention, bitmask));

static_assert(sizeof(EuDebugEventClient) == sizeof(prelim_drm_xe_eudebug_event_client));
static_assert(offsetof(EuDebugEventClient, base) == offsetof(prelim_drm_xe_eudebug_event_client, base));
static_assert(offsetof(EuDebugEventClient, clientHandle) == offsetof(prelim_drm_xe_eudebug_event_client, client_handle));

static_assert(sizeof(EuDebugEventVm) == sizeof(prelim_drm_xe_eudebug_event_vm));
static_assert(offsetof(EuDebugEventVm, base) == offsetof(prelim_drm_xe_eudebug_event_vm, base));
static_assert(offsetof(EuDebugEventVm, clientHandle) == offsetof(prelim_drm_xe_eudebug_event_vm, client_handle));
static_assert(offsetof(EuDebugEventVm, vmHandle) == offsetof(prelim_drm_xe_eudebug_event_vm, vm_handle));

static_assert(sizeof(EuDebugEventExecQueue) == sizeof(prelim_drm_xe_eudebug_event_exec_queue));
static_assert(offsetof(EuDebugEventExecQueue, base) == offsetof(prelim_drm_xe_eudebug_event_exec_queue, base));
static_assert(offsetof(EuDebugEventExecQueue, clientHandle) == offsetof(prelim_drm_xe_eudebug_event_exec_queue, client_handle));
static_assert(offsetof(EuDebugEventExecQueue, vmHandle) == offsetof(prelim_drm_xe_eudebug_event_exec_queue, vm_handle));
static_assert(offsetof(EuDebugEventExecQueue, execQueueHandle) == offsetof(prelim_drm_xe_eudebug_event_exec_queue, exec_queue_handle));
static_assert(offsetof(EuDebugEventExecQueue, engineClass) == offsetof(prelim_drm_xe_eudebug_event_exec_queue, engine_class));
static_assert(offsetof(EuDebugEventExecQueue, width) == offsetof(prelim_drm_xe_eudebug_event_exec_queue, width));
static_assert(offsetof(EuDebugEventExecQueue, lrcHandle) == offsetof(prelim_drm_xe_eudebug_event_exec_queue, lrc_handle));

static_assert(sizeof(EuDebugEventMetadata) == sizeof(prelim_drm_xe_eudebug_event_metadata));
static_assert(offsetof(EuDebugEventMetadata, base) == offsetof(prelim_drm_xe_eudebug_event_metadata, base));
static_assert(offsetof(EuDebugEventMetadata, clientHandle) == offsetof(prelim_drm_xe_eudebug_event_metadata, client_handle));
static_assert(offsetof(EuDebugEventMetadata, metadataHandle) == offsetof(prelim_drm_xe_eudebug_event_metadata, metadata_handle));
static_assert(offsetof(EuDebugEventMetadata, type) == offsetof(prelim_drm_xe_eudebug_event_metadata, type));
static_assert(offsetof(EuDebugEventMetadata, len) == offsetof(prelim_drm_xe_eudebug_event_metadata, len));

static_assert(sizeof(EuDebugEventVmBind) == sizeof(prelim_drm_xe_eudebug_event_vm_bind));
static_assert(offsetof(EuDebugEventVmBind, base) == offsetof(prelim_drm_xe_eudebug_event_vm_bind, base));
static_assert(offsetof(EuDebugEventVmBind, clientHandle) == offsetof(prelim_drm_xe_eudebug_event_vm_bind, client_handle));
static_assert(offsetof(EuDebugEventVmBind, vmHandle) == offsetof(prelim_drm_xe_eudebug_event_vm_bind, vm_handle));
static_assert(offsetof(EuDebugEventVmBind, flags) == offsetof(prelim_drm_xe_eudebug_event_vm_bind, flags));
static_assert(offsetof(EuDebugEventVmBind, numBinds) == offsetof(prelim_drm_xe_eudebug_event_vm_bind, num_binds));

static_assert(sizeof(EuDebugEventVmBindOp) == sizeof(prelim_drm_xe_eudebug_event_vm_bind_op));
static_assert(offsetof(EuDebugEventVmBindOp, base) == offsetof(prelim_drm_xe_eudebug_event_vm_bind_op, base));
static_assert(offsetof(EuDebugEventVmBindOp, vmBindRefSeqno) == offsetof(prelim_drm_xe_eudebug_event_vm_bind_op, vm_bind_ref_seqno));
static_assert(offsetof(EuDebugEventVmBindOp, numExtensions) == offsetof(prelim_drm_xe_eudebug_event_vm_bind_op, num_extensions));
static_assert(offsetof(EuDebugEventVmBindOp, addr) == offsetof(prelim_drm_xe_eudebug_event_vm_bind_op, addr));
static_assert(offsetof(EuDebugEventVmBindOp, range) == offsetof(prelim_drm_xe_eudebug_event_vm_bind_op, range));

static_assert(sizeof(EuDebugEventVmBindOpMetadata) == sizeof(prelim_drm_xe_eudebug_event_vm_bind_op_metadata));
static_assert(offsetof(EuDebugEventVmBindOpMetadata, base) == offsetof(prelim_drm_xe_eudebug_event_vm_bind_op_metadata, base));
static_assert(offsetof(EuDebugEventVmBindOpMetadata, vmBindOpRefSeqno) == offsetof(prelim_drm_xe_eudebug_event_vm_bind_op_metadata, vm_bind_op_ref_seqno));
static_assert(offsetof(EuDebugEventVmBindOpMetadata, metadataHandle) == offsetof(prelim_drm_xe_eudebug_event_vm_bind_op_metadata, metadata_handle));
static_assert(offsetof(EuDebugEventVmBindOpMetadata, metadataCookie) == offsetof(prelim_drm_xe_eudebug_event_vm_bind_op_metadata, metadata_cookie));

static_assert(sizeof(EuDebugEventVmBindUfence) == sizeof(prelim_drm_xe_eudebug_event_vm_bind_ufence));
static_assert(offsetof(EuDebugEventVmBindUfence, base) == offsetof(prelim_drm_xe_eudebug_event_vm_bind_ufence, base));
static_assert(offsetof(EuDebugEventVmBindUfence, vmBindRefSeqno) == offsetof(prelim_drm_xe_eudebug_event_vm_bind_ufence, vm_bind_ref_seqno));

static_assert(sizeof(EuDebugEventPageFault) == sizeof(prelim_drm_xe_eudebug_event_pagefault));
static_assert(offsetof(EuDebugEventPageFault, clientHandle) == offsetof(prelim_drm_xe_eudebug_event_pagefault, client_handle));
static_assert(offsetof(EuDebugEventPageFault, execQueueHandle) == offsetof(prelim_drm_xe_eudebug_event_pagefault, exec_queue_handle));
static_assert(offsetof(EuDebugEventPageFault, lrcHandle) == offsetof(prelim_drm_xe_eudebug_event_pagefault, lrc_handle));
static_assert(offsetof(EuDebugEventPageFault, flags) == offsetof(prelim_drm_xe_eudebug_event_pagefault, flags));
static_assert(offsetof(EuDebugEventPageFault, bitmaskSize) == offsetof(prelim_drm_xe_eudebug_event_pagefault, bitmask_size));
static_assert(offsetof(EuDebugEventPageFault, pagefaultAddress) == offsetof(prelim_drm_xe_eudebug_event_pagefault, pagefault_address));
static_assert(offsetof(EuDebugEventPageFault, bitmask) == offsetof(prelim_drm_xe_eudebug_event_pagefault, bitmask));

static_assert(sizeof(EuDebugReadMetadata) == sizeof(prelim_drm_xe_eudebug_read_metadata));
static_assert(offsetof(EuDebugReadMetadata, clientHandle) == offsetof(prelim_drm_xe_eudebug_read_metadata, client_handle));
static_assert(offsetof(EuDebugReadMetadata, metadataHandle) == offsetof(prelim_drm_xe_eudebug_read_metadata, metadata_handle));
static_assert(offsetof(EuDebugReadMetadata, flags) == offsetof(prelim_drm_xe_eudebug_read_metadata, flags));
static_assert(offsetof(EuDebugReadMetadata, reserved) == offsetof(prelim_drm_xe_eudebug_read_metadata, reserved));
static_assert(offsetof(EuDebugReadMetadata, ptr) == offsetof(prelim_drm_xe_eudebug_read_metadata, ptr));
static_assert(offsetof(EuDebugReadMetadata, size) == offsetof(prelim_drm_xe_eudebug_read_metadata, size));

static_assert(sizeof(EuDebugEuControl) == sizeof(prelim_drm_xe_eudebug_eu_control));
static_assert(offsetof(EuDebugEuControl, clientHandle) == offsetof(prelim_drm_xe_eudebug_eu_control, client_handle));
static_assert(offsetof(EuDebugEuControl, cmd) == offsetof(prelim_drm_xe_eudebug_eu_control, cmd));
static_assert(offsetof(EuDebugEuControl, flags) == offsetof(prelim_drm_xe_eudebug_eu_control, flags));
static_assert(offsetof(EuDebugEuControl, seqno) == offsetof(prelim_drm_xe_eudebug_eu_control, seqno));
static_assert(offsetof(EuDebugEuControl, execQueueHandle) == offsetof(prelim_drm_xe_eudebug_eu_control, exec_queue_handle));
static_assert(offsetof(EuDebugEuControl, lrcHandle) == offsetof(prelim_drm_xe_eudebug_eu_control, lrc_handle));
static_assert(offsetof(EuDebugEuControl, reserved) == offsetof(prelim_drm_xe_eudebug_eu_control, reserved));
static_assert(offsetof(EuDebugEuControl, bitmaskSize) == offsetof(prelim_drm_xe_eudebug_eu_control, bitmask_size));
static_assert(offsetof(EuDebugEuControl, bitmaskPtr) == offsetof(prelim_drm_xe_eudebug_eu_control, bitmask_ptr));

static_assert(sizeof(EuDebugVmOpen) == sizeof(prelim_drm_xe_eudebug_vm_open));
static_assert(offsetof(EuDebugVmOpen, extensions) == offsetof(prelim_drm_xe_eudebug_vm_open, extensions));
static_assert(offsetof(EuDebugVmOpen, clientHandle) == offsetof(prelim_drm_xe_eudebug_vm_open, client_handle));
static_assert(offsetof(EuDebugVmOpen, vmHandle) == offsetof(prelim_drm_xe_eudebug_vm_open, vm_handle));
static_assert(offsetof(EuDebugVmOpen, flags) == offsetof(prelim_drm_xe_eudebug_vm_open, flags));
static_assert(offsetof(EuDebugVmOpen, timeoutNs) == offsetof(prelim_drm_xe_eudebug_vm_open, timeout_ns));

static_assert(sizeof(EuDebugAckEvent) == sizeof(prelim_drm_xe_eudebug_ack_event));
static_assert(offsetof(EuDebugAckEvent, type) == offsetof(prelim_drm_xe_eudebug_ack_event, type));
static_assert(offsetof(EuDebugAckEvent, flags) == offsetof(prelim_drm_xe_eudebug_ack_event, flags));
static_assert(offsetof(EuDebugAckEvent, seqno) == offsetof(prelim_drm_xe_eudebug_ack_event, seqno));

static_assert(sizeof(DebugMetadataCreate) == sizeof(prelim_drm_xe_debug_metadata_create));
static_assert(offsetof(DebugMetadataCreate, extensions) == offsetof(prelim_drm_xe_debug_metadata_create, extensions));
static_assert(offsetof(DebugMetadataCreate, type) == offsetof(prelim_drm_xe_debug_metadata_create, type));
static_assert(offsetof(DebugMetadataCreate, userAddr) == offsetof(prelim_drm_xe_debug_metadata_create, user_addr));
static_assert(offsetof(DebugMetadataCreate, len) == offsetof(prelim_drm_xe_debug_metadata_create, len));
static_assert(offsetof(DebugMetadataCreate, metadataId) == offsetof(prelim_drm_xe_debug_metadata_create, metadata_id));

static_assert(sizeof(DebugMetadataDestroy) == sizeof(prelim_drm_xe_debug_metadata_destroy));
static_assert(offsetof(DebugMetadataDestroy, extensions) == offsetof(prelim_drm_xe_debug_metadata_destroy, extensions));
static_assert(offsetof(DebugMetadataDestroy, metadataId) == offsetof(prelim_drm_xe_debug_metadata_destroy, metadata_id));

static_assert(sizeof(XeUserExtension) == sizeof(drm_xe_user_extension));
static_assert(offsetof(XeUserExtension, nextExtension) == offsetof(drm_xe_user_extension, next_extension));
static_assert(offsetof(XeUserExtension, name) == offsetof(drm_xe_user_extension, name));
static_assert(offsetof(XeUserExtension, pad) == offsetof(drm_xe_user_extension, pad));

static_assert(sizeof(VmBindOpExtAttachDebug) == sizeof(prelim_drm_xe_vm_bind_op_ext_attach_debug));
static_assert(offsetof(VmBindOpExtAttachDebug, base) == offsetof(prelim_drm_xe_vm_bind_op_ext_attach_debug, base));
static_assert(offsetof(VmBindOpExtAttachDebug, metadataId) == offsetof(prelim_drm_xe_vm_bind_op_ext_attach_debug, metadata_id));
static_assert(offsetof(VmBindOpExtAttachDebug, flags) == offsetof(prelim_drm_xe_vm_bind_op_ext_attach_debug, flags));
static_assert(offsetof(VmBindOpExtAttachDebug, cookie) == offsetof(prelim_drm_xe_vm_bind_op_ext_attach_debug, cookie));
static_assert(offsetof(VmBindOpExtAttachDebug, reserved) == offsetof(prelim_drm_xe_vm_bind_op_ext_attach_debug, reserved));
} // namespace NEO
