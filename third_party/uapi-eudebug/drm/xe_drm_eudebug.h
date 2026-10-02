/* SPDX-License-Identifier: MIT */
/*
 * Copyright © 2023 Intel Corporation
 */

/* NOTE: Local deviation from the kernel copy, which uses
 * _UAPI_XE_DRM_EUDEBUG_H_. Kept in sync with the guard convention used by
 * xe_drm.h in this directory.
 */
#ifndef _XE_DRM_EUDEBUG_H_
#define _XE_DRM_EUDEBUG_H_

#include "drm.h"

#if defined(__cplusplus)
extern "C" {
#endif

/**
 * DOC: DRM_XE_EUDEBUG_IOCTL_READ_EVENT
 *
 * Receive one event from the connection returned by
 * &DRM_IOCTL_XE_EUDEBUG_CONNECT. The argument is a pointer to a
 * &struct drm_xe_eudebug_event filled in as described there.
 *
 * A connection opened without O_NONBLOCK waits up to five seconds for an
 * event to arrive. Only one reader at a time is allowed on a connection.
 *
 * Return: 0 on success. Negative error code on failure:
 *
 * - -EMSGSIZE if the pending event is larger than the supplied len. len is
 *   updated with the size needed and the event stays queued.
 * - -ETIMEDOUT if the blocking wait expired with no event.
 * - -EAGAIN if O_NONBLOCK was set and no event was queued.
 * - -EBUSY if another thread is already reading on this connection.
 * - -ENOTCONN if the debug target is gone and the queue has been drained.
 */
#define DRM_XE_EUDEBUG_IOCTL_READ_EVENT		_IO('j', 0x0)
#define DRM_XE_EUDEBUG_IOCTL_ACK_EVENT		_IOW('j', 0x1, struct drm_xe_eudebug_ack)
#define DRM_XE_EUDEBUG_IOCTL_VM_OPEN		_IOW('j', 0x2, struct drm_xe_eudebug_vm_open)
#define DRM_XE_EUDEBUG_IOCTL_EU_CONTROL		_IOWR('j', 0x3, struct drm_xe_eudebug_eu_control)

/**
 * struct drm_xe_eudebug_event - Base type of event delivered by xe_eudebug.
 *
 * Base event for xe_eudebug interface.
 *
 * For receiving events :c:member:`drm_xe_eudebug_event.type` has to
 * be DRM_XE_EUDEBUG_EVENT_READ. On return, this is set to the type
 * of event received. :c:member:`drm_xe_eudebug_event.len` has to be
 * set to maximum size that can be received. On return, len will be set
 * to the event size. If the pending event was larger than this size,
 * -EMSGSIZE is returned instead of 0 and the caller should retry with a larger
 * allocated receive length.
 *
 * :c:member:`drm_xe_eudebug_event.seqno` can be used to form a timeline
 * as event delivery order does not guarantee event creation
 * order. Must be set to zero.
 *
 * :c:member:`drm_xe_eudebug_event.flags` will indicate if a resource was
 * created, destroyed, or if its state changed. Must be set to zero.
 *
 * If DRM_XE_EUDEBUG_EVENT_NEED_ACK is set, xe_eudebug
 * will hold the said resource until it is acked by userspace
 * using the acking ioctl with the seqno of the said event.
 */
struct drm_xe_eudebug_event {
	/** @len: Length */
	__u32 len;

	/** @type: Type */
	__u16 type;
#define DRM_XE_EUDEBUG_EVENT_NONE		0
#define DRM_XE_EUDEBUG_EVENT_READ		1
#define DRM_XE_EUDEBUG_EVENT_VM			2
#define DRM_XE_EUDEBUG_EVENT_EXEC_QUEUE		3
#define DRM_XE_EUDEBUG_EVENT_VM_BIND		4
#define DRM_XE_EUDEBUG_EVENT_VM_BIND_OP_DEBUG_DATA	5
#define DRM_XE_EUDEBUG_EVENT_VM_BIND_UFENCE	6
#define DRM_XE_EUDEBUG_EVENT_EU_ATTENTION	7
#define DRM_XE_EUDEBUG_EVENT_PAGEFAULT		8

	/** @flags: Flags */
	__u16 flags;
#define DRM_XE_EUDEBUG_EVENT_CREATE		(1 << 0)
#define DRM_XE_EUDEBUG_EVENT_DESTROY		(1 << 1)
#define DRM_XE_EUDEBUG_EVENT_STATE_CHANGE	(1 << 2)
#define DRM_XE_EUDEBUG_EVENT_NEED_ACK		(1 << 3)

	/** @seqno: Sequence number to form a timeline */
	__u64 seqno;

	/** @reserved: Reserved field, must be zero. */
	__u64 reserved;
};

/**
 * struct drm_xe_eudebug_event_vm - VM event
 *
 * VM event is delivered when vm is created or destroyed.
 */
struct drm_xe_eudebug_event_vm {
	/** @base: base event */
	struct drm_xe_eudebug_event base;

	/** @vm_handle: unique handle for vm */
	__u64 vm_handle;
};

/**
 * struct drm_xe_eudebug_event_exec_queue - Exec Queue resource event
 *
 * Resource creation/destruction event for an Exec Queue
 */
struct drm_xe_eudebug_event_exec_queue {
	/** @base: base event */
	struct drm_xe_eudebug_event base;

	/** @vm_handle: the vm handle this exec queue belongs to */
	__u64 vm_handle;

	/** @exec_queue_handle: unique handle for this exec queue */
	__u64 exec_queue_handle;

	/** @engine_class: engine class for the exec queue */
	__u32 engine_class;

	/** @width: width of exec queue, how many lrcs (handles) it has */
	__u32 width;

	/** @lrc_handle: array of lrc handles */
	__u64 lrc_handle[];
};

/**
 * struct drm_xe_eudebug_event_vm_bind - VM Bind Event
 *
 * When the client (debuggee) calls the vm_bind_ioctl with the
 * DRM_XE_VM_BIND_OP_[ADD|REMOVE]_DEBUG_DATA operation, the following event
 * sequence will be created (for the debugger)::
 *
 *  ┌───────────────────────┐
 *  │  EVENT_VM_BIND        ├──────────────────┬─┬┄┐
 *  └───────────────────────┘                  │ │ ┊
 *      ┌──────────────────────────────────┐   │ │ ┊
 *      │ EVENT_VM_BIND_OP_DEBUG_DATA #1   ├───┘ │ ┊
 *      └──────────────────────────────────┘     │ ┊
 *                      ...                      │ ┊
 *      ┌──────────────────────────────────┐     │ ┊
 *      │ EVENT_VM_BIND_OP_DEBUG_DATA #n   ├─────┘ ┊
 *      └──────────────────────────────────┘       ┊
 *                                                 ┊
 *      ┌┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┐       ┊
 *      ┊ EVENT_UFENCE                     ├┄┄┄┄┄┄┄┘
 *      └┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┘
 *
 * All the events below VM_BIND will reference the VM_BIND
 * they associate with, by field .vm_bind_ref_seqno.
 * EVENT_UFENCE will only be included if the client did
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
	/** @base: Base event */
	struct drm_xe_eudebug_event base;

	/** @vm_handle: VM handle for this bind */
	__u64 vm_handle;

	/** @flags: Bind specific flags */
	__u32 flags;
#define DRM_XE_EUDEBUG_EVENT_VM_BIND_FLAG_UFENCE (1 << 0)

	/** @num_bind_ops: How many [ADD|REMOVE]_DEBUG_DATA operations this bind has */
	__u32 num_bind_ops;
};

