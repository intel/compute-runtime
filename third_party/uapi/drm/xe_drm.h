/* SPDX-License-Identifier: MIT */
/*
 * Copyright © 2023 Intel Corporation
 */

#ifndef _XE_DRM_H_
#define _XE_DRM_H_

#include "drm.h"

#if defined(__cplusplus)
extern "C" {
#endif

/*
 * Please note that modifications to all structs defined here are
 * subject to backwards-compatibility constraints.
 *
 * Sections in this file are organized as follows:
 *   1. IOCTL definition
 *   2. Extension definition and helper structs
 *   3. IOCTL's Query structs in the order of the Query's entries.
 *   4. The rest of IOCTL structs in the order of IOCTL declaration.
 *   5. uEvents
 *   6. PMU
 *
 * Some concepts used here are explained in Documentation/gpu/rfc/xe.rst.
 *
 */

/**
 * DOC: Xe Device Block Diagram
 *
 * The diagram below represents a high-level simplification of a discrete
 * GPU supported by the Xe driver. It shows some device components which
 * are necessary to understand this API, as well as how their relations
 * to each other. This diagram does not represent real hardware::
 *
 *   ┌──────────────────────────────────────────────────────────────────┐
 *   │ ┌──────────────────────────────────────────────────┐ ┌─────────┐ │
 *   │ │              ┌───────────────────────┐           │ │ ┌─────┐ │ │
 *   │ │              │         VRAM0         │           │ │ │VRAM1│ │ │
 *   │ │              └───────────┬───────────┘           │ │ └──┬──┘ │ │
 *   │ │ ┌────────────────────────┴─────────────────────┐ │ │ ┌──┴──┐ │ │
 *   │ │ │ ┌─────────────────────┐  ┌─────────────────┐ │ │ │ │     │ │ │
 *   │ │ │ │ ┌──┐ ┌──┐ ┌──┐ ┌──┐ │  │ ┌─────┐ ┌─────┐ │ │ │ │ │     │ │ │
 *   │ │ │ │ │EU│ │EU│ │EU│ │EU│ │  │ │RCS0 │ │BCS0 │ │ │ │ │ │     │ │ │
 *   │ │ │ │ └──┘ └──┘ └──┘ └──┘ │  │ └─────┘ └─────┘ │ │ │ │ │     │ │ │
 *   │ │ │ │ ┌──┐ ┌──┐ ┌──┐ ┌──┐ │  │ ┌─────┐ ┌─────┐ │ │ │ │ │     │ │ │
 *   │ │ │ │ │EU│ │EU│ │EU│ │EU│ │  │ │VCS0 │ │VCS1 │ │ │ │ │ │     │ │ │
 *   │ │ │ │ └──┘ └──┘ └──┘ └──┘ │  │ └─────┘ └─────┘ │ │ │ │ │     │ │ │
 *   │ │ │ │ ┌──┐ ┌──┐ ┌──┐ ┌──┐ │  │ ┌─────┐ ┌─────┐ │ │ │ │ │     │ │ │
 *   │ │ │ │ │EU│ │EU│ │EU│ │EU│ │  │ │VECS0│ │VECS1│ │ │ │ │ │ ... │ │ │
 *   │ │ │ │ └──┘ └──┘ └──┘ └──┘ │  │ └─────┘ └─────┘ │ │ │ │ │     │ │ │
 *   │ │ │ │ ┌──┐ ┌──┐ ┌──┐ ┌──┐ │  │ ┌─────┐ ┌─────┐ │ │ │ │ │     │ │ │
 *   │ │ │ │ │EU│ │EU│ │EU│ │EU│ │  │ │CCS0 │ │CCS1 │ │ │ │ │ │     │ │ │
 *   │ │ │ │ └──┘ └──┘ └──┘ └──┘ │  │ └─────┘ └─────┘ │ │ │ │ │     │ │ │
 *   │ │ │ └─────────DSS─────────┘  │ ┌─────┐ ┌─────┐ │ │ │ │ │     │ │ │
 *   │ │ │                          │ │CCS2 │ │CCS3 │ │ │ │ │ │     │ │ │
 *   │ │ │ ┌─────┐ ┌─────┐ ┌─────┐  │ └─────┘ └─────┘ │ │ │ │ │     │ │ │
 *   │ │ │ │ ... │ │ ... │ │ ... │  │                 │ │ │ │ │     │ │ │
 *   │ │ │ └─DSS─┘ └─DSS─┘ └─DSS─┘  └─────Engines─────┘ │ │ │ │     │ │ │
 *   │ │ └───────────────────────────GT0────────────────┘ │ │ └─GT1─┘ │ │
 *   │ └────────────────────────────Tile0─────────────────┘ └─ Tile1──┘ │
 *   └─────────────────────────────Device0───────┬──────────────────────┘
 *                                               │
 *                                               │
 *                        ───────────────────────┴────────── PCI bus
 *
 */

/**
 * DOC: Xe uAPI Overview
 *
 * This section aims to describe the Xe's IOCTL entries, its structs, and other
 * Xe related uAPI such as uevents and PMU (Platform Monitoring Unit) related
 * entries and usage.
 *
 * List of supported IOCTLs:
 *  - &DRM_IOCTL_XE_DEVICE_QUERY
 *  - &DRM_IOCTL_XE_GEM_CREATE
 *  - &DRM_IOCTL_XE_GEM_MMAP_OFFSET
 *  - &DRM_IOCTL_XE_VM_CREATE
 *  - &DRM_IOCTL_XE_VM_DESTROY
 *  - &DRM_IOCTL_XE_VM_BIND
 *  - &DRM_IOCTL_XE_EXEC
 *  - &DRM_IOCTL_XE_EXEC_QUEUE_CREATE
 *  - &DRM_IOCTL_XE_EXEC_QUEUE_DESTROY
 *  - &DRM_IOCTL_XE_EXEC_QUEUE_SET_PROPERTY
 *  - &DRM_IOCTL_XE_EXEC_QUEUE_GET_PROPERTY
 *  - &DRM_IOCTL_XE_WAIT_USER_FENCE
 *
 */

/*
 * xe specific ioctls.
 *
 * The device specific ioctl range is [DRM_COMMAND_BASE, DRM_COMMAND_END) ie
 * [0x40, 0xa0) (a0 is excluded). The numbers below are defined as offset
 * against DRM_COMMAND_BASE and should be between [0x0, 0x60).
 */
#define DRM_XE_DEVICE_QUERY		0x00
#define DRM_XE_GEM_CREATE		0x01
#define DRM_XE_GEM_MMAP_OFFSET		0x02
#define DRM_XE_VM_CREATE		0x03
#define DRM_XE_VM_DESTROY		0x04
#define DRM_XE_VM_BIND			0x05
#define DRM_XE_EXEC_QUEUE_CREATE	0x06
#define DRM_XE_EXEC_QUEUE_DESTROY	0x07
#define DRM_XE_EXEC_QUEUE_SET_PROPERTY	0x08
#define DRM_XE_EXEC_QUEUE_GET_PROPERTY	0x09
#define DRM_XE_EXEC			0x0a
#define DRM_XE_WAIT_USER_FENCE		0x0b
/* Must be kept compact -- no holes */

#define DRM_IOCTL_XE_DEVICE_QUERY		DRM_IOWR(DRM_COMMAND_BASE + DRM_XE_DEVICE_QUERY, struct drm_xe_device_query)
#define DRM_IOCTL_XE_GEM_CREATE			DRM_IOWR(DRM_COMMAND_BASE + DRM_XE_GEM_CREATE, struct drm_xe_gem_create)
#define DRM_IOCTL_XE_GEM_MMAP_OFFSET		DRM_IOWR(DRM_COMMAND_BASE + DRM_XE_GEM_MMAP_OFFSET, struct drm_xe_gem_mmap_offset)
#define DRM_IOCTL_XE_VM_CREATE			DRM_IOWR(DRM_COMMAND_BASE + DRM_XE_VM_CREATE, struct drm_xe_vm_create)
#define DRM_IOCTL_XE_VM_DESTROY			DRM_IOW(DRM_COMMAND_BASE + DRM_XE_VM_DESTROY, struct drm_xe_vm_destroy)
#define DRM_IOCTL_XE_VM_BIND			DRM_IOW(DRM_COMMAND_BASE + DRM_XE_VM_BIND, struct drm_xe_vm_bind)
#define DRM_IOCTL_XE_EXEC_QUEUE_CREATE		DRM_IOWR(DRM_COMMAND_BASE + DRM_XE_EXEC_QUEUE_CREATE, struct drm_xe_exec_queue_create)
#define DRM_IOCTL_XE_EXEC_QUEUE_DESTROY		DRM_IOW(DRM_COMMAND_BASE + DRM_XE_EXEC_QUEUE_DESTROY, struct drm_xe_exec_queue_destroy)
#define DRM_IOCTL_XE_EXEC_QUEUE_SET_PROPERTY	DRM_IOW(DRM_COMMAND_BASE + DRM_XE_EXEC_QUEUE_SET_PROPERTY, struct drm_xe_exec_queue_set_property)
#define DRM_IOCTL_XE_EXEC_QUEUE_GET_PROPERTY	DRM_IOWR(DRM_COMMAND_BASE + DRM_XE_EXEC_QUEUE_GET_PROPERTY, struct drm_xe_exec_queue_get_property)
#define DRM_IOCTL_XE_EXEC			DRM_IOW(DRM_COMMAND_BASE + DRM_XE_EXEC, struct drm_xe_exec)
#define DRM_IOCTL_XE_WAIT_USER_FENCE		DRM_IOWR(DRM_COMMAND_BASE + DRM_XE_WAIT_USER_FENCE, struct drm_xe_wait_user_fence)

/**
 * DOC: Xe IOCT Extensions
 *
 * Before detailing the IOCTLs and its structs, it is important to highlight
 * that every IOCTL in Xe is extensible.
 *
 * Many interfaces need to grow over time. In most cases we can simply
 * extend the struct and have userspace pass in more data. Another option,
 * as demonstrated by Vulkan's approach to providing extensions for forward
 * and backward compatibility, is to use a list of optional structs to
 * provide those extra details.
 *
 * The key advantage to using an extension chain is that it allows us to
 * redefine the interface more easily than an ever growing struct of
 * increasing complexity, and for large parts of that interface to be
 * entirely optional. The downside is more pointer chasing; chasing across
 * the boundary with pointers encapsulated inside u64.
 *
 * Example chaining:
 *
 * .. code-block:: C
 *
 *	struct xe_user_extension ext3 {
 *		.next_extension = 0, // end
 *		.name = ...,
 *	};
 *	struct xe_user_extension ext2 {
 *		.next_extension = (uintptr_t)&ext3,
 *		.name = ...,
 *	};
 *	struct xe_user_extension ext1 {
 *		.next_extension = (uintptr_t)&ext2,
 *		.name = ...,
 *	};
 *
 * Typically the struct xe_user_extension would be embedded in some uAPI
 * struct, and in this case we would feed it the head of the chain(i.e ext1),
 * which would then apply all of the above extensions.
*/

/**
 * struct xe_user_extension - Base class for defining a chain of extensions
 */
struct xe_user_extension {
	/**
	 * @next_extension:
	 *
	 * Pointer to the next struct xe_user_extension, or zero if the end.
	 */
	__u64 next_extension;

	/**
	 * @name: Name of the extension.
	 *
	 * Note that the name here is just some integer.
	 *
	 * Also note that the name space for this is not global for the whole
	 * driver, but rather its scope/meaning is limited to the specific piece
	 * of uAPI which has embedded the struct xe_user_extension.
	 */
	__u32 name;

	/**
	 * @pad: MBZ
	 *
	 * All undefined bits must be zero.
	 */
	__u32 pad;
};

/**
 * struct drm_xe_ext_set_property - Generic set property extension
 *
 * A generic struct that could allow any of the Xe's IOCLT to be extended
 * with a set_property operation.
 */
struct drm_xe_ext_set_property {
	/** @base: base user extension */
	struct xe_user_extension base;

	/** @property: property to set */
	__u32 property;

	/** @pad: MBZ */
	__u32 pad;

	/** @value: property value */
	__u64 value;

	/** @reserved: Reserved */
	__u64 reserved[2];
};

/**
 * struct drm_xe_engine_class_instance - instance of an engine class
 *
 * It is returned as part of the @drm_xe_engine, but it also is used as
 * the input of engine selection for both @drm_xe_exec_queue_create and
 * @drm_xe_query_engine_cycles
 *
 * The @engine_class can be:
 *  - %DRM_XE_ENGINE_CLASS_RENDER
 *  - %DRM_XE_ENGINE_CLASS_COPY
 *  - %DRM_XE_ENGINE_CLASS_VIDEO_DECODE
 *  - %DRM_XE_ENGINE_CLASS_VIDEO_ENHANCE
 *  - %DRM_XE_ENGINE_CLASS_COMPUTE
 *  - %DRM_XE_ENGINE_CLASS_VM_BIND_ASYNC - Kernel only class (not actual
 *    hardware engine class) used for creating ordered queues of
 *    asynchronous VM bind operations.
 *  - %DRM_XE_ENGINE_CLASS_VM_BIND_SYNC - Kernel only class (not actual
 *    synchronous VM bind operations.
 *
 */
struct drm_xe_engine_class_instance {
#define DRM_XE_ENGINE_CLASS_RENDER		0
#define DRM_XE_ENGINE_CLASS_COPY		1
#define DRM_XE_ENGINE_CLASS_VIDEO_DECODE	2
#define DRM_XE_ENGINE_CLASS_VIDEO_ENHANCE	3
#define DRM_XE_ENGINE_CLASS_COMPUTE		4
#define DRM_XE_ENGINE_CLASS_VM_BIND_ASYNC	5
#define DRM_XE_ENGINE_CLASS_VM_BIND_SYNC	6
	/**
	 * @engine_class: Class of this instance among possible
	 * DRM_XE_ENGINE_CLASS_*
	 */
	__u16 engine_class;
	/** @engine_instance: Engine instance */
	__u16 engine_instance;
	/** @sched_group_id: Scheduling Group ID for this engine instance */
	__u16 sched_group_id;
	/** @pad: MBZ */
	__u16 pad;
};

/**
 * struct drm_xe_engine - describe hardware engine
 */
struct drm_xe_engine {
	/** @instance: The @drm_xe_engine_class_instance */
	struct drm_xe_engine_class_instance instance;

	/** @tile_id: Tile ID where this Engine lives */
	__u16 tile_id;

	/** @gt_id: GT ID where this Engine lives */
	__u16 gt_id;

	/**
	 * @near_mem_regions: Bit mask of instances from
	 * drm_xe_query_mem_region that is near this engine.
	 * Each index in this mask refers directly to the struct
	 * drm_xe_query_mem_region's instance, no assumptions should
	 * be made about order. The type of each region is described
	 * by struct drm_xe_mem_region's mem_class.
	 */
	__u64 near_mem_regions;
	/**
	 * @far_mem_regions: Bit mask of instances from
	 * drm_xe_query_mem_region that is far from this engine.
	 * In general, it has extra indirections when compared to the
	 * @near_mem_regions. For a discrete device this could mean system
	 * memory and memory living in a different Tile.
	 * Each index in this mask refers directly to the struct
	 * drm_xe_query_mem_region's instance, no assumptions should
	 * be made about order. The type of each region is described
	 * by struct drm_xe_mem_region's mem_class.
	 */
	__u64 far_mem_regions;

	/** @reserved: Reserved */
	__u64 reserved[5];
};

/**
 * struct drm_xe_query_engine - describe engines
 *
 * If a query is made with a struct @drm_xe_device_query where .query
 * is equal to %DRM_XE_DEVICE_QUERY_ENGINES, then the reply uses an array of
 * struct @drm_xe_query_engine in .data.
 */
struct drm_xe_query_engine {
	/** @num_engines: number of engines returned in @engines */
	__u32 num_engines;
	/** @pad: MBZ */
	__u32 pad;
	/** @engines: The returned engines for this device */
	struct drm_xe_engine engines[];
};

/**
 * enum drm_xe_memory_class - Supported memory classes.
 */
enum drm_xe_memory_class {
	/** @DRM_XE_MEM_REGION_CLASS_SYSMEM: Represents system memory. */
	DRM_XE_MEM_REGION_CLASS_SYSMEM = 0,
	/**
	 * @DRM_XE_MEM_REGION_CLASS_VRAM: On discrete platforms, this
	 * represents the memory that is local to the device, which we
	 * call VRAM. Not valid on integrated platforms.
	 */
	DRM_XE_MEM_REGION_CLASS_VRAM
};

/**
 * struct drm_xe_mem_region - Describes some region as known to
 * the driver.
 */
struct drm_xe_mem_region {
	/**
	 * @mem_class: The memory class describing this region.
	 *
	 * See enum drm_xe_memory_class for supported values.
	 */
	__u16 mem_class;
	/**
	 * @instance: The unique ID for this region, which serves as the
	 * index in the placement bitmask used as argument for
	 * &DRM_IOCTL_XE_GEM_CREATE
	 */
	__u16 instance;
	/** @pad: MBZ */
	__u32 pad;
	/**
	 * @min_page_size: Min page-size in bytes for this region.
	 *
	 * When the kernel allocates memory for this region, the
	 * underlying pages will be at least @min_page_size in size.
	 * Buffer objects with an allowable placement in this region must be
	 * created with a size aligned to this value.
	 * GPU virtual address mappings of (parts of) buffer objects that
	 * may be placed in this region must also have their GPU virtual
	 * address and range aligned to this value.
	 * Affected IOCTLS will return %-EINVAL if alignment restrictions are
	 * not met.
	 */
	__u32 min_page_size;
	/**
	 * @total_size: The usable size in bytes for this region.
	 */
	__u64 total_size;
	/**
	 * @used: Estimate of the memory used in bytes for this region.
	 *
	 * Requires CAP_PERFMON or CAP_SYS_ADMIN to get reliable
	 * accounting.  Without this the value here will always equal
	 * zero.
	 */
	__u64 used;
	/**
	 * @cpu_visible_size: How much of this region can be CPU
	 * accessed, in bytes.
	 *
	 * This will always be <= @total_size, and the remainder (if
	 * any) will not be CPU accessible. If the CPU accessible part
	 * is smaller than @total_size then this is referred to as a
	 * small BAR system.
	 *
	 * On systems without small BAR (full BAR), the probed_size will
	 * always equal the @total_size, since all of it will be CPU
	 * accessible.
	 *
	 * Note this is only tracked for DRM_XE_MEM_REGION_CLASS_VRAM
	 * regions (for other types the value here will always equal
	 * zero).
	 */
	__u64 cpu_visible_size;
	/**
	 * @cpu_visible_used: Estimate of CPU visible memory used, in
	 * bytes.
	 *
	 * Requires CAP_PERFMON or CAP_SYS_ADMIN to get reliable
	 * accounting. Without this the value here will always equal
	 * zero.  Note this is only currently tracked for
	 * DRM_XE_MEM_REGION_CLASS_VRAM regions (for other types the value
	 * here will always be zero).
	 */
	__u64 cpu_visible_used;
	/** @reserved: Reserved */
	__u64 reserved[6];
};

/**
 * struct drm_xe_query_mem_region - describe memory regions
 *
 * If a query is made with a struct drm_xe_device_query where .query
 * is equal to DRM_XE_DEVICE_QUERY_MEM_REGION, then the reply uses
 * struct drm_xe_query_mem_region in .data.
 */
struct drm_xe_query_mem_region {
	/** @num_mem_regions: number of memory regions returned in @mem_regions */
	__u32 num_mem_regions;
	/** @pad: MBZ */
	__u32 pad;
	/** @mem_regions: The returned memory regions for this device */
	struct drm_xe_mem_region mem_regions[];
};

/**
 * struct drm_xe_query_config - describe the device configuration
 *
 * If a query is made with a struct drm_xe_device_query where .query
 * is equal to DRM_XE_DEVICE_QUERY_CONFIG, then the reply uses
 * struct drm_xe_query_config in .data.
 *
 * The index in @info can be:
 *  - %DRM_XE_QUERY_CONFIG_REV_AND_DEVICE_ID - Device ID (lower 16 bits)
 *    and the device revision (next 8 bits)
 *  - %DRM_XE_QUERY_CONFIG_FLAGS - Flags describing the device
 *    configuration, see list below
 *
 *    - %DRM_XE_QUERY_CONFIG_FLAG_HAS_VRAM - Flag is set if the device
 *      has usable VRAM
 *  - %DRM_XE_QUERY_CONFIG_MIN_ALIGNMENT - Minimal memory alignment
 *    required by this device, typically SZ_4K or SZ_64K
 *  - %DRM_XE_QUERY_CONFIG_VA_BITS - Maximum bits of a virtual address
 *  - %DRM_XE_QUERY_CONFIG_MAX_EXEC_QUEUE_PRIORITY - Value of the highest
 *    available exec queue priority
 */
struct drm_xe_query_config {
	/** @pad: MBZ */
	__u32 pad;

#define DRM_XE_QUERY_CONFIG_REV_AND_DEVICE_ID		0
#define DRM_XE_QUERY_CONFIG_FLAGS			1
	#define DRM_XE_QUERY_CONFIG_FLAG_HAS_VRAM	(1 << 0)
	/*
	 * DRM_XE_QUERY_CONFIG_MIN_ALIGNMENT - This returns the
	 * maximum value of the &min_page_size across all memory regions
	 * the device implements. User-space code that does not want
	 * to track @min_page_size per region can use this value for
	 * a buffer-object size and GPU virtual address and -range
	 * alignment value that is valid for all regions.
	 */
#define DRM_XE_QUERY_CONFIG_MIN_ALIGNMENT		2
#define DRM_XE_QUERY_CONFIG_VA_BITS			3
#define DRM_XE_QUERY_CONFIG_MAX_EXEC_QUEUE_PRIORITY	4
	/** @info: array of elements containing the config info */
	__u64 info[];
};

/**
 * struct drm_xe_gt - describe an individual GT.
 *
 * To be used with drm_xe_query_gt, which will return a list with all the
 * existing GT individual descriptions.
 * Graphics Technology (GT) is a subset of a GPU/tile that is responsible for
 * implementing graphics and/or media operations.
 */
struct drm_xe_gt {
#define DRM_XE_QUERY_GT_TYPE_MAIN		0
#define DRM_XE_QUERY_GT_TYPE_MEDIA		1
	/** @type: GT type: Main or Media */
	__u16 type;
	/** @tile_id: Tile ID where this GT lives (Information only) */
	__u16 tile_id;
	/** @gt_id: Unique ID of this GT within the PCI Device */
	__u16 gt_id;
	/** @reference_clock: A clock frequency for timestamp */
	__u32 reference_clock;
	/** @reserved: Reserved */
	__u64 reserved[8];
};

/**
 * struct drm_xe_query_gt - A list with GT description items.
 *
 * If a query is made with a struct drm_xe_device_query where .query
 * is equal to DRM_XE_DEVICE_QUERY_GT, then the reply uses struct
 * drm_xe_query_gt in .data.
 */
struct drm_xe_query_gt {
	/** @num_gt: number of GT items returned in gt_list */
	__u32 num_gt;
	/** @pad: MBZ */
	__u32 pad;
	/** @gt_list: The GT list returned for this device */
	struct drm_xe_gt gt_list[];
};

/**
 * struct drm_xe_query_topology_mask - describe the topology mask of a GT
 *
 * This is the hardware topology which reflects the internal physical
 * structure of the GPU.
 *
 * If a query is made with a struct drm_xe_device_query where .query
 * is equal to DRM_XE_DEVICE_QUERY_GT_TOPOLOGY, then the reply uses
 * struct drm_xe_query_topology_mask in .data.
 *
 * The @type can be:
 *  - %DRM_XE_TOPO_DSS_GEOMETRY - To query the mask of Dual Sub Slices
 *    (DSS) available for geometry operations. For example a query response
 *    containing the following in mask:
 *    ``DSS_GEOMETRY    ff ff ff ff 00 00 00 00``
 *    means 32 DSS are available for geometry.
 *  - %DRM_XE_TOPO_DSS_COMPUTE - To query the mask of Dual Sub Slices
 *    (DSS) available for compute operations. For example a query response
 *    containing the following in mask:
 *    ``DSS_COMPUTE    ff ff ff ff 00 00 00 00``
 *    means 32 DSS are available for compute.
 *  - %DRM_XE_TOPO_EU_PER_DSS - To query the mask of Execution Units (EU)
 *    available per Dual Sub Slices (DSS). For example a query response
 *    containing the following in mask:
 *    ``EU_PER_DSS    ff ff 00 00 00 00 00 00``
 *    means each DSS has 16 EU.
 *
 */
struct drm_xe_query_topology_mask {
	/** @gt_id: GT ID the mask is associated with */
	__u16 gt_id;

#define DRM_XE_TOPO_DSS_GEOMETRY	(1 << 0)
#define DRM_XE_TOPO_DSS_COMPUTE		(1 << 1)
#define DRM_XE_TOPO_EU_PER_DSS		(1 << 2)
	/** @type: type of mask */
	__u16 type;

	/** @num_bytes: number of bytes in requested mask */
	__u32 num_bytes;

	/** @mask: little-endian mask of @num_bytes */
	__u8 mask[];
};

/**
 * struct drm_xe_query_engine_cycles - correlate CPU and GPU timestamps
 *
 * If a query is made with a struct drm_xe_device_query where .query is equal to
 * DRM_XE_DEVICE_QUERY_ENGINE_CYCLES, then the reply uses struct drm_xe_query_engine_cycles
 * in .data. struct drm_xe_query_engine_cycles is allocated by the user and
 * .data points to this allocated structure.
 *
 * The query returns the engine cycles, which along with GT's @reference_clock,
 * can be used to calculate the engine timestamp. In addition the
 * query returns a set of cpu timestamps that indicate when the command
 * streamer cycle count was captured.
 */
struct drm_xe_query_engine_cycles {
	/**
	 * @eci: This is input by the user and is the engine for which command
	 * streamer cycles is queried.
	 */
	struct drm_xe_engine_class_instance eci;

	/**
	 * @clockid: This is input by the user and is the reference clock id for
	 * CPU timestamp. For definition, see clock_gettime(2) and
	 * perf_event_open(2). Supported clock ids are CLOCK_MONOTONIC,
	 * CLOCK_MONOTONIC_RAW, CLOCK_REALTIME, CLOCK_BOOTTIME, CLOCK_TAI.
	 */
	__s32 clockid;

	/** @width: Width of the engine cycle counter in bits. */
	__u32 width;

	/**
	 * @engine_cycles: Engine cycles as read from its register
	 * at 0x358 offset.
	 */
	__u64 engine_cycles;

	/**
	 * @cpu_timestamp: CPU timestamp in ns. The timestamp is captured before
	 * reading the engine_cycles register using the reference clockid set by the
	 * user.
	 */
	__u64 cpu_timestamp;

	/**
	 * @cpu_delta: Time delta in ns captured around reading the lower dword
	 * of the engine_cycles register.
	 */
	__u64 cpu_delta;
};

/**
 * struct drm_xe_query_uc_fw_version - query a micro-controller firmware version
 *
 * Given a uc_type this will return the major, minor, patch and branch version
 * of the micro-controller firmware.
 *
 * The @uc_type can be:
 *  - %DRM_XE_QUERY_UC_TYPE_GUC_SUBMISSION - This is the GuC Submission Version,
 *    a.k.a 'VF version'. It is not the actual GuC blob version. A running GuC can
 *    support multiple VF APIs with different Submission Versions. This version is
 *    negotiated by the VF KMD with GuC during VF initialization. In most of the
 *    current available GuC blobs, this is a 1-1 relationship where the Submission
 *    version could be inferred from the running version and vice-versa. However,
 *    the submission version is the most useful information for the user space
 *    perspective and needs.
 *  - %DRM_XE_QUERY_TYPE_HUC - The actual HuC blob that is currently running
 *    in the platform. It returns 0 when HuC is not currently loaded.
 *
 */
struct drm_xe_query_uc_fw_version {
	/** @uc_type: The micro-controller type to query firmware version */
#define DRM_XE_QUERY_UC_TYPE_GUC_SUBMISSION	0
#define DRM_XE_QUERY_UC_TYPE_HUC		1
	__u16 uc_type;

	/** @reserved: Reserved */
	__u16 reserved;

	/** @major_ver: major uc fw version */
	__u32 major_ver;
	/** @minor_ver: minor uc fw version */
	__u32 minor_ver;
	/** @patch_ver: patch uc fw version */
	__u32 patch_ver;
	/** @branch_ver: branch uc fw version */
	__u32 branch_ver;

	/** @pad2: MBZ */
	__u32 pad2;

	/** @reserved2: Reserved */
	__u64 reserved2;
};

/**
 * struct drm_xe_device_query - Input of &DRM_IOCLT_XE_DEVICE_QUERY - The
 * main structure to query device information
 *
 * The user selects the type of data to query among DRM_XE_DEVICE_QUERY_*
 * and sets the value in the query member. This determines the type of
 * the structure provided by the driver in data, among struct drm_xe_query_*.
 *
 * The @query can be:
 *  - %DRM_XE_DEVICE_QUERY_ENGINES
 *  - %DRM_XE_DEVICE_QUERY_MEM_REGIONS
 *  - %DRM_XE_DEVICE_QUERY_CONFIG
 *  - %DRM_XE_DEVICE_QUERY_GT - Query type to retrieve the hardware
 *    configuration of the device such as information on slices, memory,
 *    caches, and so on. It is provided as a table of key / value
 *    attributes.
 *  - %DRM_XE_DEVICE_QUERY_HWCONFIG
 *  - %DRM_XE_DEVICE_QUERY_GT_TOPOLOGY
 *  - %DRM_XE_DEVICE_QUERY_ENGINE_CYCLES
 *  - %DRM_XE_DEVICE_QUERY_UC_FW_VERSION
 *
 * If size is set to 0, the driver fills it with the required size for
 * the requested type of data to query. If size is equal to the required
 * size, the queried information is copied into data. If size is set to
 * a value different from 0 and different from the required size, the
 * IOCTL call returns -EINVAL.
 *
 * For example the following code snippet allows retrieving and printing
 * information about the device engines with DRM_XE_DEVICE_QUERY_ENGINES:
 *
 * .. code-block:: C
 *
 *     struct drm_xe_query_engine *engines;
 *     struct drm_xe_device_query query = {
 *         .extensions = 0,
 *         .query = DRM_XE_DEVICE_QUERY_ENGINES,
 *         .size = 0,
 *         .data = 0,
 *     };
 *     ioctl(fd, DRM_IOCTL_XE_DEVICE_QUERY, &query);
 *     engines = malloc(query.size);
 *     query.data = (uintptr_t)engines;
 *     ioctl(fd, DRM_IOCTL_XE_DEVICE_QUERY, &query);
 *     for (int i = 0; i < engines->num_engines; i++) {
 *         printf("Engine %d: %s\n", i,
 *             engines->engines[i].instance.engine_class == DRM_XE_ENGINE_CLASS_RENDER ? "RENDER":
 *             engines->engines[i].instance.engine_class == DRM_XE_ENGINE_CLASS_COPY ? "COPY":
 *             engines->engines[i].instance.engine_class == DRM_XE_ENGINE_CLASS_VIDEO_DECODE ? "VIDEO_DECODE":
 *             engines->engines[i].instance.engine_class == DRM_XE_ENGINE_CLASS_VIDEO_ENHANCE ? "VIDEO_ENHANCE":
 *             engines->engines[i].instance.engine_class == DRM_XE_ENGINE_CLASS_COMPUTE ? "COMPUTE":
 *             "UNKNOWN");
 *     }
 *     free(engines);
 */
struct drm_xe_device_query {
	/** @extensions: Pointer to the first extension struct, if any */
	__u64 extensions;

#define DRM_XE_DEVICE_QUERY_ENGINES		0
#define DRM_XE_DEVICE_QUERY_MEM_REGION		1
#define DRM_XE_DEVICE_QUERY_CONFIG		2
#define DRM_XE_DEVICE_QUERY_GT			3
#define DRM_XE_DEVICE_QUERY_HWCONFIG		4
#define DRM_XE_DEVICE_QUERY_GT_TOPOLOGY		5
#define DRM_XE_DEVICE_QUERY_ENGINE_CYCLES	6
#define DRM_XE_DEVICE_QUERY_UC_FW_VERSION	7
	/** @query: The type of data to query */
	__u32 query;

	/** @size: Size of the queried data */
	__u32 size;

	/** @data: Queried data is placed here */
	__u64 data;

	/** @reserved: Reserved */
	__u64 reserved[2];
};

/**
 * struct drm_xe_gem_create - Input of &DRM_IOCLT_XE_GEM_CREATE - A structure for
 * gem creation
 *
 * The @flags can be:
 *  - %DRM_XE_GEM_CREATE_FLAG_DEFER_BACKING
 *  - %DRM_XE_GEM_CREATE_FLAG_SCANOUT
 *  - %DRM_XE_GEM_CREATE_FLAG_NEEDS_VISIBLE_VRAM - When using VRAM as a
 *    possible placement, ensure that the corresponding VRAM allocation
 *    will always use the CPU accessible part of VRAM. This is important
 *    for small-bar systems (on full-bar systems this gets turned into a
 *    noop).
 *    Note1: System memory can be used as an extra placement if the kernel
 *    should spill the allocation to system memory, if space can't be made
 *    available in the CPU accessible part of VRAM (giving the same
 *    behaviour as the i915 interface, see
 *    I915_GEM_CREATE_EXT_FLAG_NEEDS_CPU_ACCESS).
 *    Note2: For clear-color CCS surfaces the kernel needs to read the
 *    clear-color value stored in the buffer, and on discrete platforms we
 *    need to use VRAM for display surfaces, therefore the kernel requires
 *    setting this flag for such objects, otherwise an error is thrown on
 *    small-bar systems.
 *
 */
struct drm_xe_gem_create {
	/** @extensions: Pointer to the first extension struct, if any */
	__u64 extensions;

	/**
	 * @size: Size of the object to be created, must match region
	 * (system or vram) minimum alignment (&min_page_size).
	 */
	__u64 size;

	/**
	 * @placement: A mask of memory instances of where GEM can be placed.
	 * Each index in this mask refers directly to the struct
	 * drm_xe_query_mem_region's instance, no assumptions should
	 * be made about order. The type of each region is described
	 * by struct drm_xe_mem_region's mem_class.
	 */
	__u32 placement;

#define DRM_XE_GEM_CREATE_FLAG_DEFER_BACKING		(1 << 0)
#define DRM_XE_GEM_CREATE_FLAG_SCANOUT			(1 << 1)
#define DRM_XE_GEM_CREATE_FLAG_NEEDS_VISIBLE_VRAM	(1 << 2)
	/**
	 * @flags: Flags, currently a mask of memory instances of where GEM can
	 * be placed
	 */
	__u32 flags;

	/**
	 * @vm_id: Attached VM, if any
	 *
	 * If a VM is specified, this GEM must:
	 *
	 *  1. Only ever be bound to that VM.
	 *  2. Cannot be exported as a PRIME fd.
	 */
	__u32 vm_id;

	/**
	 * @handle: Returned handle for the object.
	 *
	 * Object handles are nonzero.
	 */
	__u32 handle;

	/** @reserved: Reserved */
	__u64 reserved[2];
};

/**
 * struct drm_xe_gem_mmap_offset - Input of &DRM_IOCTL_XE_GEM_MMAP_OFFSET
 */
struct drm_xe_gem_mmap_offset {
	/** @extensions: Pointer to the first extension struct, if any */
	__u64 extensions;

	/** @handle: Handle for the object being mapped. */
	__u32 handle;

	/** @flags: Must be zero */
	__u32 flags;

	/** @offset: The fake offset to use for subsequent mmap call */
	__u64 offset;

	/** @reserved: Reserved */
	__u64 reserved[2];
};

/**
 * struct drm_xe_vm_create - Input of &DRM_IOCTL_XE_VM_CREATE
 */
struct drm_xe_vm_create {
#define DRM_XE_VM_EXTENSION_SET_PROPERTY	0
	/** @extensions: Pointer to the first extension struct, if any */
	__u64 extensions;

#define DRM_XE_VM_CREATE_FLAG_SCRATCH_PAGE	(1 << 0)
#define DRM_XE_VM_CREATE_FLAG_COMPUTE_MODE	(1 << 1)
#define DRM_XE_VM_CREATE_FLAG_ASYNC_DEFAULT	(1 << 2)
#define DRM_XE_VM_CREATE_FLAG_FAULT_MODE	(1 << 3)
	/** @flags: Flags */
	__u32 flags;

	/** @vm_id: Returned VM ID */
	__u32 vm_id;

	/** @reserved: Reserved */
	__u64 reserved[2];
};

/**
 * struct drm_xe_vm_destroy - Input of &DRM_IOCTL_XE_VM_DESTROY
 */
struct drm_xe_vm_destroy {
	/** @vm_id: VM ID */
	__u32 vm_id;

	/** @pad: MBZ */
	__u32 pad;

	/** @reserved: Reserved */
	__u64 reserved[2];
};

/**
 * struct drm_xe_vm_bind_op - run bind operations
 *
 * The @op can be:
 *  - %DRM_XE_VM_BIND_OP_MAP
 *  - %DRM_XE_VM_BIND_OP_UNMAP
 *  - %DRM_XE_VM_BIND_OP_MAP_USERPTR
 *  - %DRM_XE_VM_BIND_OP_UNMAP_ALL
 *  - %DRM_XE_VM_BIND_OP_PREFETCH
 *
 * and the @flags can be:
 *  - %DRM_XE_VM_BIND_FLAG_READONLY
 *  - %DRM_XE_VM_BIND_FLAG_ASYNC
 *  - %DRM_XE_VM_BIND_FLAG_IMMEDIATE - Valid on a faulting VM only, do the
 *    MAP operation immediately rather than deferring the MAP to the page
 *    fault handler.
 *  - %DRM_XE_VM_BIND_FLAG_NULL - When the NULL flag is set, the page
 *    tables are setup with a special bit which indicates writes are
 *    dropped and all reads return zero. In the future, the NULL flags
 *    will only be valid for DRM_XE_VM_BIND_OP_MAP operations, the GEM
 *    handle MBZ, and the GEM offset MBZ. This flag is intended to
 *    implement VK sparse bindings.
 *
 */
struct drm_xe_vm_bind_op {
	/** @extensions: Pointer to the first extension struct, if any */
	__u64 extensions;

	/**
	 * @obj: GEM object to operate on, MBZ for MAP_USERPTR, MBZ for UNMAP
	 */
	__u32 obj;

	/** @pad: MBZ */
	__u32 pad;

	union {
		/**
		 * @obj_offset: Offset into the object, MBZ for CLEAR_RANGE,
		 * ignored for unbind
		 */
		__u64 obj_offset;

		/** @userptr: user pointer to bind on */
		__u64 userptr;
	};

	/**
	 * @range: Number of bytes from the object to bind to addr, MBZ for UNMAP_ALL
	 */
	__u64 range;

	/** @addr: Address to operate on, MBZ for UNMAP_ALL */
	__u64 addr;

	/**
	 * @pt_placement_hint: An optional memory_region bit-mask hint, which
	 * only applies when creating new VMAs. Default value '0' is the
	 * recommended value.
	 *
	 * It hints the optimal placement for the page-table tree for this VMA.
	 * For instance, when userspace is using engines living in a secondary
	 * tile with allocated BOs near those engines, that same
	 * @near_mem_region could be used in this hint field.
	 *
	 * Since it is a hint, the Xe kernel driver is free to ignore this mask
	 * and choose the best location for the page-table, taking into
	 * consideration the running hardware and runtime constrains.
	 */
	__u64 pt_placement_hint;

#define DRM_XE_VM_BIND_OP_MAP		0x0
#define DRM_XE_VM_BIND_OP_UNMAP		0x1
#define DRM_XE_VM_BIND_OP_MAP_USERPTR	0x2
#define DRM_XE_VM_BIND_OP_UNMAP_ALL	0x3
#define DRM_XE_VM_BIND_OP_PREFETCH	0x4
	/** @op: Bind operation to perform */
	__u32 op;

#define DRM_XE_VM_BIND_FLAG_READONLY	(1 << 0)
#define DRM_XE_VM_BIND_FLAG_ASYNC	(1 << 1)
#define DRM_XE_VM_BIND_FLAG_IMMEDIATE	(1 << 2)
#define DRM_XE_VM_BIND_FLAG_NULL	(1 << 3)
	/** @flags: Bind flags */
	__u32 flags;

	/**
	 * @prefetch_mem_region_instance: Memory region to prefetch VMA to.
	 * It is a region instance, not a mask.
	 * To be used only with %DRM_XE_VM_BIND_OP_PREFETCH operation.
	 */
	__u32 prefetch_mem_region_instance;

	/** @reserved: Reserved */
	__u64 reserved[2];
};

/**
 * struct drm_xe_vm_bind - Input of &DRM_IOCTL_XE_VM_BIND
 *
 * Below is an example of a minimal use of @drm_xe_vm_bind to
 * asynchronously bind the buffer `data` at address `BIND_ADDRESS` to
 * illustrate `userptr`. It can be synchronized by using the example
 * provided for @drm_xe_sync.
 *
 * .. code-block:: C
 *
 *     data = aligned_alloc(ALIGNMENT, BO_SIZE);
 *     struct drm_xe_vm_bind bind = {
 *         .vm_id = vm,
 *         .num_binds = 1,
 *         .bind.obj = 0,
 *         .bind.obj_offset = to_user_pointer(data),
 *         .bind.range = BO_SIZE,
 *         .bind.addr = BIND_ADDRESS,
 *         .bind.op = DRM_XE_VM_BIND_OP_MAP_USERPTR,
 *         .bind.flags = DRM_XE_VM_BIND_FLAG_ASYNC,
 *         .num_syncs = 1,
 *         .syncs = &sync,
 *         .exec_queue_id = 0,
 *     };
 *     ioctl(fd, DRM_IOCTL_XE_VM_BIND, &bind);
 *
 */
struct drm_xe_vm_bind {
	/** @extensions: Pointer to the first extension struct, if any */
	__u64 extensions;

	/** @vm_id: The ID of the VM to bind to */
	__u32 vm_id;

	/**
	 * @exec_queue_id: exec_queue_id, must be of class DRM_XE_ENGINE_CLASS_VM_BIND
	 * and exec queue must have same vm_id. If zero, the default VM bind engine
	 * is used.
	 */
	__u32 exec_queue_id;

	/** @num_binds: number of binds in this IOCTL */
	__u32 num_binds;

	/** @pad: MBZ */
	__u32 pad;

	union {
		/** @bind: used if num_binds == 1 */
		struct drm_xe_vm_bind_op bind;

		/**
		 * @vector_of_binds: userptr to array of struct
		 * drm_xe_vm_bind_op if num_binds > 1
		 */
		__u64 vector_of_binds;
	};

	/** @num_syncs: amount of syncs to wait on */
	__u32 num_syncs;

	/** @pad2: MBZ */
	__u32 pad2;

	/** @syncs: pointer to struct drm_xe_sync array */
	__u64 syncs;

	/** @reserved: Reserved */
	__u64 reserved[2];
};

/* For use with XE_EXEC_QUEUE_SET_PROPERTY_ACC_GRANULARITY */

/* Monitor 128KB contiguous region with 4K sub-granularity */
#define XE_ACC_GRANULARITY_128K 0

/* Monitor 2MB contiguous region with 64KB sub-granularity */
#define XE_ACC_GRANULARITY_2M 1

/* Monitor 16MB contiguous region with 512KB sub-granularity */
#define XE_ACC_GRANULARITY_16M 2

/* Monitor 64MB contiguous region with 2M sub-granularity */
#define XE_ACC_GRANULARITY_64M 3

/**
 * struct drm_xe_sync - Main structure for sync objects and user fences
 *
 * This can be used with both @drm_xe_exec or with @drm_xe_vm_bind
 *
 * The @type can be:
 *  - %DRM_XE_SYNC_TYPE_SYNCOBJ - A simple drm sync object
 *  - %DRM_XE_SYNC_TYPE_TIMELINE_SYNCOBJ - A timelined sync object
 *  - %DRM_XE_SYNC_TYPE_USER_FENCE - An user fence
 *
 * The @flags can be:
 *  - %DRM_XE_SYNC_FLAG_SIGNAL
 *
 * A minimal use of @drm_xe_sync looks like this:
 *
 * .. code-block:: C
 *
 *     struct drm_xe_sync sync = {
 *         .flags = DRM_XE_SYNC_FLAG_SIGNAL,
 *         .type = DRM_XE_SYNC_TYPE_SYNCOBJ,
 *     };
 *     struct drm_syncobj_create syncobj_create = { 0 };
 *     ioctl(fd, DRM_IOCTL_SYNCOBJ_CREATE, &syncobj_create);
 *     sync.handle = syncobj_create.handle;
 *         ...
 *         use of &sync in drm_xe_exec or drm_xe_vm_bind
 *         ...
 *     struct drm_syncobj_wait wait = {
 *         .handles = &sync.handle,
 *         .timeout_nsec = INT64_MAX,
 *         .count_handles = 1,
 *         .flags = 0,
 *         .first_signaled = 0,
 *         .pad = 0,
 *     };
 *     ioctl(fd, DRM_IOCTL_SYNCOBJ_WAIT, &wait);
 *
 */
struct drm_xe_sync {
	/** @extensions: Pointer to the first extension struct, if any */
	__u64 extensions;

#define DRM_XE_SYNC_TYPE_SYNCOBJ		0x0
#define DRM_XE_SYNC_TYPE_TIMELINE_SYNCOBJ	0x1
#define DRM_XE_SYNC_TYPE_USER_FENCE		0x2
	/** @type: Type of the this sync object */
	__u32 type;

#define DRM_XE_SYNC_FLAG_SIGNAL	(1 << 0)
	/** @flags: Sync Flags */
	__u32 flags;

	union {
		/** @handle: Handle to the sync object */
		__u32 handle;

		/**
		 * @addr: Address of user fence. When sync is passed in via exec
		 * IOCTL this is a GPU address in the VM. When sync passed in via
		 * VM bind IOCTL this is a user pointer. In either case, it is
		 * the users responsibility that this address is present and
		 * mapped when the user fence is signalled. Must be qword
		 * aligned.
		 */
		__u64 addr;
	};

	/**
	 * @timeline_value: Input for the timeline sync object. Needs to be
	 * different than 0 when used with %DRM_XE_SYNC_TYPE_TIMELINE_SYNCOBJ.
	 */
	__u64 timeline_value;

	/** @reserved: Reserved */
	__u64 reserved[2];
};

/**
 * DOC: Execution Queue
 *
 * The Execution Queue abstracts the Hardware Engine that is going to be used
 * with the execution of the Batch Buffers in &DRM_IOCTL_XE_EXEC
 *
 * In a regular usage of this execution queue, only one hardware engine pointer
 * would be given as input of the @instances below and both @num_bb_per_exec and
 * @num_eng_per_bb would be set to '1'.
 *
 * Regular execution example::
 *
 *                    ┌─────┐
 *                    │ BB0 │
 *                    └──┬──┘
 *                       │     @num_bb_per_exec = 1
 *                       │     @num_eng_per_bb = 1
 *                       │     @instances = {Engine0}
 *                       ▼
 *                   ┌───────┐
 *                   │Engine0│
 *                   └───────┘
 *
 * However this execution queue is flexible to be used for parallel submission or
 * for load balancing submission (a.k.a virtual load balancing).
 *
 * In a parallel submission, different batch buffers will be simultaneously
 * dispatched to different engines listed in @instances, in a 1-1 relationship.
 *
 * Parallel execution example::
 *
 *               ┌─────┐   ┌─────┐
 *               │ BB0 │   │ BB1 │
 *               └──┬──┘   └──┬──┘
 *                  │         │     @num_bb_per_exec = 2
 *                  │         │     @num_eng_per_bb = 1
 *                  │         │     @instances = {Engine0, Engine1}
 *                  ▼         ▼
 *              ┌───────┐ ┌───────┐
 *              │Engine0│ │Engine1│
 *              └───────┘ └───────┘
 *
 * On a load balancing submission, each batch buffer is virtually dispatched
 * to all of the listed engine @instances. Then, underneath driver, firmware, or
 * hardware can select the best available engine to actually run the job.
 *
 * Virtual Load Balancing example::
 *
 *                    ┌─────┐
 *                    │ BB0 │
 *                    └──┬──┘
 *                       │      @num_bb_per_exec = 1
 *                       │      @num_eng_per_bb = 2
 *                       │      @instances = {Engine0, Engine1}
 *                  ┌────┴────┐
 *                  │         │
 *                  ▼         ▼
 *              ┌───────┐ ┌───────┐
 *              │Engine0│ │Engine1│
 *              └───────┘ └───────┘
 */

/**
 * struct drm_xe_exec_queue_create - Input of &DRM_IOCTL_XE_EXEC_QUEUE_CREATE
 *
 * The example below shows how to use @drm_xe_exec_queue_create to create
 * a simple exec_queue (no parallel submission) of class
 * &DRM_XE_ENGINE_CLASS_RENDER.
 *
 * .. code-block:: C
 *
 *     struct drm_xe_engine_class_instance instance = {
 *         .engine_class = DRM_XE_ENGINE_CLASS_RENDER,
 *     };
 *     struct drm_xe_exec_queue_create exec_queue_create = {
 *          .extensions = 0,
 *          .vm_id = vm,
 *          .num_bb_per_exec = 1,
 *          .num_eng_per_bb = 1,
 *          .instances = to_user_pointer(&instance),
 *     };
 *     ioctl(fd, DRM_IOCTL_XE_EXEC_QUEUE_CREATE, &exec_queue_create);
 *
 */
struct drm_xe_exec_queue_create {
#define DRM_XE_EXEC_QUEUE_EXTENSION_SET_PROPERTY               0
	/** @extensions: Pointer to the first extension struct, if any */
	__u64 extensions;

	/**
	 * @num_bb_per_exec: Indicates a submission width for this exec queue,
	 * for how many batch buffers can be submitted in parallel.
	 */
	__u16 num_bb_per_exec;

	/**
	 * @num_eng_per_bb: Indicates how many possible engines are available
	 * at @instances for the Xe to distribute the load.
	 */
	__u16 num_eng_per_bb;

	/** @vm_id: VM to use for this exec queue */
	__u32 vm_id;

	/** @flags: MBZ */
	__u32 flags;

	/** @exec_queue_id: Returned exec queue ID */
	__u32 exec_queue_id;

	/**
	 * @instances: user pointer to a 2-d array of struct
	 * drm_xe_engine_class_instance
	 *
	 * Every engine in the array needs to have the same @sched_group_id
	 *
	 * length = num_bb_per_exec (i) * num_eng_per_bb (j)
	 * index = j + i * num_bb_per_exec
	 */
	__u64 instances;

	/** @reserved: Reserved */
	__u64 reserved[2];
};

/**
 * struct drm_xe_exec_queue_destroy - Input of &DRM_IOCTL_XE_EXEC_QUEUE_DESTROY
 */
struct drm_xe_exec_queue_destroy {
	/** @exec_queue_id: Exec queue ID */
	__u32 exec_queue_id;

	/** @pad: MBZ */
	__u32 pad;

	/** @reserved: Reserved */
	__u64 reserved[2];
};

/**
 * struct drm_xe_exec_queue_set_property - Input of &DRM_IOCTL_XE_EXEC_QUEUE_SET_PROPERTY
 *
 * The @property can be:
 *  - %DRM_XE_EXEC_QUEUE_SET_PROPERTY_PRIORITY
 *  - %DRM_XE_EXEC_QUEUE_SET_PROPERTY_TIMESLICE
 *  - %DRM_XE_EXEC_QUEUE_SET_PROPERTY_PREEMPTION_TIMEOUT
 *  - %DRM_XE_EXEC_QUEUE_SET_PROPERTY_PERSISTENCE
 *  - %DRM_XE_EXEC_QUEUE_SET_PROPERTY_JOB_TIMEOUT
 *  - %DRM_XE_EXEC_QUEUE_SET_PROPERTY_ACC_TRIGGER
 *  - %DRM_XE_EXEC_QUEUE_SET_PROPERTY_ACC_NOTIFY
 *  - %DRM_XE_EXEC_QUEUE_SET_PROPERTY_ACC_GRANULARITY
 *
 */
struct drm_xe_exec_queue_set_property {
	/** @extensions: Pointer to the first extension struct, if any */
	__u64 extensions;

	/** @exec_queue_id: Exec queue ID */
	__u32 exec_queue_id;

#define DRM_XE_EXEC_QUEUE_SET_PROPERTY_PRIORITY			0
#define DRM_XE_EXEC_QUEUE_SET_PROPERTY_TIMESLICE		1
#define DRM_XE_EXEC_QUEUE_SET_PROPERTY_PREEMPTION_TIMEOUT	2
#define DRM_XE_EXEC_QUEUE_SET_PROPERTY_PERSISTENCE		3
#define DRM_XE_EXEC_QUEUE_SET_PROPERTY_JOB_TIMEOUT		4
#define DRM_XE_EXEC_QUEUE_SET_PROPERTY_ACC_TRIGGER		5
#define DRM_XE_EXEC_QUEUE_SET_PROPERTY_ACC_NOTIFY		6
#define DRM_XE_EXEC_QUEUE_SET_PROPERTY_ACC_GRANULARITY		7
	/** @property: property to set */
	__u32 property;

	/** @value: property value */
	__u64 value;

	/** @reserved: Reserved */
	__u64 reserved[2];
};

/**
 * struct drm_xe_exec_queue_get_property - Input of &DRM_IOCTL_XE_EXEC_QUEUE_GET_PROPERTY
 *
 * The @property can be:
 *  - %DRM_XE_EXEC_QUEUE_GET_PROPERTY_BAN
 *
 */
struct drm_xe_exec_queue_get_property {
	/** @extensions: Pointer to the first extension struct, if any */
	__u64 extensions;

	/** @exec_queue_id: Exec queue ID */
	__u32 exec_queue_id;

#define DRM_XE_EXEC_QUEUE_GET_PROPERTY_BAN	0
	/** @property: property to get */
	__u32 property;

	/** @value: property value */
	__u64 value;

	/** @reserved: Reserved */
	__u64 reserved[2];
};

/**
 * struct drm_xe_exec - Input of &DRM_IOCTL_XE_EXEC
 *
 * This is an example to use @drm_xe_exec for execution of the object
 * at BIND_ADDRESS (see example in @drm_xe_vm_bind) by an exec_queue
 * (see example in @drm_xe_exec_queue_create). It can be synchronized
 * by using the example provided for @drm_xe_sync.
 *
 * .. code-block:: C
 *
 *     struct drm_xe_exec exec = {
 *         .exec_queue_id = exec_queue,
 *         .syncs = &sync,
 *         .num_syncs = 1,
 *         .address = BIND_ADDRESS,
 *         .num_batch_buffer = 1,
 *     };
 *     ioctl(fd, DRM_IOCTL_XE_EXEC, &exec);
 *
 */
struct drm_xe_exec {
	/** @extensions: Pointer to the first extension struct, if any */
	__u64 extensions;

	/** @exec_queue_id: Exec queue ID for the batch buffer */
	__u32 exec_queue_id;

	/** @num_syncs: Amount of struct drm_xe_sync in array. */
	__u32 num_syncs;

	/** @syncs: Pointer to struct drm_xe_sync array. */
	__u64 syncs;

	/**
	 * @address: address of batch buffer if num_batch_buffer == 1 or an
	 * array of batch buffer addresses
	 */
	__u64 address;

	/**
	 * @num_batch_buffer: number of batch buffer in this exec, must match
	 * the @num_bb_per_exec of the struct drm_xe_exec_queue_create
	 */
	__u16 num_batch_buffer;

	/** @pad: MBZ */
	__u16 pad[3];

	/** @reserved: Reserved */
	__u64 reserved[2];
};

/**
 * struct drm_xe_wait_user_fence - Input of &DRM_IOCTL_XE_WAIT_USER_FENCE
 *
 * Wait on user fence, XE will wake-up on every HW engine interrupt in the
 * instances list and check if user fence is complete::
 *
 *	(*addr & MASK) OP (VALUE & MASK)
 *
 * Returns to user on user fence completion or timeout.
 *
 * The wait @op can be:
 *  - %DRM_XE_UFENCE_WAIT_OP_EQ
 *  - %DRM_XE_UFENCE_WAIT_OP_NEQ
 *  - %DRM_XE_UFENCE_WAIT_OP_GT
 *  - %DRM_XE_UFENCE_WAIT_OP_GTE
 *  - %DRM_XE_UFENCE_WAIT_OP_LT
 *  - %DRM_XE_UFENCE_WAIT_OP_LTE
 *
 * The wait @flags can be:
 *  - %DRM_XE_UFENCE_WAIT_FLAG_SOFT_OP
 *  - %DRM_XE_UFENCE_WAIT_FLAG_ABSTIME
 *
 * The wait @mask can be:
 *  - %DRM_XE_UFENCE_WAIT_MASK_U8
 *  - %DRM_XE_UFENCE_WAIT_MASK_U16
 *  - %DRM_XE_UFENCE_WAIT_MASK_U32
 *  - %DRM_XE_UFENCE_WAIT_MASK_U64
 *
 */
struct drm_xe_wait_user_fence {
	/** @extensions: Pointer to the first extension struct, if any */
	__u64 extensions;

	/**
	 * @addr: user pointer address to wait on, must qword aligned
	 */
	__u64 addr;

#define DRM_XE_UFENCE_WAIT_OP_EQ	0x0
#define DRM_XE_UFENCE_WAIT_OP_NEQ	0x1
#define DRM_XE_UFENCE_WAIT_OP_GT	0x2
#define DRM_XE_UFENCE_WAIT_OP_GTE	0x3
#define DRM_XE_UFENCE_WAIT_OP_LT	0x4
#define DRM_XE_UFENCE_WAIT_OP_LTE	0x5
	/** @op: wait operation (type of comparison) */
	__u16 op;

#define DRM_XE_UFENCE_WAIT_FLAG_ABSTIME	(1 << 0)
	/** @flags: wait flags */
	__u16 flags;

	/** @pad: MBZ */
	__u32 pad;

	/** @value: compare value */
	__u64 value;

#define DRM_XE_UFENCE_WAIT_MASK_U8	0xffu
#define DRM_XE_UFENCE_WAIT_MASK_U16	0xffffu
#define DRM_XE_UFENCE_WAIT_MASK_U32	0xffffffffu
#define DRM_XE_UFENCE_WAIT_MASK_U64	0xffffffffffffffffu
	/** @mask: comparison mask */
	__u64 mask;

	/**
	 * @timeout: how long to wait before bailing, value in nanoseconds.
	 * Without DRM_XE_UFENCE_WAIT_FLAG_ABSTIME flag set (relative timeout)
	 * it contains timeout expressed in nanoseconds to wait (fence will
	 * expire at now() + timeout).
	 * When DRM_XE_UFENCE_WAIT_FLAG_ABSTIME flat is set (absolute timeout) wait
	 * will end at timeout (uses system MONOTONIC_CLOCK).
	 * Passing negative timeout leads to neverending wait.
	 *
	 * On relative timeout this value is updated with timeout left
	 * (for restarting the call in case of signal delivery).
	 * On absolute timeout this value stays intact (restarted call still
	 * expire at the same point of time).
	 */
	__s64 timeout;

	/** @reserved: Reserved */
	__u64 reserved[4];
};

/**
 * DOC: uevent generated by xe on it's pci node.
 *
 * DRM_XE_RESET_FAILED_UEVENT - Event is generated when attempt to reset gt
 * fails. The value supplied with the event is always "NEEDS_RESET".
 * Additional information supplied is tile id and gt id of the gt unit for
 * which reset has failed.
 */
#define DRM_XE_RESET_FAILED_UEVENT "DEVICE_STATUS"

/**
 * DOC: XE PMU event config IDs
 *
 * Check 'man perf_event_open' to use the ID's XE_PMU_XXXX listed in xe_drm.h
 * in 'struct perf_event_attr' as part of perf_event_open syscall to read a
 * particular event.
 *
 * For example to open the DRM_XE_PMU_RENDER_GROUP_BUSY(0):
 *
 * .. code-block:: C
 *
 *	struct perf_event_attr attr;
 *	long long count;
 *	int cpu = 0;
 *	int fd;
 *
 *	memset(&attr, 0, sizeof(struct perf_event_attr));
 *	attr.type = type; // eg: /sys/bus/event_source/devices/xe_0000_56_00.0/type
 *	attr.read_format = PERF_FORMAT_TOTAL_TIME_ENABLED;
 *	attr.use_clockid = 1;
 *	attr.clockid = CLOCK_MONOTONIC;
 *	attr.config = DRM_XE_PMU_RENDER_GROUP_BUSY(0);
 *
 *	fd = syscall(__NR_perf_event_open, &attr, -1, cpu, -1, 0);
 */

/*
 * Top bits of every counter are GT id.
 */
#define __XE_PMU_GT_SHIFT (56)

#define ___XE_PMU_OTHER(gt, x) \
	(((__u64)(x)) | ((__u64)(gt) << __XE_PMU_GT_SHIFT))

#define DRM_XE_PMU_RENDER_GROUP_BUSY(gt)	___XE_PMU_OTHER(gt, 0)
#define DRM_XE_PMU_COPY_GROUP_BUSY(gt)		___XE_PMU_OTHER(gt, 1)
#define DRM_XE_PMU_MEDIA_GROUP_BUSY(gt)		___XE_PMU_OTHER(gt, 2)
#define DRM_XE_PMU_ANY_ENGINE_GROUP_BUSY(gt)	___XE_PMU_OTHER(gt, 3)

#if defined(__cplusplus)
}
#endif

#endif /* _XE_DRM_H_ */
