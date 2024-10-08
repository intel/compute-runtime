/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/os_interface/linux/xe/xedrm_prelim.h"

constexpr const char *sysFsXeEuDebugFile = "/device/prelim_enable_eudebug";

#define DRM_IOCTL_XE_DEBUG_METADATA_DESTROY PRELIM_DRM_IOCTL_XE_DEBUG_METADATA_DESTROY
#define DRM_IOCTL_XE_DEBUG_METADATA_CREATE PRELIM_DRM_IOCTL_XE_DEBUG_METADATA_CREATE
#define DRM_IOCTL_XE_EUDEBUG_CONNECT PRELIM_DRM_IOCTL_XE_EUDEBUG_CONNECT

using drm_xe_vm_bind_op_ext_attach_debug = prelim_drm_xe_vm_bind_op_ext_attach_debug;

#define XE_VM_BIND_OP_EXTENSIONS_ATTACH_DEBUG PRELIM_XE_VM_BIND_OP_EXTENSIONS_ATTACH_DEBUG

#define DRM_XE_EXEC_QUEUE_SET_PROPERTY_EUDEBUG PRELIM_DRM_XE_EXEC_QUEUE_SET_PROPERTY_EUDEBUG
#define DRM_XE_EXEC_QUEUE_EUDEBUG_FLAG_ENABLE PRELIM_DRM_XE_EXEC_QUEUE_EUDEBUG_FLAG_ENABLE
#define DRM_XE_EXEC_QUEUE_EUDEBUG_FLAG_PAGEFAULT_ENABLE PRELIM_DRM_XE_EXEC_QUEUE_EUDEBUG_FLAG_PAGEFAULT_ENABLE

using drm_xe_debug_metadata_create = prelim_drm_xe_debug_metadata_create;

#define DRM_XE_DEBUG_METADATA_ELF_BINARY PRELIM_DRM_XE_DEBUG_METADATA_ELF_BINARY
#define DRM_XE_DEBUG_METADATA_PROGRAM_MODULE PRELIM_DRM_XE_DEBUG_METADATA_PROGRAM_MODULE
#define WORK_IN_PROGRESS_DRM_XE_DEBUG_METADATA_MODULE_AREA PRELIM_WORK_IN_PROGRESS_DRM_XE_DEBUG_METADATA_MODULE_AREA
#define WORK_IN_PROGRESS_DRM_XE_DEBUG_METADATA_SBA_AREA PRELIM_WORK_IN_PROGRESS_DRM_XE_DEBUG_METADATA_SBA_AREA
#define WORK_IN_PROGRESS_DRM_XE_DEBUG_METADATA_SIP_AREA PRELIM_WORK_IN_PROGRESS_DRM_XE_DEBUG_METADATA_SIP_AREA
#define WORK_IN_PROGRESS_DRM_XE_DEBUG_METADATA_NUM PRELIM_WORK_IN_PROGRESS_DRM_XE_DEBUG_METADATA_NUM

using drm_xe_debug_metadata_destroy = prelim_drm_xe_debug_metadata_destroy;

#define DRM_XE_EUDEBUG_IOCTL_READ_EVENT PRELIM_DRM_XE_EUDEBUG_IOCTL_READ_EVENT
#define DRM_XE_EUDEBUG_IOCTL_EU_CONTROL PRELIM_DRM_XE_EUDEBUG_IOCTL_EU_CONTROL
#define DRM_XE_EUDEBUG_IOCTL_VM_OPEN PRELIM_DRM_XE_EUDEBUG_IOCTL_VM_OPEN
#define DRM_XE_EUDEBUG_IOCTL_READ_METADATA PRELIM_DRM_XE_EUDEBUG_IOCTL_READ_METADATA
#define DRM_XE_EUDEBUG_IOCTL_ACK_EVENT PRELIM_DRM_XE_EUDEBUG_IOCTL_ACK_EVENT

using drm_xe_eudebug_event = prelim_drm_xe_eudebug_event;