/**
 * struct drm_xe_eudebug_event_vm_bind_op_debug_data - VM Bind Op Debug Data Event
 *
 * When the target drm client issues a vm bind, each operation of type
 * %DRM_XE_VM_BIND_OP_ADD_DEBUG_DATA or %DRM_XE_VM_BIND_OP_REMOVE_DEBUG_DATA
 * will generate an event of this type. For reference see
 * :c:type:`drm_xe_vm_bind_op_ext_debug_data`.
 */
struct drm_xe_eudebug_event_vm_bind_op_debug_data {
	/** @base: Base event */
	struct drm_xe_eudebug_event base;

	/** @vm_bind_ref_seqno: Parent :c:member:`drm_xe_eudebug_event_vm_bind.base.seqno` */
	__u64 vm_bind_ref_seqno;

	/** @num_extensions: Extension count for this op */
	__u64 num_extensions;

	/** @addr: Address of the debug data mapping */
	__u64 addr;

	/** @range: Range of the debug data mapping */
	__u64 range;

	/** @flags: Debug data flags */
	__u64 flags;

	/** @offset: Offset into the debug data file */
	__u64 offset;

	/** @reserved: Reserved, must be zero */
	__u64 reserved;
	union {
		/**
		 * @pseudopath: Pseudopath if
		 * %DRM_XE_VM_BIND_DEBUG_DATA_FLAG_PSEUDO was set
		 */
		__u64 pseudopath;

#define DRM_XE_VM_BIND_DEBUG_PATH_MAX 4096
		/** @pathname: Path to the debug data file */
		char pathname[DRM_XE_VM_BIND_DEBUG_PATH_MAX];
	};
};

/**
 * struct drm_xe_eudebug_event_vm_bind_ufence - User Fence Event
 *
 * When target drm client does vm bind with associated user fence,
 * this event will be delivered. This event will have
 * DRM_XE_EUDEBUG_EVENT_NEED_ACK set in :c:member:`drm_xe_eudebug_event.flags`
 * and upon receiving this event you need to ack it with
 * DRM_XE_EUDEBUG_IOCTL_ACK_EVENT.
 *
 */
struct drm_xe_eudebug_event_vm_bind_ufence {
	/** @base: Base event */
	struct drm_xe_eudebug_event base;

	/** @vm_bind_ref_seqno: Parent :c:member:`drm_xe_eudebug_event_vm_bind.base.seqno` */
	__u64 vm_bind_ref_seqno;
};

