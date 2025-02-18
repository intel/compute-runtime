/* SPDX-License-Identifier: MIT */
/*
 * Copyright © 2023 Intel Corporation
 */

 #ifndef _XE_DRM_EUDEBUG_H_
 #define _XE_DRM_EUDEBUG_H_
 
 #if defined(__cplusplus)
 extern "C" {
 #endif
 
 /**
  * Do a eudebug event read for a debugger connection.
  *
  * This ioctl is available in debug version 1.
  */
 #define DRM_XE_EUDEBUG_IOCTL_READ_EVENT		_IO('j', 0x0)
 #define DRM_XE_EUDEBUG_IOCTL_EU_CONTROL		_IOWR('j', 0x2, struct drm_xe_eudebug_eu_control)
 #define DRM_XE_EUDEBUG_IOCTL_ACK_EVENT		_IOW('j', 0x4, struct drm_xe_eudebug_ack_event)
 #define DRM_XE_EUDEBUG_IOCTL_VM_OPEN		_IOW('j', 0x1, struct drm_xe_eudebug_vm_open)
 #define DRM_XE_EUDEBUG_IOCTL_READ_METADATA	_IOWR('j', 0x3, struct drm_xe_eudebug_read_metadata)
 
 /* XXX: Document events to match their internal counterparts when moved to xe_drm.h */
 struct drm_xe_eudebug_event {
	 __u32 len;
 
	 __u16 type;
 #define DRM_XE_EUDEBUG_EVENT_NONE		0
 #define DRM_XE_EUDEBUG_EVENT_READ		1
 #define DRM_XE_EUDEBUG_EVENT_OPEN		2
 #define DRM_XE_EUDEBUG_EVENT_VM			3
 #define DRM_XE_EUDEBUG_EVENT_EXEC_QUEUE		4
 #define DRM_XE_EUDEBUG_EVENT_EU_ATTENTION	5
 #define DRM_XE_EUDEBUG_EVENT_VM_BIND		6
 #define DRM_XE_EUDEBUG_EVENT_VM_BIND_OP		7
 #define DRM_XE_EUDEBUG_EVENT_VM_BIND_UFENCE	8
 #define DRM_XE_EUDEBUG_EVENT_METADATA		9
 #define DRM_XE_EUDEBUG_EVENT_VM_BIND_OP_METADATA 10
 #define DRM_XE_EUDEBUG_EVENT_PAGEFAULT		11
 
	 __u16 flags;
 #define DRM_XE_EUDEBUG_EVENT_CREATE		(1 << 0)
 #define DRM_XE_EUDEBUG_EVENT_DESTROY		(1 << 1)
 #define DRM_XE_EUDEBUG_EVENT_STATE_CHANGE	(1 << 2)
 #define DRM_XE_EUDEBUG_EVENT_NEED_ACK		(1 << 3)
 
	 __u64 seqno;
	 __u64 reserved;
 };
 
 struct drm_xe_eudebug_event_client {
	 struct drm_xe_eudebug_event base;
 
	 __u64 client_handle; /* This is unique per debug connection */
 };
 
 struct drm_xe_eudebug_event_vm {
	 struct drm_xe_eudebug_event base;
 
	 __u64 client_handle;
	 __u64 vm_handle;
 };
 
 struct drm_xe_eudebug_event_exec_queue {
	 struct drm_xe_eudebug_event base;
 
	 __u64 client_handle;
	 __u64 vm_handle;
	 __u64 exec_queue_handle;
	 __u32 engine_class;
	 __u32 width;
	 __u64 lrc_handle[];
 };
 
 struct drm_xe_eudebug_event_eu_attention {
	 struct drm_xe_eudebug_event base;
 
	 __u64 client_handle;
	 __u64 exec_queue_handle;
	 __u64 lrc_handle;
	 __u32 flags;
	 __u32 bitmask_size;
	 __u8 bitmask[];
 };
 
 struct drm_xe_eudebug_eu_control {
	 __u64 client_handle;
 
 #define DRM_XE_EUDEBUG_EU_CONTROL_CMD_INTERRUPT_ALL	0
 #define DRM_XE_EUDEBUG_EU_CONTROL_CMD_STOPPED		1
 #define DRM_XE_EUDEBUG_EU_CONTROL_CMD_RESUME		2
	 __u32 cmd;
	 __u32 flags;
 
	 __u64 seqno;
 
	 __u64 exec_queue_handle;
	 __u64 lrc_handle;
	 __u32 reserved;
	 __u32 bitmask_size;
	 __u64 bitmask_ptr;
 };
 
 /*
  *  When client (debuggee) does vm_bind_ioctl() following event
  *  sequence will be created (for the debugger):
  *
  *  ┌───────────────────────┐
  *  │  EVENT_VM_BIND        ├───────┬─┬─┐
  *  └───────────────────────┘       │ │ │
  *      ┌───────────────────────┐   │ │ │
  *      │ EVENT_VM_BIND_OP #1   ├───┘ │ │
  *      └───────────────────────┘     │ │
  *                 ...                │ │
  *      ┌───────────────────────┐     │ │
  *      │ EVENT_VM_BIND_OP #n   ├─────┘ │
  *      └───────────────────────┘       │
  *                                      │
  *      ┌───────────────────────┐       │
  *      │ EVENT_UFENCE          ├───────┘
  *      └───────────────────────┘
  *
  * All the events below VM_BIND will reference the VM_BIND
  * they associate with, by field .vm_bind_ref_seqno.
  * event_ufence will only be included if the client did
  * attach sync of type UFENCE into its vm_bind_ioctl().
  *
  * When EVENT_UFENCE is sent by the driver, all the OPs of
  * the original VM_BIND are completed and the [addr,range]
  * contained in them are present and modifiable through the
  * vm accessors. Accessing [addr, range] before related ufence
  * event will lead to undefined results as the actual bind
  * operations are async and the backing storage might not
  * be there on a moment of receiving the event.
  *
  * Client's UFENCE sync will be held by the driver: client's
  * drm_xe_wait_ufence will not complete and the value of the ufence
  * won't appear until ufence is acked by the debugger process calling
  * DRM_XE_EUDEBUG_IOCTL_ACK_EVENT with the event_ufence.base.seqno.
  * This will signal the fence, .value will update and the wait will
  * complete allowing the client to continue.
  *
  */
 
 struct drm_xe_eudebug_event_vm_bind {
	 struct drm_xe_eudebug_event base;
 
	 __u64 client_handle;
	 __u64 vm_handle;
 
	 __u32 flags;
 #define DRM_XE_EUDEBUG_EVENT_VM_BIND_FLAG_UFENCE (1 << 0)
 
	 __u32 num_binds;
 };
 
 struct drm_xe_eudebug_event_vm_bind_op {
	 struct drm_xe_eudebug_event base;
	 __u64 vm_bind_ref_seqno; /* *_event_vm_bind.base.seqno */
	 __u64 num_extensions;
 
	 __u64 addr; /* XXX: Zero for unmap all? */
	 __u64 range; /* XXX: Zero for unmap all? */
 };
 
 struct drm_xe_eudebug_event_vm_bind_ufence {
	 struct drm_xe_eudebug_event base;
	 __u64 vm_bind_ref_seqno; /* *_event_vm_bind.base.seqno */
 };
 
 struct drm_xe_eudebug_ack_event {
	 __u32 type;
	 __u32 flags; /* MBZ */
	 __u64 seqno;
 };
 
 struct drm_xe_eudebug_vm_open {
	 /** @extensions: Pointer to the first extension struct, if any */
	 __u64 extensions;
 
	 /** @client_handle: id of client */
	 __u64 client_handle;
 
	 /** @vm_handle: id of vm */
	 __u64 vm_handle;
 
	 /** @flags: flags */
	 __u64 flags;
 
 #define DRM_XE_EUDEBUG_VM_SYNC_MAX_TIMEOUT_NSECS (10ULL * NSEC_PER_SEC)
	 /** @timeout_ns: Timeout value in nanoseconds operations (fsync) */
	 __u64 timeout_ns;
 };
 
 struct drm_xe_eudebug_read_metadata {
	 __u64 client_handle;
	 __u64 metadata_handle;
	 __u32 flags;
	 __u32 reserved;
	 __u64 ptr;
	 __u64 size;
 };
 
 struct drm_xe_eudebug_event_metadata {
	 struct drm_xe_eudebug_event base;
 
	 __u64 client_handle;
	 __u64 metadata_handle;
	 /* XXX: Refer to xe_drm.h for fields */
	 __u64 type;
	 __u64 len;
 };
 
 struct drm_xe_eudebug_event_vm_bind_op_metadata {
	 struct drm_xe_eudebug_event base;
	 __u64 vm_bind_op_ref_seqno; /* *_event_vm_bind_op.base.seqno */
 
	 __u64 metadata_handle;
	 __u64 metadata_cookie;
 };
 
 struct drm_xe_eudebug_event_pagefault {
	 struct drm_xe_eudebug_event base;
 
	 __u64 client_handle;
	 __u64 exec_queue_handle;
	 __u64 lrc_handle;
	 __u32 flags;
	 __u32 bitmask_size;
	 __u64 pagefault_address;
	 __u8 bitmask[];
 };
 
 #if defined(__cplusplus)
 }
 #endif
 
 #endif /* _XE_DRM_EUDEBUG_H_ */
 