#define DRM_XE_EUDEBUG_EVENT_NONE PRELIM_DRM_XE_EUDEBUG_EVENT_NONE
#define DRM_XE_EUDEBUG_EVENT_READ PRELIM_DRM_XE_EUDEBUG_EVENT_READ
#define DRM_XE_EUDEBUG_EVENT_OPEN PRELIM_DRM_XE_EUDEBUG_EVENT_OPEN
#define DRM_XE_EUDEBUG_EVENT_VM PRELIM_DRM_XE_EUDEBUG_EVENT_VM
#define DRM_XE_EUDEBUG_EVENT_EXEC_QUEUE PRELIM_DRM_XE_EUDEBUG_EVENT_EXEC_QUEUE
#define DRM_XE_EUDEBUG_EVENT_EU_ATTENTION PRELIM_DRM_XE_EUDEBUG_EVENT_EU_ATTENTION
#define DRM_XE_EUDEBUG_EVENT_VM_BIND PRELIM_DRM_XE_EUDEBUG_EVENT_VM_BIND
#define DRM_XE_EUDEBUG_EVENT_VM_BIND_OP PRELIM_DRM_XE_EUDEBUG_EVENT_VM_BIND_OP
#define DRM_XE_EUDEBUG_EVENT_VM_BIND_UFENCE PRELIM_DRM_XE_EUDEBUG_EVENT_VM_BIND_UFENCE
#define DRM_XE_EUDEBUG_EVENT_METADATA PRELIM_DRM_XE_EUDEBUG_EVENT_METADATA
#define DRM_XE_EUDEBUG_EVENT_VM_BIND_OP_METADATA PRELIM_DRM_XE_EUDEBUG_EVENT_VM_BIND_OP_METADATA
#define DRM_XE_EUDEBUG_EVENT_VM_SET_METADATA PRELIM_DRM_XE_EUDEBUG_EVENT_VM_SET_METADATA
#define DRM_XE_EUDEBUG_EVENT_PAGEFAULT PRELIM_DRM_XE_EUDEBUG_EVENT_PAGEFAULT

#define DRM_XE_EUDEBUG_EVENT_CREATE PRELIM_DRM_XE_EUDEBUG_EVENT_CREATE
#define DRM_XE_EUDEBUG_EVENT_DESTROY PRELIM_DRM_XE_EUDEBUG_EVENT_DESTROY
#define DRM_XE_EUDEBUG_EVENT_STATE_CHANGE PRELIM_DRM_XE_EUDEBUG_EVENT_STATE_CHANGE
#define DRM_XE_EUDEBUG_EVENT_NEED_ACK PRELIM_DRM_XE_EUDEBUG_EVENT_NEED_ACK

using drm_xe_eudebug_event_client = prelim_drm_xe_eudebug_event_client;
using drm_xe_eudebug_event_vm = prelim_drm_xe_eudebug_event_vm;
using drm_xe_eudebug_event_exec_queue = prelim_drm_xe_eudebug_event_exec_queue;
using drm_xe_eudebug_event_eu_attention = prelim_drm_xe_eudebug_event_eu_attention;
using drm_xe_eudebug_event_vm_bind = prelim_drm_xe_eudebug_event_vm_bind;
#define DRM_XE_EUDEBUG_EVENT_VM_BIND_FLAG_UFENCE PRELIM_DRM_XE_EUDEBUG_EVENT_VM_BIND_FLAG_UFENCE

using drm_xe_eudebug_event_vm_bind_op = prelim_drm_xe_eudebug_event_vm_bind_op;
using drm_xe_eudebug_event_vm_bind_ufence = prelim_drm_xe_eudebug_event_vm_bind_ufence;
using drm_xe_eudebug_event_metadata = prelim_drm_xe_eudebug_event_metadata;
using drm_xe_eudebug_event_vm_bind_op_metadata = prelim_drm_xe_eudebug_event_vm_bind_op_metadata;

#define DRM_XE_EUDEBUG_VERSION PRELIM_DRM_XE_EUDEBUG_VERSION

using drm_xe_eudebug_connect = prelim_drm_xe_eudebug_connect;
using drm_xe_eudebug_eu_control = prelim_drm_xe_eudebug_eu_control;

#define DRM_XE_EUDEBUG_EU_CONTROL_CMD_INTERRUPT_ALL PRELIM_DRM_XE_EUDEBUG_EU_CONTROL_CMD_INTERRUPT_ALL
#define DRM_XE_EUDEBUG_EU_CONTROL_CMD_STOPPED PRELIM_DRM_XE_EUDEBUG_EU_CONTROL_CMD_STOPPED
#define DRM_XE_EUDEBUG_EU_CONTROL_CMD_RESUME PRELIM_DRM_XE_EUDEBUG_EU_CONTROL_CMD_RESUME
#define DRM_XE_EUDEBUG_EU_CONTROL_CMD_UNLOCK PRELIM_DRM_XE_EUDEBUG_EU_CONTROL_CMD_UNLOCK

using drm_xe_eudebug_vm_open = prelim_drm_xe_eudebug_vm_open;
using drm_xe_eudebug_read_metadata = prelim_drm_xe_eudebug_read_metadata;

using drm_xe_eudebug_ack_event = prelim_drm_xe_eudebug_ack_event;