/**
 * struct drm_xe_eudebug_ack - Deliver ack for an event
 *
 * If event base.flags has DRM_XE_EUDEBUG_EVENT_NEED_ACK set,
 * then the associated resource processing is held for client and
 * thus held for the debugger. In order to release the client,
 * ack needs to be delivered with DRM_XE_EUDEBUG_IOCTL_ACK_EVENT.
 */
struct drm_xe_eudebug_ack {
	/** @type: Type, must be zero */
	__u32 type;

	/** @flags: Flags, must be zero */
	__u32 flags;

	/** @seqno: Seqno of event that is to be acked */
	__u64 seqno;

	/** @reserved: Reserved field, must be zero. */
	__u64 reserved;
};

/**
 * struct drm_xe_eudebug_vm_open - Open a target vm
 *
 * Open target VM for reading and writing with DRM_XE_EUDEBUG_IOCTL_VM_OPEN.
 *
 * File descriptor is returned which can be used with pread and pwrite
 * to inspect and modify the target VM.
 *
 * Multiple operations can be synced with calling fsync(fd). If
 * timeout_ns was specified, the fsync will timeout if the
 * VM can't be guaranteed to be in sync. Caller should re-read the
 * state with pread again.
 *
 */
struct drm_xe_eudebug_vm_open {
	/** @extensions: Pointer to the first extension struct, if any */
	__u64 extensions;

	/** @vm_handle: handle of vm to be accessed */
	__u64 vm_handle;

	/** @flags: flags, must be zero */
	__u64 flags;

	/** @timeout_ns: Timeout value in nanoseconds */
	__u64 timeout_ns;
};

/**
 * struct drm_xe_eudebug_eu_control - Control EU states
 *
 * Issue commands to execution units in hardware.
 *
 * With DRM_XE_EUDEBUG_IOCTL_EU_CONTROL debugger can
 * interrupt all threads on execution units, query thread
 * state and resume execution.
 *
 * :c:member:`drm_xe_eudebug_eu_control.seqno`
 * will be updated to the timeline point when
 * the command was issued.
 *
 * :c:member:`drm_xe_eudebug_eu_control.cmd` can
 * be following:
 *
 * *DRM_XE_EUDEBUG_EU_CONTROL_CMD_INTERRUPT_ALL*
 *  will instruct hardware to stop all threads on EUs
 *  for exec_queue:lrc. This command takes no bitmask, so
 *  :c:member:`drm_xe_eudebug_eu_control.bitmask_size`
 *  must be set to zero.
 *
 * *DRM_XE_EUDEBUG_EU_CONTROL_CMD_STOPPED*
 *  returns the bitmask for threads that are
 *  in so called attention state.
 *
 * *DRM_XE_EUDEBUG_EU_CONTROL_CMD_RESUME*
 *  resumes the threads whose bit is set in the bitmask.
 *
 */
struct drm_xe_eudebug_eu_control {
	/** @cmd: Command for execution units */
#define DRM_XE_EUDEBUG_EU_CONTROL_CMD_INTERRUPT_ALL	0
#define DRM_XE_EUDEBUG_EU_CONTROL_CMD_STOPPED		1
#define DRM_XE_EUDEBUG_EU_CONTROL_CMD_RESUME		2
	__u32 cmd;

	/** @flags: Flags, must be set to zero */
	__u32 flags;

	/** @seqno: Seqno, must be set to zero */
	__u64 seqno;

	/** @exec_queue_handle: Exec queue handle for the command */
	__u64 exec_queue_handle;

	/** @lrc_handle: LRC handle for the command */
	__u64 lrc_handle;

	/** @reserved: Reserved field, must be set to zero */
	__u32 reserved;

	/**
	 * @bitmask_size: Bitmask size in bytes
	 *
	 * Only the leading bytes that fit are used. If this differs from
	 * the size the hardware bitmask needs, it is overwritten with that
	 * size on return, so a short buffer can be grown and the command
	 * retried.
	 */
	__u32 bitmask_size;

	/** @bitmask_ptr: Bitmask pointer, each bit is one thread */
	__u64 bitmask_ptr;
};

/**
 * struct drm_xe_eudebug_event_eu_attention - EU Attention Event
 *
 * Whenever there is any thread in halted/attentions state, this
 * event will be delivered. The event will be delivered periodically
 * until there are no attentions detected.
 *
 */
struct drm_xe_eudebug_event_eu_attention {
	/** @base: base event */
	struct drm_xe_eudebug_event base;

	/** @exec_queue_handle: Exec queue handle for the attentions */
	__u64 exec_queue_handle;

	/** @lrc_handle: LRC handle for the attentions */
	__u64 lrc_handle;

	/**
	 * @flags: Reserved for future use, reads as zero
	 *
	 * Event wide flags are carried in
	 * :c:member:`drm_xe_eudebug_event_eu_attention.base`.
	 */
	__u32 flags;

	/** @bitmask_size: Bitmask size in bytes for bitmask[] */
	__u32 bitmask_size;

	/** @bitmask: Attention bits, one per thread */
	__u8 bitmask[];
};

struct drm_xe_eudebug_event_pagefault {
	struct drm_xe_eudebug_event base;

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

#endif /* _UAPI_XE_DRM_EUDEBUG_H_ */
