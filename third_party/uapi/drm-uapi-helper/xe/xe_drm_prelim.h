/* SPDX-License-Identifier: MIT */
/*
 * Copyright © 2023 Intel Corporation
 */

#ifndef _XE_DRM_PRELIM_H_
#define _XE_DRM_PRELIM_H_

#include "xe_drm.h"

/**
 * DOC: Xe PRELIM uAPI
 *
 * Reasoning:
 *
 * The DRM community imposes some strict requirements on the uAPI:
 *
 * - https://www.kernel.org/doc/html/latest/gpu/drm-uapi.html#open-source-userspace-requirements
 * - "The open-source userspace must not be a toy/test application, but the real thing."
 * - "The userspace patches must be against the canonical upstream, not some vendor fork."
 *
 * In some very specific cases, there will be a particular need to get a preliminary and
 * non-upstream uAPI merged in some of our branches. When this happens, the uAPI cannot
 * take a risk of conflicting IOCTL ranges with other preliminary uAPI or with a possible
 * real user space uAPI that could win the race towards upstream.
 *
 * 'PRELIM' uAPIs are APIs that are not yet merged on upstream. They were designed to be
 * in a different range in a way that the divergence with the upstream and other development
 * branches can be controlled and the conflicts minimized.
 *
 * It is also a mechanism that prevents user space regressions since the prelim modification
 * is always in a two-phase approach, where upstream and prelim can coexist for a period of
 * time while the UMDs adjust to the changes.
 *
 * Rules:
 *
 * - Communication will happen on a specific Teams channel: KMD uAPI Changes
 * - APIs to be considered Preliminary / WIP / Temporary to what will eventually be in upstream.
 *   It's designed to allow kernel and userspace to work together and make sure it works with all
 *   components before committing to support it forever in upstream (we don't want to break Linux's
 *   rule about not breaking userspace).
 * - IOCTL in separate file (this one).
 * - IOCTL, flags, enums, or any number that might conflict should be in separated range
 *   (i.e end of range like IOCTL numbers).
 * - PRELIM prefix mandatory in any define, struct, or non-static functions in PRELIM source files
 *   called by other source files (i.e xe_perf_ioctl -> prelim_xe_perf_ioctl)
 * - Code .c/.h using PRELIM should also be placed in prelim/ sub-directory for easier rebase.
 * - Two-Phase removal:
 *
 *   + When API needs to be modified in non-backwards compatible ways, the current PRELIM API
 *     cannot be removed (yet).
 *   + Either because it must change the behavior or because the final upstream one has landed.
 *   + If a new PRELIM is needed, it needs to be added as a new PRELIM_V<n+1> without removing the
 *     PRELIM_V<n>.
 *   + The previous one can only be removed after all user space components confirmed they are not
 *     using.
 *
 * Other prelim considerations:
 *
 * - Out of tree Sysfs and debugfs need to stay behind a 'prelim' directory.
 * - Out of tree module-parameters need to be identified by a PRELIM prefix.
 *   (xe.prelim_my_awesome_param=not_default)
 * - Internal/downstream declarations must be added here, not to xe_drm.h.
 * - The values in xe_drm_prelim.h must also be kept synchronized with values in xe_drm.h.
 * - PRELIM ioctl's: IOCTL numbers go down from 0x5f
 */

/*
 * IOCTL numbers listed below are reserved, they are taken up by other
 * components. Please add an unreserved ioctl number here to reserve that
 * number.
 */
#define PRELIM_DRM_XE_EUDEBUG_CONNECT		0x5f
#define PRELIM_DRM_XE_DEBUG_METADATA_CREATE	0x5e
#define PRELIM_DRM_XE_DEBUG_METADATA_DESTROY	0x5d
#define PRELIM_DRM_IOCTL_XE_EUDEBUG_CONNECT		DRM_IOWR(DRM_COMMAND_BASE + PRELIM_DRM_XE_EUDEBUG_CONNECT, struct prelim_drm_xe_eudebug_connect)
#define PRELIM_DRM_IOCTL_XE_DEBUG_METADATA_CREATE	 DRM_IOWR(DRM_COMMAND_BASE + PRELIM_DRM_XE_DEBUG_METADATA_CREATE, struct prelim_drm_xe_debug_metadata_create)
#define PRELIM_DRM_IOCTL_XE_DEBUG_METADATA_DESTROY	 DRM_IOW(DRM_COMMAND_BASE + PRELIM_DRM_XE_DEBUG_METADATA_DESTROY, struct prelim_drm_xe_debug_metadata_destroy)

struct prelim_drm_xe_vm_bind_op_ext_attach_debug {
	/** @base: base user extension */
	struct drm_xe_user_extension base;

	/** @id: Debug object id from create metadata */
	__u64 metadata_id;

