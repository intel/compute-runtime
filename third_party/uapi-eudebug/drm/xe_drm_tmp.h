/* SPDX-License-Identifier: MIT */
/*
 * Copyright © 2023 Intel Corporation
 */
#ifndef _UAPI_XE_DRM_TMP_H_
#define _UAPI_XE_DRM_TMP_H_

#include "xe_drm.h"

#if defined(__cplusplus)
extern "C" {
#endif

#define DRM_XE_EUDEBUG_CONNECT			0x5f

#define DRM_IOCTL_XE_EUDEBUG_CONNECT		DRM_IOWR(DRM_COMMAND_BASE + DRM_XE_EUDEBUG_CONNECT, struct drm_xe_eudebug_connect)

/**
 * Do a eudebug event read for a debugger connection.
 *
 * This ioctl is available in debug version 1.
 */
#define DRM_XE_EUDEBUG_IOCTL_READ_EVENT		_IO('j', 0x0)
#define DRM_XE_EUDEBUG_IOCTL_EU_CONTROL		_IOWR('j', 0x2, struct drm_xe_eudebug_eu_control)
#define DRM_XE_EUDEBUG_IOCTL_VM_OPEN		_IOW('j', 0x1, struct drm_xe_eudebug_vm_open)
#define DRM_XE_EUDEBUG_IOCTL_READ_METADATA	_IOWR('j', 0x3, struct drm_xe_eudebug_read_metadata)
#define DRM_XE_EUDEBUG_IOCTL_ACK_EVENT		_IOW('j', 0x4, struct drm_xe_eudebug_event_ack)

/* XXX: Document events to match their internal counterparts when moved to xe_drm.h */
struct drm_xe_eudebug_event {
	__u32 len;

	__u16 type;
#define DRM_XE_EUDEBUG_EVENT_NONE 0
#define DRM_XE_EUDEBUG_EVENT_READ 1
#define DRM_XE_EUDEBUG_EVENT_OPEN 2
#define DRM_XE_EUDEBUG_EVENT_VM 3
#define DRM_XE_EUDEBUG_EVENT_EXEC_QUEUE 4
#define DRM_XE_EUDEBUG_EVENT_EU_ATTENTION 5
#define DRM_XE_EUDEBUG_EVENT_VM_BIND 6
#define DRM_XE_EUDEBUG_EVENT_METADATA 7
#define DRM_XE_EUDEBUG_EVENT_VM_SET_METADATA 8
#define DRM_XE_EUDEBUG_EVENT_MAX_EVENT DRM_XE_EUDEBUG_EVENT_VM_SET_METADATA

	__u16 flags;
#define DRM_XE_EUDEBUG_EVENT_CREATE		(1 << 0)
#define DRM_XE_EUDEBUG_EVENT_DESTROY		(1 << 1)
#define DRM_XE_EUDEBUG_EVENT_STATE_CHANGE	(1 << 2)
#define DRM_XE_EUDEBUG_EVENT_NEED_ACK		(1 << 3)

	__u64 seqno;
	__u64 reserved;
} __attribute__((packed));

struct drm_xe_eudebug_event_client {
	struct drm_xe_eudebug_event base;

	__u64 client_handle; /* This is unique per debug connection */
} __attribute__((packed));

struct drm_xe_eudebug_event_vm {
	struct drm_xe_eudebug_event base;

	__u64 client_handle;
	__u64 vm_handle;
} __attribute__((packed));

struct drm_xe_eudebug_event_exec_queue {
	struct drm_xe_eudebug_event base;

	__u64 client_handle;
	__u64 vm_handle;
	__u64 exec_queue_handle;
	__u16 engine_class;
	__u16 width;
	__u64 lrc_handle[0];
} __attribute__((packed));

struct drm_xe_eudebug_event_eu_attention {
	struct drm_xe_eudebug_event base;
	__u64 client_handle;
	__u64 exec_queue_handle;
	__u64 lrc_handle;
	__u32 flags;
	__u32 bitmask_size;
	__u8 bitmask[0];
} __attribute__((packed));

struct drm_xe_eudebug_event_vm_bind {
	struct drm_xe_eudebug_event base;
	__u64 client_handle;

	__u64 vm_handle;
	__u64 va_start;
	__u64 va_length;

	__u64 metadata_handle;
	__u64 metadata_cookie;
} __attribute__((packed));

struct drm_xe_eudebug_event_metadata {
	struct drm_xe_eudebug_event base;

	__u64 client_handle;
	__u64 metadata_handle;
	/* XXX: Refer to xe_drm.h for fields */
	__u64 type;
	__u64 len;
} __attribute__((packed));

struct drm_xe_eudebug_event_vm_set_metadata {
	struct drm_xe_eudebug_event base;

	__u64 client_handle;
	__u64 vm_handle;

	/* XXX: Refer to xe_drm.h for fields */
	__u64 type;
	union {
		__u64 cookie;
		__u64 offset;
	};
	__u64 len;
} __attribute__((packed));

/*
 * Debugger ABI (ioctl and events) Version History:
 * 0 - No debugger available
 * 1 - Initial version
 */
#define DRM_XE_EUDEBUG_VERSION 1

struct drm_xe_eudebug_connect {
	/** @extensions: Pointer to the first extension struct, if any */
	__u64 extensions;

	__u64 pid; /* input: Target process ID */
	__u32 flags; /* MBZ */

	__u32 version; /* output: current ABI (ioctl / events) version */
};

struct drm_xe_eudebug_eu_control {
	__u64 client_handle;
	__u32 cmd;
#define DRM_XE_EUDEBUG_EU_CONTROL_CMD_INTERRUPT_ALL	0
#define DRM_XE_EUDEBUG_EU_CONTROL_CMD_STOPPED		1
#define DRM_XE_EUDEBUG_EU_CONTROL_CMD_RESUME		2
	__u32 flags;
	__u64 seqno;

	__u64 exec_queue_handle;
	__u64 lrc_handle;
	__u32 bitmask_size;
	__u64 bitmask_ptr;
} __attribute__((packed));

struct drm_xe_eudebug_vm_open {
	/** @extensions: Pointer to the first extension struct, if any */
	__u64 extensions;

	/** @client_handle: id of client */
	__u64 client_handle;

	/** @vm_handle: id of vm */
	__u64 vm_handle;

	/** @flags: flags */
	__u64 flags;

	/** @timeout_ns: Timeout value in nanoseconds operations (fsync) */
	__u64 timeout_ns;
};

struct drm_xe_eudebug_read_metadata {
	__u64 client_handle;
	__u64 metadata_handle;
	__u32 flags;

	__u64 ptr;
	__u64 size;
};

struct drm_xe_eudebug_event_ack {
	__u32 type;
	__u32 flags; /* MBZ */
	__u64 seqno;
};

#if defined(__cplusplus)
}
#endif

#endif /* _UAPI_XE_DRM_TMP_H_ */