	/** @flags: Flags */
	__u64 flags;

	/** @cookie: Cookie */
	__u64 cookie;

	/** @reserved: Reserved */
	__u64 reserved;
};

#define PRELIM_XE_VM_BIND_OP_EXTENSIONS_ATTACH_DEBUG 0
#define   PRELIM_DRM_XE_EXEC_QUEUE_SET_PROPERTY_EUDEBUG		2
#define     PRELIM_DRM_XE_EXEC_QUEUE_EUDEBUG_FLAG_ENABLE		(1 << 0)
/*
 * Debugger ABI (ioctl and events) Version History:
 * 0 - No debugger available
 * 1 - Initial version
 */
#define PRELIM_DRM_XE_EUDEBUG_VERSION 1

struct prelim_drm_xe_eudebug_connect {
	/** @extensions: Pointer to the first extension struct, if any */
	__u64 extensions;

	__u64 pid; /* input: Target process ID */
	__u32 flags; /* MBZ */

	__u32 version; /* output: current ABI (ioctl / events) version */
};

/*
 * struct prelim_drm_xe_debug_metadata_create - Create debug metadata
 *
 * Add a region of user memory to be marked as debug metadata.
 * When the debugger attaches, the metadata regions will be delivered
 * for debugger. Debugger can then map these regions to help decode
 * the program state.
 *
 * Returns handle to created metadata entry.
 */
struct prelim_drm_xe_debug_metadata_create {
	/** @extensions: Pointer to the first extension struct, if any */
	__u64 extensions;

#define PRELIM_DRM_XE_DEBUG_METADATA_ELF_BINARY     0
#define PRELIM_DRM_XE_DEBUG_METADATA_PROGRAM_MODULE 1
#define PRELIM_WORK_IN_PROGRESS_DRM_XE_DEBUG_METADATA_MODULE_AREA 2
#define PRELIM_WORK_IN_PROGRESS_DRM_XE_DEBUG_METADATA_SBA_AREA 3
#define PRELIM_WORK_IN_PROGRESS_DRM_XE_DEBUG_METADATA_SIP_AREA 4
#define PRELIM_WORK_IN_PROGRESS_DRM_XE_DEBUG_METADATA_NUM (1 + \
	  PRELIM_WORK_IN_PROGRESS_DRM_XE_DEBUG_METADATA_SIP_AREA)

	/** @type: Type of metadata */
	__u64 type;

	/** @user_addr: pointer to start of the metadata */
	__u64 user_addr;

	/** @len: length, in bytes of the medata */
	__u64 len;

	/** @metadata_id: created metadata handle (out) */
	__u32 metadata_id;
};

/**
 * struct prelim_drm_xe_debug_metadata_destroy - Destroy debug metadata
 *
 * Destroy debug metadata.
 */
struct prelim_drm_xe_debug_metadata_destroy {
	/** @extensions: Pointer to the first extension struct, if any */
	__u64 extensions;

	/** @metadata_id: metadata handle to destroy */
	__u32 metadata_id;
};

#include "xe_drm_prelim.h"

/**
 * Do a eudebug event read for a debugger connection.
 *
 * This ioctl is available in debug version 1.
 */
#define PRELIM_DRM_XE_EUDEBUG_IOCTL_READ_EVENT		_IO('j', 0x0)
#define PRELIM_DRM_XE_EUDEBUG_IOCTL_EU_CONTROL		_IOWR('j', 0x2, struct prelim_drm_xe_eudebug_eu_control)
#define PRELIM_DRM_XE_EUDEBUG_IOCTL_ACK_EVENT		_IOW('j', 0x4, struct prelim_drm_xe_eudebug_ack_event)
#define PRELIM_DRM_XE_EUDEBUG_IOCTL_VM_OPEN		_IOW('j', 0x1, struct prelim_drm_xe_eudebug_vm_open)
#define PRELIM_DRM_XE_EUDEBUG_IOCTL_READ_METADATA	_IOWR('j', 0x3, struct prelim_drm_xe_eudebug_read_metadata)

/* XXX: Document events to match their internal counterparts when moved to xe_drm.h */
struct prelim_drm_xe_eudebug_event {
	__u32 len;

	__u16 type;
#define PRELIM_DRM_XE_EUDEBUG_EVENT_NONE		0
#define PRELIM_DRM_XE_EUDEBUG_EVENT_READ		1
#define PRELIM_DRM_XE_EUDEBUG_EVENT_OPEN		2
#define PRELIM_DRM_XE_EUDEBUG_EVENT_VM			3
#define PRELIM_DRM_XE_EUDEBUG_EVENT_EXEC_QUEUE		4
#define PRELIM_DRM_XE_EUDEBUG_EVENT_EU_ATTENTION	5
#define PRELIM_DRM_XE_EUDEBUG_EVENT_VM_BIND		6
#define PRELIM_DRM_XE_EUDEBUG_EVENT_VM_BIND_OP		7
#define PRELIM_DRM_XE_EUDEBUG_EVENT_VM_BIND_UFENCE	8
#define PRELIM_DRM_XE_EUDEBUG_EVENT_METADATA		9
#define PRELIM_DRM_XE_EUDEBUG_EVENT_VM_BIND_OP_METADATA 10

	__u16 flags;
#define PRELIM_DRM_XE_EUDEBUG_EVENT_CREATE		(1 << 0)
#define PRELIM_DRM_XE_EUDEBUG_EVENT_DESTROY		(1 << 1)
#define PRELIM_DRM_XE_EUDEBUG_EVENT_STATE_CHANGE	(1 << 2)
#define PRELIM_DRM_XE_EUDEBUG_EVENT_NEED_ACK		(1 << 3)

	__u64 seqno;
	__u64 reserved;
};

struct prelim_drm_xe_eudebug_event_client {
	struct prelim_drm_xe_eudebug_event base;

	__u64 client_handle; /* This is unique per debug connection */
};

struct prelim_drm_xe_eudebug_event_vm {
	struct prelim_drm_xe_eudebug_event base;

	__u64 client_handle;
	__u64 vm_handle;
};

struct prelim_drm_xe_eudebug_event_exec_queue {
	struct prelim_drm_xe_eudebug_event base;

	__u64 client_handle;
	__u64 vm_handle;
	__u64 exec_queue_handle;
	__u32 engine_class;
	__u32 width;
	__u64 lrc_handle[];
};

struct prelim_drm_xe_eudebug_event_eu_attention {
	struct prelim_drm_xe_eudebug_event base;

	__u64 client_handle;
	__u64 exec_queue_handle;
	__u64 lrc_handle;
	__u32 flags;
	__u32 bitmask_size;
	__u8 bitmask[];
};

struct prelim_drm_xe_eudebug_eu_control {
	__u64 client_handle;

#define PRELIM_DRM_XE_EUDEBUG_EU_CONTROL_CMD_INTERRUPT_ALL	0
#define PRELIM_DRM_XE_EUDEBUG_EU_CONTROL_CMD_STOPPED		1
#define PRELIM_DRM_XE_EUDEBUG_EU_CONTROL_CMD_RESUME		2
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
 * PRELIM_DRM_XE_EUDEBUG_IOCTL_ACK_EVENT with the event_ufence.base.seqno.
 * This will signal the fence, .value will update and the wait will
 * complete allowing the client to continue.
 *
 */

struct prelim_drm_xe_eudebug_event_vm_bind {
	struct prelim_drm_xe_eudebug_event base;

	__u64 client_handle;
	__u64 vm_handle;

	__u32 flags;
#define PRELIM_DRM_XE_EUDEBUG_EVENT_VM_BIND_FLAG_UFENCE (1 << 0)

	__u32 num_binds;
};

struct prelim_drm_xe_eudebug_event_vm_bind_op {
	struct prelim_drm_xe_eudebug_event base;
	__u64 vm_bind_ref_seqno; /* *_event_vm_bind.base.seqno */
	__u64 num_extensions;

	__u64 addr; /* XXX: Zero for unmap all? */
	__u64 range; /* XXX: Zero for unmap all? */
};

struct prelim_drm_xe_eudebug_event_vm_bind_ufence {
	struct prelim_drm_xe_eudebug_event base;
	__u64 vm_bind_ref_seqno; /* *_event_vm_bind.base.seqno */
};

struct prelim_drm_xe_eudebug_ack_event {
	__u32 type;
	__u32 flags; /* MBZ */
	__u64 seqno;
};

struct prelim_drm_xe_eudebug_vm_open {
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

struct prelim_drm_xe_eudebug_read_metadata {
	__u64 client_handle;
	__u64 metadata_handle;
	__u32 flags;
	__u32 reserved;
	__u64 ptr;
	__u64 size;
};

struct prelim_drm_xe_eudebug_event_metadata {
	struct prelim_drm_xe_eudebug_event base;

	__u64 client_handle;
	__u64 metadata_handle;
	/* XXX: Refer to xe_drm.h for fields */
	__u64 type;
	__u64 len;
};

struct prelim_drm_xe_eudebug_event_vm_bind_op_metadata {
	struct prelim_drm_xe_eudebug_event base;
	__u64 vm_bind_op_ref_seqno; /* *_event_vm_bind_op.base.seqno */

	__u64 metadata_handle;
	__u64 metadata_cookie;
};

#endif /* _XE_DRM_PRELIM_H_ */
