/* SPDX-License-Identifier: MIT */
/*
 * Copyright © 2021 Intel Corporation
 */

#ifndef __I915_DRM_PRELIM_H__
#define __I915_DRM_PRELIM_H__

#include "i915_drm.h"

/*
 * Modifications to structs/values defined here are subject to
 * backwards-compatibility constraints.
 *
 * Internal/downstream declarations must be added here, not to
 * i915_drm.h. The values in i915_drm_prelim.h must also be kept
 * synchronized with values in i915_drm.h.
 */

struct prelim_i915_uevent {
/*
 * PRELIM_I915_RESET_FAILED_UEVENT - Event is generated when engine or GPU
 *	resets fail and also when GPU is declared wedged. The value
 *	supplied with the event is always 1. Event is also generated when
 *	resets are disabled by module parameter and an attempt to reset
 *	either engine or GPU is made.
 */
#define PRELIM_I915_RESET_FAILED_UEVENT	"RESET_FAILED"

/*
 * PRELIM_I915_MEMORY_HEALTH_UEVENT - Generated when driver receives a memory
 *	degradation error from the GPU FW. The event serves as notification to
 *	an Admin to reboot the system as soon as possible, due to the fact that
 *	device is no longer RUNTIME recoverable again. This event will always
 *	have a value of 1, which indicates that uncorrectable error has been
 *	detected, and that runtime memory sparing is not feasible without system
 *	reboot - for recovery of failed BANK.
 */
#define PRELIM_I915_MEMORY_HEALTH_UEVENT	"MEMORY_HEALTH"
};

struct prelim_i915_user_extension {
#define PRELIM_I915_USER_EXT		(1 << 16)
#define PRELIM_I915_USER_EXT_MASK(x)	(x & 0xffff)
#define PRELIM_I915_CONTEXT_ENGINES_EXT_PARALLEL2_SUBMIT (PRELIM_I915_USER_EXT | 3)
};

/* This API has been removed.  On the off chance someone somewhere has
 * attempted to use it, never re-use this extension number.
 */

#define PRELIM_I915_CONTEXT_CREATE_EXT_CLONE	(PRELIM_I915_USER_EXT | 1)

/*
 * PRELIM UAPI VERSION - /sys/<...>/drm/card<n>/prelim_uapi_version
 * MAJOR - to be incremented right after a major public Production branch
 *         release containing PRELIM uAPIs
 *         PROD_DG1_201210.0 released so starting with major = 2, although
 *         it didn't have the proper prelim api infrastructure yet.
 * MINOR - Reset to 0 when MAJOR is bumped.
 *         Bumped as needed when some kind of API incompatibility is identified.
 *         This patch, which introduces this, should be the only patch in
 *         the pile that is changing this number.
 */
#define PRELIM_UAPI_MAJOR	2
#define PRELIM_UAPI_MINOR	1

/*
 * Top 4 bits of every non-engine counter are GT id.
 */
#define __PRELIM_I915_PMU_GT_SHIFT (60)
#define __PRELIM_I915_PMU_GT_MASK (0xfull << 60)
#define __PRELIM_I915_PMU_FN_SHIFT (44)
#define __PRELIM_I915_PMU_FN_MASK (0xffffull << 44)

/*
 * bits[59:44] are used to specify the function number for the event. Only a PF
 * is allowed to specify function numbers. If there are N functions ranging from
 * 0 through N - 1, the following definitions are used for the function numbers:
 *
 * 0 : PF
 * 1 through (N - 1) : VFs
 *
 * Below macro can be used to convert an existing event config to a function
 * event config.
 */
#define ___PRELIM_I915_PMU_FN_EVENT(event, function) \
	(((event) & ~__PRELIM_I915_PMU_FN_MASK) | \
	 ((function + 1) << __PRELIM_I915_PMU_FN_SHIFT))

#define ___PRELIM_I915_PMU_OTHER(gt, x) \
	(((__u64)__I915_PMU_ENGINE(0xff, 0xff, 0xf) + 1 + (x)) | \
	((__u64)(gt) << __PRELIM_I915_PMU_GT_SHIFT))

#define __I915_PMU_OTHER(x) ___PRELIM_I915_PMU_OTHER(0, x)

#define __PRELIM_I915_PMU_ACTUAL_FREQUENCY(gt)		___PRELIM_I915_PMU_OTHER(gt, 0)
#define __PRELIM_I915_PMU_REQUESTED_FREQUENCY(gt)	___PRELIM_I915_PMU_OTHER(gt, 1)
#define __PRELIM_I915_PMU_INTERRUPTS(gt)		___PRELIM_I915_PMU_OTHER(gt, 2)
#define __PRELIM_I915_PMU_RC6_RESIDENCY(gt)		___PRELIM_I915_PMU_OTHER(gt, 3)
#define __PRELIM_I915_PMU_SOFTWARE_GT_AWAKE_TIME(gt)	___PRELIM_I915_PMU_OTHER(gt, 4)
#define __PRELIM_I915_PMU_ENGINE_RESET_COUNT(gt)	___PRELIM_I915_PMU_OTHER(gt, 5)
#define __PRELIM_I915_PMU_EU_ATTENTION_COUNT(gt)	___PRELIM_I915_PMU_OTHER(gt, 6)
#define __PRELIM_I915_PMU_RENDER_GROUP_BUSY(gt)		___PRELIM_I915_PMU_OTHER(gt, 7)
#define __PRELIM_I915_PMU_COPY_GROUP_BUSY(gt)		___PRELIM_I915_PMU_OTHER(gt, 8)
#define __PRELIM_I915_PMU_MEDIA_GROUP_BUSY(gt)		___PRELIM_I915_PMU_OTHER(gt, 9)
#define __PRELIM_I915_PMU_ANY_ENGINE_GROUP_BUSY(gt)	___PRELIM_I915_PMU_OTHER(gt, 10)
#define __PRELIM_I915_PMU_TOTAL_ACTIVE_TICKS(gt)	___PRELIM_I915_PMU_OTHER(gt, 11)


#define __PRELIM_I915_PMU_HW_ERROR_EVENT_ID_OFFSET	(__I915_PMU_OTHER(0) + 1000)

#define PRELIM_I915_PMU_ENGINE_RESET_COUNT	__PRELIM_I915_PMU_ENGINE_RESET_COUNT(0)
#define PRELIM_I915_PMU_EU_ATTENTION_COUNT	__PRELIM_I915_PMU_EU_ATTENTION_COUNT(0)
#define PRELIM_I915_PMU_RENDER_GROUP_BUSY		__PRELIM_I915_PMU_RENDER_GROUP_BUSY(0)
#define PRELIM_I915_PMU_COPY_GROUP_BUSY			__PRELIM_I915_PMU_COPY_GROUP_BUSY(0)
#define PRELIM_I915_PMU_MEDIA_GROUP_BUSY		__PRELIM_I915_PMU_MEDIA_GROUP_BUSY(0)
#define PRELIM_I915_PMU_ANY_ENGINE_GROUP_BUSY		__PRELIM_I915_PMU_ANY_ENGINE_GROUP_BUSY(0)
#define PRELIM_I915_PMU_TOTAL_ACTIVE_TICKS		__PRELIM_I915_PMU_TOTAL_ACTIVE_TICKS(0)

/*
 * Note that I915_PMU_SAMPLE_BITS is 4 so a max of 16 events can be sampled for
 * an engine. For the PRELIM version start at half of that value.
 */
#define PRELIM_I915_SAMPLE_BUSY_TICKS 8

#define PRELIM_I915_PMU_ENGINE_BUSY_TICKS(class, instance) \
	__I915_PMU_ENGINE(class, instance, PRELIM_I915_SAMPLE_BUSY_TICKS)

#define  PRELIM_I915_SCHEDULER_CAP_ENGINE_BUSY_TICKS_STATS	(1ul << 16)

/*
 * HW error counters.
 */
#define PRELIM_I915_PMU_GT_ERROR_CORRECTABLE_L3_SNG		(0)
#define PRELIM_I915_PMU_GT_ERROR_CORRECTABLE_GUC		(1)
#define PRELIM_I915_PMU_GT_ERROR_CORRECTABLE_SAMPLER		(2)
#define PRELIM_I915_PMU_GT_ERROR_CORRECTABLE_SLM		(3)
#define PRELIM_I915_PMU_GT_ERROR_CORRECTABLE_EU_IC		(4)
#define PRELIM_I915_PMU_GT_ERROR_CORRECTABLE_EU_GRF		(5)
#define PRELIM_I915_PMU_GT_ERROR_FATAL_ARR_BIST			(6)
#define PRELIM_I915_PMU_GT_ERROR_FATAL_L3_DOUB			(7)
#define PRELIM_I915_PMU_GT_ERROR_FATAL_L3_ECC_CHK		(8)
#define PRELIM_I915_PMU_GT_ERROR_FATAL_GUC			(9)
#define PRELIM_I915_PMU_GT_ERROR_FATAL_IDI_PAR			(10)
#define PRELIM_I915_PMU_GT_ERROR_FATAL_SQIDI			(11)
#define PRELIM_I915_PMU_GT_ERROR_FATAL_SAMPLER			(12)
#define PRELIM_I915_PMU_GT_ERROR_FATAL_SLM			(13)
#define PRELIM_I915_PMU_GT_ERROR_FATAL_EU_IC			(14)
#define PRELIM_I915_PMU_GT_ERROR_FATAL_EU_GRF			(15)
#define PRELIM_I915_PMU_SGUNIT_ERROR_CORRECTABLE		(16)
#define PRELIM_I915_PMU_SGUNIT_ERROR_NONFATAL			(17)
#define PRELIM_I915_PMU_SGUNIT_ERROR_FATAL			(18)
#define PRELIM_I915_PMU_SOC_ERROR_FATAL_PSF_CSC_0		(19)
#define PRELIM_I915_PMU_SOC_ERROR_FATAL_PSF_CSC_1		(20)
#define PRELIM_I915_PMU_SOC_ERROR_FATAL_PSF_CSC_2		(21)
#define PRELIM_I915_PMU_SOC_ERROR_FATAL_PUNIT			(22)
#define PRELIM_I915_PMU_SOC_ERROR_FATAL_MDFI_EAST		(23)
#define PRELIM_I915_PMU_SOC_ERROR_FATAL_MDFI_WEST		(24)
#define PRELIM_I915_PMU_SOC_ERROR_FATAL_MDFI_SOUTH		(25)

#define PRELIM_I915_PMU_SOC_ERROR_FATAL_FBR(ss, n) \
	(PRELIM_I915_PMU_SOC_ERROR_FATAL_MDFI_SOUTH + 0x1 + (ss) * 0x4 + (n))

#define PRELIM_I915_PMU_SOC_ERROR_FATAL_HBM(ss, n)\
	(PRELIM_I915_PMU_SOC_ERROR_FATAL_FBR(1, 5) + (ss) * 0x10 + (n))

#define PRELIM_I915_PMU_GT_ERROR_FATAL_FPU			(67)
#define PRELIM_I915_PMU_GT_ERROR_FATAL_TLB			(68)
#define PRELIM_I915_PMU_GT_ERROR_FATAL_L3_FABRIC		(69)
#define PRELIM_I915_PVC_PMU_SOC_ERROR_FATAL_PSF_0		(70)
#define PRELIM_I915_PVC_PMU_SOC_ERROR_FATAL_PSF_1		(71)
#define PRELIM_I915_PVC_PMU_SOC_ERROR_FATAL_PSF_2		(72)
#define PRELIM_I915_PVC_PMU_SOC_ERROR_FATAL_CD0			(73)
#define PRELIM_I915_PVC_PMU_SOC_ERROR_FATAL_CD0_MDFI		(74)
#define PRELIM_I915_PVC_PMU_SOC_ERROR_FATAL_MDFI_EAST		(75)
#define PRELIM_I915_PVC_PMU_SOC_ERROR_FATAL_MDFI_SOUTH		(76)

#define PRELIM_I915_PVC_PMU_SOC_ERROR_FATAL_HBM(ss, n)\
	(PRELIM_I915_PVC_PMU_SOC_ERROR_FATAL_MDFI_SOUTH + 0x1 + (ss) * 0x10 + (n))

/* 108 is the last ID used by SOC errors */
#define PRELIM_I915_PMU_GSC_ERROR_CORRECTABLE_SRAM_ECC          (109)
#define PRELIM_I915_PMU_GSC_ERROR_NONFATAL_MIA_SHUTDOWN         (110)
#define PRELIM_I915_PMU_GSC_ERROR_NONFATAL_MIA_INT              (111)
#define PRELIM_I915_PMU_GSC_ERROR_NONFATAL_SRAM_ECC             (112)
#define PRELIM_I915_PMU_GSC_ERROR_NONFATAL_WDG_TIMEOUT          (113)
#define PRELIM_I915_PMU_GSC_ERROR_NONFATAL_ROM_PARITY           (114)
#define PRELIM_I915_PMU_GSC_ERROR_NONFATAL_UCODE_PARITY         (115)
#define PRELIM_I915_PMU_GSC_ERROR_NONFATAL_GLITCH_DET           (116)
#define PRELIM_I915_PMU_GSC_ERROR_NONFATAL_FUSE_PULL            (117)
#define PRELIM_I915_PMU_GSC_ERROR_NONFATAL_FUSE_CRC_CHECK       (118)
#define PRELIM_I915_PMU_GSC_ERROR_NONFATAL_FUSE_SELFMBIST       (119)
#define PRELIM_I915_PMU_GSC_ERROR_NONFATAL_AON_PARITY           (120)
#define PRELIM_I915_PMU_GT_ERROR_CORRECTABLE_SUBSLICE           (121)
#define PRELIM_I915_PMU_GT_ERROR_CORRECTABLE_L3BANK             (122)
#define PRELIM_I915_PMU_GT_ERROR_FATAL_SUBSLICE                 (123)
#define PRELIM_I915_PMU_GT_ERROR_FATAL_L3BANK                   (124)
#define PRELIM_I915_PVC_PMU_SOC_ERROR_NONFATAL_CD0_MDFI         (125)
#define PRELIM_I915_PVC_PMU_SOC_ERROR_NONFATAL_MDFI_EAST        (126)
#define PRELIM_I915_PVC_PMU_SOC_ERROR_NONFATAL_MDFI_SOUTH       (127)

#define PRELIM_I915_PMU_HW_ERROR(gt, id) \
	((__PRELIM_I915_PMU_HW_ERROR_EVENT_ID_OFFSET + (id)) | \
	((__u64)(gt) << __PRELIM_I915_PMU_GT_SHIFT))

/* Per GT driver error counters */
#define __PRELIM_I915_PMU_GT_DRIVER_ERROR_EVENT_ID_OFFSET (__I915_PMU_OTHER(0) + 2000)

#define PRELIM_I915_PMU_GT_DRIVER_ERROR_GGTT			(0)
#define PRELIM_I915_PMU_GT_DRIVER_ERROR_ENGINE_OTHER		(1)
#define PRELIM_I915_PMU_GT_DRIVER_ERROR_GUC_COMMUNICATION	(2)
#define PRELIM_I915_PMU_GT_DRIVER_ERROR_RPS			(3)
#define PRELIM_I915_PMU_GT_DRIVER_ERROR_GT_OTHER		(4)
#define PRELIM_I915_PMU_GT_DRIVER_ERROR_INTERRUPT		(5)

#define PRELIM_I915_PMU_GT_DRIVER_ERROR(gt, id) \
	((__PRELIM_I915_PMU_GT_DRIVER_ERROR_EVENT_ID_OFFSET + (id)) | \
	 ((__u64)(gt) << __PRELIM_I915_PMU_GT_SHIFT))

/* Global driver error counters */
#define __PRELIM_I915_PMU_DRIVER_ERROR_EVENT_ID_OFFSET (__I915_PMU_OTHER(0) + 3000)
#define PRELIM_I915_PMU_DRIVER_ERROR_OBJECT_MIGRATION	(0)
#define PRELIM_I915_PMU_DRIVER_ERROR(id)	(__PRELIM_I915_PMU_DRIVER_ERROR_EVENT_ID_OFFSET + (id))

/* PRELIM ioctl's */

/* PRELIM ioctl numbers go down from 0x5f */
#define PRELIM_DRM_I915_RESERVED_FOR_VERSION	0x5f
/* 0x5e is free, please use if needed */
#define PRELIM_DRM_I915_GEM_VM_BIND		0x5d
#define PRELIM_DRM_I915_GEM_VM_UNBIND		0x5c
#define PRELIM_DRM_I915_GEM_VM_ADVISE		0x5b
#define PRELIM_DRM_I915_GEM_WAIT_USER_FENCE	0x5a
#define PRELIM_DRM_I915_GEM_VM_PREFETCH		0x59
#define PRELIM_DRM_I915_UUID_REGISTER		0x58
#define PRELIM_DRM_I915_UUID_UNREGISTER		0x57
#define PRELIM_DRM_I915_DEBUGGER_OPEN		0x56
#define PRELIM_DRM_I915_GEM_CLOS_RESERVE	0x55
#define PRELIM_DRM_I915_GEM_CLOS_FREE		0x54
#define PRELIM_DRM_I915_GEM_CACHE_RESERVE	0x53
#define PRELIM_DRM_I915_GEM_VM_GETPARAM		DRM_I915_GEM_CONTEXT_GETPARAM
#define PRELIM_DRM_I915_GEM_VM_SETPARAM		DRM_I915_GEM_CONTEXT_SETPARAM
#define PRELIM_DRM_I915_GEM_OBJECT_SETPARAM	DRM_I915_GEM_CONTEXT_SETPARAM


#define PRELIM_DRM_IOCTL_I915_GEM_CREATE_EXT		DRM_IOWR(DRM_COMMAND_BASE + DRM_I915_GEM_CREATE, struct prelim_drm_i915_gem_create_ext)
#define PRELIM_DRM_IOCTL_I915_GEM_VM_BIND		DRM_IOWR(DRM_COMMAND_BASE + PRELIM_DRM_I915_GEM_VM_BIND, struct prelim_drm_i915_gem_vm_bind)
#define PRELIM_DRM_IOCTL_I915_GEM_VM_UNBIND		DRM_IOWR(DRM_COMMAND_BASE + PRELIM_DRM_I915_GEM_VM_UNBIND, struct prelim_drm_i915_gem_vm_bind)
#define PRELIM_DRM_IOCTL_I915_GEM_VM_ADVISE		DRM_IOWR(DRM_COMMAND_BASE + PRELIM_DRM_I915_GEM_VM_ADVISE, struct prelim_drm_i915_gem_vm_advise)
#define PRELIM_DRM_IOCTL_I915_GEM_WAIT_USER_FENCE	DRM_IOWR(DRM_COMMAND_BASE + PRELIM_DRM_I915_GEM_WAIT_USER_FENCE, struct prelim_drm_i915_gem_wait_user_fence)
#define PRELIM_DRM_IOCTL_I915_GEM_VM_PREFETCH		DRM_IOWR(DRM_COMMAND_BASE + PRELIM_DRM_I915_GEM_VM_PREFETCH, struct prelim_drm_i915_gem_vm_prefetch)
#define PRELIM_DRM_IOCTL_I915_UUID_REGISTER		DRM_IOWR(DRM_COMMAND_BASE + PRELIM_DRM_I915_UUID_REGISTER, struct prelim_drm_i915_uuid_control)
#define PRELIM_DRM_IOCTL_I915_UUID_UNREGISTER		DRM_IOWR(DRM_COMMAND_BASE + PRELIM_DRM_I915_UUID_UNREGISTER, struct prelim_drm_i915_uuid_control)
#define PRELIM_DRM_IOCTL_I915_DEBUGGER_OPEN		DRM_IOWR(DRM_COMMAND_BASE + PRELIM_DRM_I915_DEBUGGER_OPEN, struct prelim_drm_i915_debugger_open_param)
#define PRELIM_DRM_IOCTL_I915_GEM_CLOS_RESERVE		DRM_IOWR(DRM_COMMAND_BASE + PRELIM_DRM_I915_GEM_CLOS_RESERVE, struct prelim_drm_i915_gem_clos_reserve)
#define PRELIM_DRM_IOCTL_I915_GEM_CLOS_FREE		DRM_IOWR(DRM_COMMAND_BASE + PRELIM_DRM_I915_GEM_CLOS_FREE, struct prelim_drm_i915_gem_clos_free)
#define PRELIM_DRM_IOCTL_I915_GEM_CACHE_RESERVE		DRM_IOWR(DRM_COMMAND_BASE + PRELIM_DRM_I915_GEM_CACHE_RESERVE, struct prelim_drm_i915_gem_cache_reserve)
#define PRELIM_DRM_IOCTL_I915_GEM_VM_GETPARAM		DRM_IOWR(DRM_COMMAND_BASE + PRELIM_DRM_I915_GEM_VM_GETPARAM, struct prelim_drm_i915_gem_vm_param)
#define PRELIM_DRM_IOCTL_I915_GEM_VM_SETPARAM		DRM_IOWR(DRM_COMMAND_BASE + PRELIM_DRM_I915_GEM_VM_SETPARAM, struct prelim_drm_i915_gem_vm_param)
#define PRELIM_DRM_IOCTL_I915_GEM_OBJECT_SETPARAM	DRM_IOWR(DRM_COMMAND_BASE + PRELIM_DRM_I915_GEM_OBJECT_SETPARAM, struct prelim_drm_i915_gem_object_param)

/* End PRELIM ioctl's */

/* getparam */
#define PRELIM_I915_PARAM               (1 << 16)
/*
 * Querying I915_PARAM_EXECBUF2_MAX_ENGINE will return the number of context
 * map engines addressable via the low bits of execbuf2 flags, or in the
 * cases where the parameter is not supported (-EINVAL), legacy maximum of
 * 64 engines should be assumed.
 */
#define PRELIM_I915_PARAM_EXECBUF2_MAX_ENGINE	(PRELIM_I915_PARAM | 1)

/* Total local memory in bytes */
#define PRELIM_I915_PARAM_LMEM_TOTAL_BYTES	(PRELIM_I915_PARAM | 2)

/* Available local memory in bytes */
#define PRELIM_I915_PARAM_LMEM_AVAIL_BYTES	(PRELIM_I915_PARAM | 3)

/* Shared Virtual Memory (SVM) support capability */
#define PRELIM_I915_PARAM_HAS_SVM		(PRELIM_I915_PARAM | 4)

/*
 * Frequency of the timestamps in OA reports. This used to be the same as the CS
 * timestamp frequency, but differs on some platforms.
 */
#define PRELIM_I915_PARAM_OA_TIMESTAMP_FREQUENCY	(PRELIM_I915_PARAM | 5)

/* VM_BIND feature availability */
#define PRELIM_I915_PARAM_HAS_VM_BIND	(PRELIM_I915_PARAM | 6)

/* Recoverable pagefault support */
#define PRELIM_I915_PARAM_HAS_PAGE_FAULT	(PRELIM_I915_PARAM | 7)

/* Implicit scale support */
#define PRELIM_I915_PARAM_HAS_SET_PAIR	(PRELIM_I915_PARAM | 8)

/* EU Debugger support */
#define PRELIM_I915_PARAM_EU_DEBUGGER_VERSION  (PRELIM_I915_PARAM | 9)

/* BO chunk granularity support */
#define PRELIM_I915_PARAM_HAS_CHUNK_SIZE	(PRELIM_I915_PARAM | 10)
/* End getparam */

/*
 * On large systems, there may be different classes of memory available (NUMA),
 * each with different performance characteristics and distances from the
 * individual cores or devices. Using a precise NUMA node for a particular
 * system buffer can have a significant performance advantage, and which
 * memory region to use depends on the use case. For example, infrequently used
 * objects can be stored in DDR whereas the frequently used objects should be
 * in HBM, and in both cases want to be in a memory node closed to the core or
 * device predominating generating the most accesses (at least with respect to
 * the latency sensitive critical path of the workload).
 *
 * To accommodate the varying requirements of buffer placement in NUMA system
 * memory, we allow passing both preferred node and allowed nodes in a similar
 * manner to the NUMA mempolicy (numactl --membind), see mbind(2), used to
 * govern memory allocation within a process. This policy only applies to
 * system memory placements.
 *
 * If no user mempolicy is supplied, by default we will allocate from the memory
 * node closest to the device (likely DDR). This is the same as using
 * MPOL_DEFAULT.
 *
 * If the policy is set to MPOL_PREFERRED, an attempt is made to allocate
 * from the specified nodes, before trying the default node associated with
 * the GPU device.
 *
 * If the policy is set to MPOL_BIND, similar to MPOL_PREFFERED, the nodemask
 * is used to pick which nodes to allocate from, but instead of allowing a
 * fallback to the default node, it will fail.
 *
 * If the policy is set to MPOL_LOCAL, allocation will be performed local to
 * the CPU, using the closest memory, rather than placing the memory close to
 * the GPU device.
 *
 * If we fail to allocate from the preferred node, the sysfs error_counter
 * numa%d_allocation is incremented.
 *
 * The policy is only applied for the first access of the object in system
 * memory. If the object is evicted from its preferred node, due to swapping,
 * subsequent access to that object will use the system's default allocation
 * strategy.
 */
struct prelim_drm_i915_gem_create_ext_memory_policy {
	/* .name = PRELIM_I915_GEM_CREATE_EXT_MEMORY_POLICY */
	struct i915_user_extension base;

	/* Memory policy; how to pick which numa node for individual chunk allocations */
	__u32 mode;
#define I915_GEM_CREATE_MPOL_DEFAULT		0
#define I915_GEM_CREATE_MPOL_PREFERRED		1
#define I915_GEM_CREATE_MPOL_BIND		2
#define I915_GEM_CREATE_MPOL_INTERLEAVED	3
#define I915_GEM_CREATE_MPOL_LOCAL		4
#define I915_GEM_CREATE_MPOL_PREFERRED_MANY	5 /* not implemented */

	/* Placehoder for future flags; must be zero */
	__u32 flags;

	/* Exclusive maximum of the node ids; the limit of the nodemask */
	__u32 nodemask_max;

	/*
	 * Pointer to a bitmask of the numa nodes to use for allocation.
	 * Size is derived from [0, nodemask_max).
	 */
	__u64 nodemask_ptr;
};

struct prelim_drm_i915_gem_create_ext {

	/**
	 * Requested size for the object.
	 *
	 * The (page-aligned) allocated size for the object will be returned.
	 */
	__u64 size;
	/**
	 * Returned handle for the object.
	 *
	 * Object handles are nonzero.
	 */
	__u32 handle;
	__u32 pad;

#define PRELIM_I915_GEM_CREATE_EXT_SETPARAM		(PRELIM_I915_USER_EXT | 1)
#define PRELIM_I915_GEM_CREATE_EXT_PROTECTED_CONTENT   (PRELIM_I915_USER_EXT | 2)
#define PRELIM_I915_GEM_CREATE_EXT_VM_PRIVATE		(PRELIM_I915_USER_EXT | 3)
#define PRELIM_I915_GEM_CREATE_EXT_MEMORY_POLICY	(PRELIM_I915_USER_EXT | 4)
	__u64 extensions;
};

struct prelim_drm_i915_gem_object_param {
	/* Object handle (0 for I915_GEM_CREATE_EXT_SETPARAM) */
	__u32 handle;

	/* Data pointer size */
	__u32 size;

/*
 * PRELIM_I915_OBJECT_PARAM:
 *
 * Select object namespace for the param.
 */
#define PRELIM_I915_OBJECT_PARAM  (1ull << 48)

/*
 * PRELIM_I915_PARAM_MEMORY_REGIONS:
 *
 * Set the data pointer with the desired set of placements in priority
 * order(each entry must be unique and supported by the device), as an array of
 * prelim_drm_i915_gem_memory_class_instance, or an equivalent layout of class:instance
 * pair encodings. See PRELIM_DRM_I915_QUERY_MEMORY_REGIONS for how to query the
 * supported regions.
 *
 * Note that this requires the PRELIM_I915_OBJECT_PARAM namespace:
 *	.param = PRELIM_I915_OBJECT_PARAM | PRELIM_I915_PARAM_MEMORY_REGIONS
 */
#define PRELIM_I915_PARAM_MEMORY_REGIONS ((1 << 16) | 0x1)

/*
 * PRELIM_I915_PARAM_SET_PAIR:
 *
 * Allows a "paired" buffer object to be specified to allow implicit scaling to
 * use two buffer objects with a single exported dma-buf file descriptor
 */
#define PRELIM_I915_PARAM_SET_PAIR ((1 << 17) | 0x1)

/*
 * PRELIM_I915_PARAM_SET_CHUNK_SIZE:
 *
 * Specifies that this buffer object should support 'chunking' and chunk
 * granularity. Allows internal KMD paging/migration/eviction handling to
 * operate on a single chunk instead of the whole buffer object. Size is
 * specified in bytes. KMD will return error (-ENOSPC) if CHUNK_SIZE is
 * smaller than 64 KiB or (-EINVAL) if not aligned to 64 KiB.
 */
#define PRELIM_I915_PARAM_SET_CHUNK_SIZE ((1 << 18) | 1)
	__u64 param;

	/* Data value or pointer */
	__u64 data;
};

struct prelim_drm_i915_gem_create_ext_setparam {
	struct i915_user_extension base;
	struct prelim_drm_i915_gem_object_param param;
};

struct prelim_drm_i915_gem_create_ext_vm_private {
	/** @base: Extension link. See struct i915_user_extension. */
	struct i915_user_extension base;
	/** @vm_id: Id of the VM to which Object is private */
	__u32 vm_id;
};

#define PRELIM_PERF_VERSION	(1000)

/**
 * Returns OA buffer properties to be used with mmap.
 *
 * This ioctl is available in perf revision 1000.
 */
#define PRELIM_I915_PERF_IOCTL_GET_OA_BUFFER_INFO _IOWR('i', 0x80, struct prelim_drm_i915_perf_oa_buffer_info)

/**
 * OA buffer size and offset.
 *
 * OA output buffer
 *   type: 0
 *   flags: mbz
 *
 *   After querying the info, pass (size,offset) to mmap(),
 *
 *   mmap(0, info.size, PROT_READ, MAP_PRIVATE, perf_fd, info.offset).
 *
 *   Note that only a private (not shared between processes, or across fork())
 *   read-only mmapping is allowed.
 *
 *   Userspace must treat the incoming data as tainted, but it conforms to the OA
 *   format as specified by user config. The buffer provides reports that have
 *   OA counters - A, B and C.
 */
struct prelim_drm_i915_perf_oa_buffer_info {
	__u32 type;   /* in */
	__u32 flags;  /* in */
	__u64 size;   /* out */
	__u64 offset; /* out */
	__u64 rsvd;   /* mbz */
};

struct prelim_drm_i915_gem_mmap_offset {
	/* Specific MMAP offset for PCI memory barrier */
#define PRELIM_I915_PCI_BARRIER_MMAP_OFFSET (0x50 << PAGE_SHIFT)
};

enum prelim_drm_i915_eu_stall_property_id {
	/**
	 * This field specifies the Per DSS Memory Buffer Size.
	 * Valid values are 128 KB, 256 KB, and 512 KB.
	 */
	PRELIM_DRM_I915_EU_STALL_PROP_BUF_SZ = 1001,

	/**
	 * This field specifies the sampling rate per tile
	 * in multiples of 251 cycles. Valid values are 1 to 7.
	 * If the value is 1, sampling interval is 251 cycles.
	 * If the value is 7, sampling interval is 7 x 251 cycles.
	 */
	PRELIM_DRM_I915_EU_STALL_PROP_SAMPLE_RATE,

	/**
	 * This field specifies the EU stall data poll period
	 * in nanoseconds. Minimum allowed value is 100 ms.
	 * A default value is used by the driver if this field
	 * is not specified.
	 */
	PRELIM_DRM_I915_EU_STALL_PROP_POLL_PERIOD,

	PRELIM_DRM_I915_EU_STALL_PROP_ENGINE_CLASS,

	PRELIM_DRM_I915_EU_STALL_PROP_ENGINE_INSTANCE,

	/**
	 * This field specifies the minimum number of
	 * EU stall data rows to be present in the kernel
	 * buffer for poll() to set POLLIN (data present).
	 * A default value of 1 is used by the driver if this
	 * field is not specified.
	 */
	PRELIM_DRM_I915_EU_STALL_PROP_EVENT_REPORT_COUNT,

	PRELIM_DRM_I915_EU_STALL_PROP_MAX
};

/*
 * Info that the driver adds to each entry in the EU stall counters data.
 */
struct prelim_drm_i915_stall_cntr_info {
	__u16 subslice;
	__u16 flags;
/* EU stall data line dropped due to memory buffer being full */
#define PRELIM_I915_EUSTALL_FLAG_OVERFLOW_DROP	(1 << 8)
};

struct prelim_drm_i915_perf_open_param {
	/* PRELIM flags */
#define PRELIM_I915_PERF_FLAG_FD_EU_STALL	(1 << 16)
};

struct prelim_drm_i915_gem_memory_class_instance {
	__u16 memory_class; /* see enum prelim_drm_i915_gem_memory_class */
	__u16 memory_instance;
};

struct prelim_drm_i915_query_item {
#define PRELIM_DRM_I915_QUERY			(1 << 16)
#define PRELIM_DRM_I915_QUERY_MASK(x)		(x & 0xffff)
#define PRELIM_DRM_I915_QUERY_MEMORY_REGIONS	(PRELIM_DRM_I915_QUERY | 4)
#define PRELIM_DRM_I915_QUERY_DISTANCE_INFO	(PRELIM_DRM_I915_QUERY | 5)
/* Deprecated: HWConfig is now upstream, do not use the prelim version anymore */
#define PRELIM_DRM_I915_QUERY_HWCONFIG_TABLE	(PRELIM_DRM_I915_QUERY | 6)
	/**
	 * Query Geometry Subslices: returns the items found in query_topology info
	 * with a mask for geometry_subslice_mask applied
	 *
	 * @flags:
	 *
	 * bits 0:7 must be a valid engine class and bits 8:15 must be a valid engine
	 * instance.
	 */
#define PRELIM_DRM_I915_QUERY_GEOMETRY_SUBSLICES	(PRELIM_DRM_I915_QUERY | 7)
	/**
	 * Query Compute Subslices: returns the items found in query_topology info
	 * with a mask for compute_subslice_mask applied
	 *
	 * @flags:
	 *
	 * bits 0:7 must be a valid engine class and bits 8:15 must be a valid engine
	 * instance.
	 */
#define PRELIM_DRM_I915_QUERY_COMPUTE_SUBSLICES		(PRELIM_DRM_I915_QUERY | 8)
	/**
	 * Query Command Streamer timestamp register.
	 */
#define PRELIM_DRM_I915_QUERY_CS_CYCLES			(PRELIM_DRM_I915_QUERY | 9)
#define PRELIM_DRM_I915_QUERY_FABRIC_INFO		(PRELIM_DRM_I915_QUERY | 11)
#define PRELIM_DRM_I915_QUERY_HW_IP_VERSION		(PRELIM_DRM_I915_QUERY | 12)
#define PRELIM_DRM_I915_QUERY_ENGINE_INFO		(PRELIM_DRM_I915_QUERY | 13)
#define PRELIM_DRM_I915_QUERY_L3BANK_COUNT		(PRELIM_DRM_I915_QUERY | 14)
#define PRELIM_DRM_I915_QUERY_LMEM_MEMORY_REGIONS	(PRELIM_DRM_I915_QUERY | 15)
};

/*
 * In XEHPSDV total number of engines can be more than the maximum supported
 * engines by I915_EXEC_RING_MASK.
 * PRELIM_I915_EXEC_ENGINE_MASK expands the total number of engines from 64 to 256.
 *
 * To use PRELIM_I915_EXEC_ENGINE_MASK, userspace needs to query
 * I915_PARAM_EXECBUF2_MAX_ENGINE. On getting valid value, userspace needs
 * to set PRELIM_I915_EXEC_ENGINE_MASK_SELECT to enable PRELIM_I915_EXEC_ENGINE_MASK.
 *
 * Bitfield associated with legacy I915_EXEC_CONSTANTS_MASK which was
 * restricted previously, will be utilized by PRELIM_I915_EXEC_ENGINE_MASK.
 *
 * PRELIM_I915_EXEC_ENGINE_MASK only applies to contexts with engine map set up.
 */
#define PRELIM_I915_EXEC_ENGINE_MASK    (0xff)
#define PRELIM_I915_EXEC_ENGINE_MASK_SELECT (1ull << 55)

#define __PRELIM_I915_EXEC_UNKNOWN_FLAGS (~(GENMASK_ULL(55, 48) | ~__I915_EXEC_UNKNOWN_FLAGS))

/*
 * Indicates the 2k user priority levels are statically mapped into 3 buckets as
 * follows:
 *
 * -1k to -1	Low priority
 * 0		Normal priority
 * 1 to 1k	Highest priority
 */
#define   PRELIM_I915_SCHEDULER_CAP_STATIC_PRIORITY_MAP	(1ul << 31)

enum prelim_drm_i915_gem_engine_class {
#define	PRELIM_I915_ENGINE_CLASS		(1 << 8)
#define	PRELIM_I915_ENGINE_CLASS_MASK(x)	(x & 0xff)

	PRELIM_I915_ENGINE_CLASS_COMPUTE = 4,
};

struct prelim_i915_context_param_engines {
#define PRELIM_I915_CONTEXT_ENGINES_EXT_PARALLEL_SUBMIT (PRELIM_I915_USER_EXT | 2) /* see prelim_i915_context_engines_parallel_submit */
};

/* PRELIM OA formats */
enum prelim_drm_i915_oa_format {
	PRELIM_I915_OA_FORMAT_START = 128,

	/* XEHPSDV */
	PRELIM_I915_OAR_FORMAT_A32u40_A4u32_B8_C8 = PRELIM_I915_OA_FORMAT_START,
	PRELIM_I915_OA_FORMAT_A24u40_A14u32_B8_C8,
	PRELIM_I915_OAM_FORMAT_A2u64_B8_C8,

	/* DG2 */
	PRELIM_I915_OAR_FORMAT_A36u64_B8_C8,
	PRELIM_I915_OAC_FORMAT_A24u64_B8_C8,
	PRELIM_I915_OA_FORMAT_A38u64_R2u64_B8_C8,
	PRELIM_I915_OAM_FORMAT_A2u64_R2u64_B8_C8,
	PRELIM_I915_OAC_FORMAT_A22u32_R2u32_B8_C8,

	/* MTL */
	PRELIM_I915_OAM_FORMAT_MPEC8u64_B8_C8,
	PRELIM_I915_OAM_FORMAT_MPEC8u32_B8_C8,

	PRELIM_I915_OA_FORMAT_MAX	/* non-ABI */
};

enum prelim_drm_i915_perf_record_type {
#define PRELIM_DRM_I915_PERF_RECORD	(1 << 16)
	/*
	 * MMIO trigger queue is full.
	 * This record type is available in perf revision 1003.
	 */
	PRELIM_DRM_I915_PERF_RECORD_OA_MMIO_TRG_Q_FULL = (PRELIM_DRM_I915_PERF_RECORD | 1),
};

/*
 * Access Counter programming
 *
 * The programmable access counters enable hardware to detect and report
 * frequently accessed pages. The report generated by hardware can be used by
 * software for influencing page migration and data placement decisions.
 *
 * Once the count reaches the value set by trigger, HW generates trigger
 * interrupt. DRM driver then starts the page migration from SMEM to
 * LMEM so the upcoming access to the same page(s) from GPU will access LMEM
 * to achive better performance.
 *
 * Due to the HW capacity limitation, an access counter can be de-allocated on
 * the fly. If the counter getting de-allocated has reached at least notify
 * it is reported to SW via interrupt. The driver interrupt handling is TBD.
 *
 * The use case is to let the upper layer SW such as Open CL to make the
 * decision to program all the configurations and the DRM driver will handle
 * the interrupts generated by HW.
 *
 * NOTE: if ac_notify is set to 0, access counter notification reporting is disabled
 *       if ac_trigger is set to 0, access counter triggering is disabled.
 *
 *	Only allowed in i915_gem_context_create_ioctl extension
 */
struct prelim_drm_i915_gem_context_param_acc {
		__u16 trigger;
		__u16 notify;
		__u8  granularity;
#define   PRELIM_I915_CONTEXT_ACG_128K      0
#define   PRELIM_I915_CONTEXT_ACG_2M        1
#define   PRELIM_I915_CONTEXT_ACG_16M       2
#define   PRELIM_I915_CONTEXT_ACG_64M       3
		__u8 pad1;
		__u16 pad2;
};

struct prelim_drm_i915_gem_context_param {
/*
 * I915_CONTEXT_PARAM_DEBUG_FLAGS
 *
 * Set or clear debug flags associated with this context.
 * The flags works with 32 bit masking to enable/disable individual
 * flags. For example to set debug flag of bit position 0, the
 * value needs to be 0x0000000100000001, and to clear flag of
 * bit position 0, the value needs to be 0x0000000100000000.
 *
 */
#define PRELIM_I915_CONTEXT_PARAM		(1 << 16)
#define PRELIM_I915_CONTEXT_PARAM_DEBUG_FLAGS	(PRELIM_I915_CONTEXT_PARAM | 0xfd)

/*
 * Notify driver that SIP is provided with the pipeline setup.
 * Driver raises exception on hang resolution and waits for pipeline's
 * sip to signal attention before capturing state of user objects
 * associated with the context.
 *
 */
#define PRELIM_I915_CONTEXT_PARAM_DEBUG_FLAG_SIP	(1ull << 0)

/*
 *  PRELIM_I915_CONTEXT_PARAM_ACC:
 *
 *  To be able to change the access counter thresholds and configurations.
 *
 *  By default: access counter feature is disabled.
 */
#define PRELIM_I915_CONTEXT_PARAM_ACC		(PRELIM_I915_CONTEXT_PARAM | 0xd)
};

/*
 * I915_CONTEXT_PARAM_PROTECTED_CONTENT:
 *
 * Mark that the context makes use of protected content, which will result
 * in the context being invalidated when the protected content session is.
 * This flag can only be set at context creation time and, when set to true,
 * must be preceded by an explicit setting of I915_CONTEXT_PARAM_RECOVERABLE
 * to false. This flag can't be set to true in conjunction with setting the
 * I915_CONTEXT_PARAM_BANNABLE flag to false.
 *
 * Given the numerous restriction on this flag, there are several unique
 * failure cases:
 *
 * -ENODEV: feature not available
 * -EPERM: trying to mark a recoverable or not bannable context as protected
 */
#define PRELIM_I915_CONTEXT_PARAM_PROTECTED_CONTENT (PRELIM_I915_CONTEXT_PARAM | 0xe)

struct prelim_drm_i915_gem_context_create_ext {
/* Depricated in favor of PRELIM_I915_CONTEXT_CREATE_FLAGS_LONG_RUNNING */
#define PRELIM_I915_CONTEXT_CREATE_FLAGS_ULLS		(1u << 31)
#define PRELIM_I915_CONTEXT_CREATE_FLAGS_LONG_RUNNING	(1u << 31)
#define PRELIM_I915_CONTEXT_CREATE_FLAGS_UNKNOWN			\
	(~(PRELIM_I915_CONTEXT_CREATE_FLAGS_LONG_RUNNING | ~I915_CONTEXT_CREATE_FLAGS_UNKNOWN))
};

/*
 *  PRELIM_I915_CONTEXT_PARAM_RUNALONE:
 *
 *  Enable runalone mode on a context, disabled by default.
 */
#define PRELIM_I915_CONTEXT_PARAM_RUNALONE      (PRELIM_I915_CONTEXT_PARAM | 0xf)

/* Downstream PRELIM properties */
enum prelim_drm_i915_perf_property_id {
	PRELIM_DRM_I915_PERF_PROP = (1 << 16),

	/**
	 * Specify a global OA buffer size to be allocated in bytes. The size
	 * specified must be supported by HW (before XEHPSDV supported sizes are
	 * powers of 2 ranging from 128Kb to 16Mb. With XEHPSDV max supported size
	 * is 128Mb).
	 *
	 * This property is available in perf revision 1001.
	 */
	PRELIM_DRM_I915_PERF_PROP_OA_BUFFER_SIZE = (PRELIM_DRM_I915_PERF_PROP | 1),

	/**
	 * Specify the engine class defined in @enum drm_i915_gem_engine_class.
	 * This defaults to I915_ENGINE_CLASS_RENDER or
	 * I915_ENGINE_CLASS_COMPUTE based on the platform.
	 *
	 * This property is available in perf revision 1002
	 *
	 * Perf revision 1004 supports I915_ENGINE_CLASS_VIDEO and
	 * I915_ENGINE_CLASS_VIDEO_ENHANCE.
	 */
	PRELIM_DRM_I915_PERF_PROP_OA_ENGINE_CLASS = (PRELIM_DRM_I915_PERF_PROP | 2),

	/**
	 * Specify the engine instance. Defaults to 0.
	 *
	 * This property is available in perf revision 1002.
	 */
	PRELIM_DRM_I915_PERF_PROP_OA_ENGINE_INSTANCE = (PRELIM_DRM_I915_PERF_PROP | 3),

	/**
	 * Specify the number of reports that the driver will wait for before
	 * returning data to the user. This value must be less than the number
	 * of reports that the OA buffer can hold.
	 *
	 * This property is available in perf revision 1008.
	 */
	PRELIM_DRM_I915_PERF_PROP_OA_NOTIFY_NUM_REPORTS = (PRELIM_DRM_I915_PERF_PROP | 4),

	PRELIM_DRM_I915_PERF_PROP_LAST,

	PRELIM_DRM_I915_PERF_PROP_MAX = DRM_I915_PERF_PROP_MAX - 1 + \
					(PRELIM_DRM_I915_PERF_PROP_LAST & 0xffff)
};

struct prelim_drm_i915_uuid_control {
	char  uuid[36]; /* String formatted like
			 *      "%08x-%04x-%04x-%04x-%012x"
			 */

	__u32 uuid_class; /* Predefined UUID class or handle to
			   * the previously registered UUID Class
			   */

	__u32 flags;	/* MBZ */

	__u64 ptr;	/* Pointer to CPU memory payload associated
			 * with the UUID Resource.
			 * For uuid_class I915_UUID_CLASS_STRING
			 * it must point to valid string buffer.
			 * Otherwise must point to page aligned buffer
			 * or be NULL.
			 */

	__u64 size;	/* Length of the payload in bytes */

#define PRELIM_I915_UUID_CLASS_STRING	((__u32)-1)
/*
 * d9900de4-be09-56ab-84a5-dfc280f52ee5 =
 *                          sha1(“I915_UUID_CLASS_STRING”)[0..35]
 */
#define PRELIM_I915_UUID_CLASS_MAX_RESERVED ((__u32)-1024)

	__u32 handle; /* Output: Registered handle ID */

	__u64 extensions; /* MBZ */
};

/*
 * struct prelim_drm_i915_vm_bind_ext_uuid
 *
 * Used for registering metadata that will be attached to the vm
 */
struct prelim_drm_i915_vm_bind_ext_uuid {
#define PRELIM_I915_VM_BIND_EXT_UUID	(PRELIM_I915_USER_EXT | 1)
	struct i915_user_extension base;
	__u32 uuid_handle; /* Handle to the registered UUID resource. */
};

/**
 * Do a debug event read for a debugger connection.
 *
 * This ioctl is available in debug version 1.
 */
#define PRELIM_I915_DEBUG_IOCTL_READ_EVENT _IO('j', 0x0)
#define PRELIM_I915_DEBUG_IOCTL_READ_UUID  _IOWR('j', 0x1, struct prelim_drm_i915_debug_read_uuid)
#define PRELIM_I915_DEBUG_IOCTL_VM_OPEN  _IOW('j', 0x2, struct prelim_drm_i915_debug_vm_open)
#define PRELIM_I915_DEBUG_IOCTL_EU_CONTROL _IOWR('j', 0x3, struct prelim_drm_i915_debug_eu_control)
#define PRELIM_I915_DEBUG_IOCTL_ACK_EVENT _IOW('j', 0x4, struct prelim_drm_i915_debug_event_ack)

struct prelim_drm_i915_debug_event {
	__u32 type;
#define PRELIM_DRM_I915_DEBUG_EVENT_NONE     0
#define PRELIM_DRM_I915_DEBUG_EVENT_READ     1
#define PRELIM_DRM_I915_DEBUG_EVENT_CLIENT   2
#define PRELIM_DRM_I915_DEBUG_EVENT_CONTEXT  3
#define PRELIM_DRM_I915_DEBUG_EVENT_UUID     4
#define PRELIM_DRM_I915_DEBUG_EVENT_VM       5
#define PRELIM_DRM_I915_DEBUG_EVENT_VM_BIND  6
#define PRELIM_DRM_I915_DEBUG_EVENT_CONTEXT_PARAM 7
#define PRELIM_DRM_I915_DEBUG_EVENT_EU_ATTENTION 8
#define PRELIM_DRM_I915_DEBUG_EVENT_ENGINES 9
#define PRELIM_DRM_I915_DEBUG_EVENT_PAGE_FAULT 10
#define PRELIM_DRM_I915_DEBUG_EVENT_MAX_EVENT PRELIM_DRM_I915_DEBUG_EVENT_PAGE_FAULT

	__u32 flags;
#define PRELIM_DRM_I915_DEBUG_EVENT_CREATE	(1 << 31)
#define PRELIM_DRM_I915_DEBUG_EVENT_DESTROY	(1 << 30)
#define PRELIM_DRM_I915_DEBUG_EVENT_STATE_CHANGE (1 << 29)
#define PRELIM_DRM_I915_DEBUG_EVENT_NEED_ACK	(1 << 28)
	__u64 seqno;
	__u64 size;
} __attribute__((packed));

struct prelim_drm_i915_debug_event_client {
	struct prelim_drm_i915_debug_event base; /* .flags = CREATE/DESTROY */

	__u64 handle; /* This is unique per debug connection */
} __attribute__((packed));

struct prelim_drm_i915_debug_event_context {
	struct prelim_drm_i915_debug_event base;

	__u64 client_handle;
	__u64 handle;
} __attribute__((packed));

/*
 * Debugger ABI (ioctl and events) Version History:
 * 0 - No debugger available
 * 1 - Initial version
 * 2 - Events sent from a small fifo queue
 * 3 - VM_BIND ioctl is non-blocking wrt to the debugger ack
 */
#define PRELIM_DRM_I915_DEBUG_VERSION 3

struct prelim_drm_i915_debugger_open_param {
	__u64 pid; /* input: Target process ID */
	__u32 flags;
#define PRELIM_DRM_I915_DEBUG_FLAG_FD_NONBLOCK	(1u << 31)

	__u32 version; /* output: current ABI (ioctl / events) version */
	__u64 events;  /* input: event types to subscribe to */
	__u64 extensions; /* MBZ */
};

struct prelim_drm_i915_debug_event_uuid {
	struct prelim_drm_i915_debug_event base;
	__u64 client_handle;

	__u64 handle;
	__u64 class_handle; /* Can be filtered based on pre-defined classes */
	__u64 payload_size;
} __attribute__((packed));

struct prelim_drm_i915_debug_event_vm {
	struct prelim_drm_i915_debug_event base;
	__u64 client_handle;

	__u64 handle;
} __attribute__((packed));

struct prelim_drm_i915_debug_event_vm_bind {
	struct prelim_drm_i915_debug_event base;
	__u64 client_handle;

	__u64 vm_handle;
	__u64 va_start;
	__u64 va_length;
	__u32 num_uuids;
	__u32 flags;
	__u64 uuids[0];
} __attribute__((packed));

struct prelim_drm_i915_debug_event_eu_attention {
	struct prelim_drm_i915_debug_event base;
	__u64 client_handle;
	__u64 ctx_handle;
	__u64 lrc_handle;

	__u32 flags;

	struct i915_engine_class_instance ci;

	__u32 bitmask_size;

	/**
	 * Bitmask of thread attentions starting from natural
	 * hardware order of slice=0,subslice=0,eu=0, 8 attention
	 * bits per eu.
	 *
	 * NOTE: For dual subslice GENs, the bitmask is for
	 * lockstepped EUs and not for logical EUs. This makes
	 * the bitmask includu only half of logical EU count
	 * provided by topology query as we only control the
	 * 'pair' instead of individual EUs.
	 */

	__u8 bitmask[0];
} __attribute__((packed));

struct prelim_drm_i915_debug_event_page_fault {
	struct prelim_drm_i915_debug_event base;
	__u64 client_handle;
	__u64 ctx_handle;
	__u64 lrc_handle;

	__u32 flags;

	struct i915_engine_class_instance ci;

	__u64 page_fault_address;

	/**
	 * Size of one bitmask: sum of size before/after/resolved att bits.
	 * It has three times the size of prelim_drm_i915_debug_event_eu_attention.bitmask_size.
	 */
	__u32 bitmask_size;

	/**
	 * Bitmask of thread attentions starting from natural
	 * hardware order of slice=0,subslice=0,eu=0, 8 attention
	 * bits per eu.
	 * The order of the bitmask array is before, after, resolved.
	 */

	__u8 bitmask[0];
} __attribute__((packed));

struct prelim_drm_i915_debug_read_uuid {
	__u64 client_handle;
	__u64 handle;
	__u32 flags; /* MBZ */
	char uuid[36]; /* output */
	__u64 payload_ptr;
	__u64 payload_size;
} __attribute__((packed));

struct prelim_drm_i915_debug_event_context_param {
	struct prelim_drm_i915_debug_event base;
	__u64 client_handle;
	__u64 ctx_handle;
	struct drm_i915_gem_context_param param;
} __attribute__((packed));

struct prelim_drm_i915_debug_engine_info {
	struct i915_engine_class_instance engine;
	__u64 lrc_handle;
} __attribute__((packed));

struct prelim_drm_i915_debug_event_engines {
	struct prelim_drm_i915_debug_event base;
	__u64 client_handle;
	__u64 ctx_handle;
	__u64 num_engines;
	struct prelim_drm_i915_debug_engine_info engines[0];
} __attribute__((packed));

struct prelim_drm_i915_debug_vm_open {
	__u64 client_handle;
	__u64 handle; /* input: The target address space (ppGTT) */
	__u64 flags;
#define PRELIM_I915_DEBUG_VM_OPEN_READ_ONLY	O_RDONLY
#define PRELIM_I915_DEBUG_VM_OPEN_WRITE_ONLY	O_WRONLY
#define PRELIM_I915_DEBUG_VM_OPEN_READ_WRITE	O_RDWR
};

struct prelim_drm_i915_debug_eu_control {
	__u64 client_handle;
	__u32 cmd;
#define PRELIM_I915_DEBUG_EU_THREADS_CMD_INTERRUPT_ALL 0
#define PRELIM_I915_DEBUG_EU_THREADS_CMD_STOPPED   1
#define PRELIM_I915_DEBUG_EU_THREADS_CMD_RESUME    2
#define PRELIM_I915_DEBUG_EU_THREADS_CMD_INTERRUPT 3
	__u32 flags;
	__u64 seqno;

	struct i915_engine_class_instance ci;
	__u32 bitmask_size;

	/**
	 * Bitmask of thread attentions starting from natural
	 * hardware order of slice=0,subslice=0,eu=0, 8 attention bits
	 * per eu.
	 *
	 * NOTE: For dual subslice GENs, the bitmask is for
	 * lockstepped EUs and not for logical EUs. This makes
	 * the bitmask includu only half of logical EU count
	 * provided by topology query as we only control the
	 * 'pair' instead of individual EUs.
	 */
	__u64 bitmask_ptr;
} __attribute__((packed));

struct prelim_drm_i915_debug_event_ack {
	__u32 type;
	__u32 flags; /* MBZ */
	__u64 seqno;
} __attribute__((packed));

enum prelim_drm_i915_gem_memory_class {
	PRELIM_I915_MEMORY_CLASS_SYSTEM = 0,
	PRELIM_I915_MEMORY_CLASS_DEVICE,
	PRELIM_I915_MEMORY_CLASS_NONE = -1
};

/**
 * struct prelim_drm_i915_memory_region_info
 *
 * Describes one region as known to the driver.
 */
struct prelim_drm_i915_memory_region_info {
	/** class:instance pair encoding */
	struct prelim_drm_i915_gem_memory_class_instance region;

	/** MBZ */
	__u32 rsvd0;

	/** MBZ */
	__u64 caps;

	/** MBZ */
	__u64 flags;

	/** Memory probed by the driver (-1 = unknown) */
	__u64 probed_size;

	/** Estimate of memory remaining (-1 = unknown) */
	__u64 unallocated_size;

	/** MBZ */
	__u64 rsvd1[8];
};

/**
 * struct prelim_drm_i915_query_memory_regions
 *
 * Region info query enumerates all regions known to the driver by filling in
 * an array of struct prelim_drm_i915_memory_region_info structures.
 */
struct prelim_drm_i915_query_memory_regions {
	/** Number of supported regions */
	__u32 num_regions;

	/** MBZ */
	__u32 rsvd[3];

	/* Info about each supported region */
	struct prelim_drm_i915_memory_region_info regions[];
};

/**
 * struct prelim_drm_i915_lmem_memory_region_info
 *
 * Expose the available lmem region size in bytes as per the limit's
 * set in prelim_sharedmem_alloc_limit, prelim_lmem_alloc_limit
 */
struct prelim_drm_i915_lmem_memory_region_info {
	/** class:instance pair encoding */
	struct prelim_drm_i915_gem_memory_class_instance region;

	/** MBZ */
	__u32 rsvd0;

	/** Estimate of memory remaining  */
	__u64 unallocated_usr_lmem_size;

	/** Estimate of memory remaining */
	__u64 unallocated_usr_shared_size;
};

/**
 * struct prelim_drm_i915_query_lmem_memory_regions
 *
 * Region info query enumerates all lmem regions known to the driver by filling
 * in an array of struct prelim_drm_i915_lmem_memory_region_info structures.
 */
struct prelim_drm_i915_query_lmem_memory_regions {
	/** Number of supported regions */
	__u32 num_lmem_regions;

	/** MBZ */
	__u32 rsvd[3];

	/** Info about each supported region */
	struct prelim_drm_i915_lmem_memory_region_info regions[];
};

/**
 * struct prelim_drm_i915_query_distance_info
 *
 * Distance info query returns the distance of given (class, instance)
 * engine to the memory region id passed by the user. If the distance
 * is -1 it means region is unreachable.
 */
struct prelim_drm_i915_query_distance_info {
	/** Engine for which distance is queried */
	struct i915_engine_class_instance engine;

	/** Memory region to be used */
	struct prelim_drm_i915_gem_memory_class_instance region;

	/** Distance to region from engine */
	__s32 distance;

	/** Must be zero */
	__u32 rsvd[3];
};

/**
 * struct prelim_drm_i915_query_cs_cycles
 *
 * The query returns the command streamer cycles and the frequency that can be
 * used to calculate the command streamer timestamp. In addition the query
 * returns the cpu timestamp that indicates when the command streamer cycle
 * count was captured.
 */
struct prelim_drm_i915_query_cs_cycles {
	/** Engine for which command streamer cycles is queried. */
	struct i915_engine_class_instance engine;

	/** Must be zero. */
	__u32 flags;

	/**
	 * Command streamer cycles as read from the command streamer
	 * register at 0x358 offset.
	 */
	__u64 cs_cycles;

	/** Frequency of the cs cycles in Hz. */
	__u64 cs_frequency;

	/** CPU timestamp in nanoseconds. */
	__u64 cpu_timestamp;

	/**
	 * Reference clock id for CPU timestamp. For definition, see
	 * clock_gettime(2) and perf_event_open(2). Supported clock ids are
	 * CLOCK_MONOTONIC, CLOCK_MONOTONIC_RAW, CLOCK_REALTIME, CLOCK_BOOTTIME,
	 * CLOCK_TAI.
	 */
	__s32 clockid;

	/** Must be zero. */
	__u32 rsvd;
};

/**
 * prelim_struct drm_i915_query_hw_ip_version
 *
 * Hardware IP version (i.e., architecture generation) associated with a
 * specific engine.
 */
struct prelim_drm_i915_query_hw_ip_version {
	/** Engine to query HW IP version for */
	struct i915_engine_class_instance engine;

	__u8 flags;	/* MBZ */

	/** Architecture  version */
	__u8 arch;

	/** Architecture release id */
	__u8 release;

	/** Stepping (e.g., A0, A1, B0, etc.) */
	__u8 stepping;
};

/**
 * struct prelim_drm_i915_query_fabric_info
 *
 * With the given fabric id, query fabric info wrt the device.
 * Higher bandwidth is better.  0 means no fabric.
 * Latency is averaged latency (from all paths)
 *
 * fabric_id can be obtained from
 *    /sys/class/drm/cardx/device/iaf.y/iaf_fabric_id
 * Bandwidth is in Gigabits per second (max value of 8 * 4 * 90)
 *    8 possible ports
 *    4 lanes max per port
 *   90 gigabits per lane
 * Latency is in tenths of path length. 10 == 1 fabric link between src and dst
 *   POR is max 1 link (zero hops).
 */
struct prelim_drm_i915_query_fabric_info {
	__u32 fabric_id;
	__u16 bandwidth;
	__u16 latency;
};

/**
 * struct prelim_drm_i915_engine_info
 *
 * Describes one engine and it's capabilities as known to the driver.
 */
struct prelim_drm_i915_engine_info {
	/** Engine class and instance. */
	struct i915_engine_class_instance engine;

	/**
	 * SW defined id that identifies the OA unit associated with this
	 * engine. A value of U32_MAX means engine is not supported by OA. All
	 * other values are valid and can be used to group engines into the
	 * associated OA unit.
	 */
	__u32 oa_unit_id;

	/** Engine flags. */
	__u64 flags;
#define PRELIM_I915_ENGINE_INFO_HAS_KNOWN_CAPABILITIES	(1ull << 63)
#define PRELIM_I915_ENGINE_INFO_HAS_LOGICAL_INSTANCE	(1ull << 62)
#define PRELIM_I915_ENGINE_INFO_HAS_OA_UNIT_ID		(1ull << 61)

	/** Capabilities of this engine. */
	__u64 capabilities;
#define I915_VIDEO_CLASS_CAPABILITY_HEVC		(1 << 0)
#define I915_VIDEO_AND_ENHANCE_CLASS_CAPABILITY_SFC	(1 << 1)
#define PRELIM_I915_VIDEO_CLASS_CAPABILITY_VDENC	(1ull << 63)
#define PRELIM_I915_COPY_CLASS_CAP_BLOCK_COPY		(1ull << 63)
	/*
	 * The following are capabilties of the copy engines, while all engines
	 * are functionally same, but engines with cap PRELIM_I915_COPY_CLASS_CAP_SATURATE_LINK
	 * can saturate pcie and scaleup links faster than engines with
	 * PRELIM_I915_COPY_CLASS_CAP_SATURATE_PCIE. Engines having the capability of
	 * PRELIM_I915_COPY_CLASS_CAP_SATURATE_LMEM can operate at HBM speeds.
	 */
#define PRELIM_I915_COPY_CLASS_CAP_SATURATE_PCIE	(1ull << 62)
#define PRELIM_I915_COPY_CLASS_CAP_SATURATE_LINK	(1ull << 61)
#define PRELIM_I915_COPY_CLASS_CAP_SATURATE_LMEM	(1ull << 60)

	/** All known capabilities for this engine class. */
	__u64 known_capabilities;

	/** Logical engine instance */
	__u16 logical_instance;

	/** Reserved fields. */
	__u16 rsvd1[3];
	__u64 rsvd2[2];
};

/**
 * struct drm_i915_query_engine_info
 *
 * Engine info query enumerates all engines known to the driver by filling in
 * an array of struct drm_i915_engine_info structures.
 */
struct prelim_drm_i915_query_engine_info {
	/** Number of struct drm_i915_engine_info structs following. */
	__u32 num_engines;

	/** MBZ */
	__u32 rsvd[3];

	/** Marker for drm_i915_engine_info structures. */
	struct prelim_drm_i915_engine_info engines[];
};

/**
 * struct prelim_drm_i915_gem_vm_bind
 *
 * VA to object/buffer mapping to [un]bind.
 *
 * NOTE:
 * A vm_bind will hold a reference on the BO which is released
 * during corresponding vm_unbind or while closing the VM.
 * Hence closing the BO alone will not ensure BO is released.
 */
struct prelim_drm_i915_gem_vm_bind {
	/** vm to [un]bind **/
	__u32 vm_id;

	/** BO handle or file descriptor **/
	union {
		__u32 handle; /* For unbind, it is reserved and must be 0 */
		__s32 fd;
	};

	/** VA start to [un]bind **/
	__u64 start;

	/** Offset in object to [un]bind **/
	__u64 offset;

	/** VA length to [un]bind **/
	__u64 length;

	/** Flags **/
	__u64 flags;
#define PRELIM_I915_GEM_VM_BIND_IMMEDIATE	(1ull << 63)
#define PRELIM_I915_GEM_VM_BIND_READONLY	(1ull << 62)
#define PRELIM_I915_GEM_VM_BIND_CAPTURE		(1ull << 61)

/*
 * PRELIM_I915_GEM_VM_BIND_MAKE_RESIDENT
 *
 * The legacy behaviour of a VM_BIND is that it is resident whenever the
 * context/vm is active on the GPU. That is when the user is executing their
 * batch, all current VM_BIND objects are accessible via their defined user
 * virtual addresses. This is relaxed for pagefaultable vm, where the virtual
 * address may be faulted and loaded upon demand, thus not all the user objects
 * are bound at the time of user execution.
 *
 * PRELIM_I915_GEM_VM_BIND_MAKE_RESIDENT changes the behaviour of the faultable
 * VM_BIND such that is resident in memory and bound to that virtual address
 * for the duration of the binding (until the equivalent VM_UNBIND). Like with
 * the non-faultable vm, a resident object is always accessible by user
 * execution without generating pagefaults. The exception being that if the
 * object is marked for atomic access, the buffer has to be faulted in and out
 * of device memory for CPU access. It is also possible for the buffer to be
 * migrated via its preferred placement hint.
 *
 * If the entire resident set cannot fit within memory, an out of memory error
 * is immediately reported from the ioctl.
 */
#define PRELIM_I915_GEM_VM_BIND_MAKE_RESIDENT	(1ull << 60)
#define PRELIM_I915_GEM_VM_BIND_FD		(1ull << 59)

	__u64 extensions;
};

/**
 * struct prelim_drm_i915_gem_vm_advise
 *
 * Set attribute (hint) for an address range, whole buffer object, or
 * part of buffer object.
 *
 * To apply attribute to whole buffer object, specify:  handle
 *
 * To apply attribute to part of buffer object (chunk granularity), specify:
 *   handle, start, and length.
 * Start and length must be exactly aligned to chunk boundaries of object.
 * Above requires object to have been created during GEM_CREATE with:
 * PRELIM_I915_PARAM_SET_CHUNK_SIZE.
 *
 * To apply attribute to address range, specify:  vm_id, start, and length.
 *
 * On error, any applied hints are reverted before returning.
 */
struct prelim_drm_i915_gem_vm_advise {
	/** vm that contains address range (specified with start, length) */
	__u32 vm_id;

	/** BO handle to apply hint */
	__u32 handle;

	/**
	 * chunk granular hints: offset from beginning of object to apply hint
	 * address range hints: VA start of address range to apply hint
	 */
	__u64 start;

	/** Length of range to apply attribute */
	__u64 length;

	/**
	 * Attributes to apply to address range or buffer object
	 *
	 * For object hints, hints may not be set on imported (prime_import)
	 * objects and will return error (-EPERM). Hints may only be set against
	 * the exported object,
	 *
	 * ADVISE_ATOMIC_SYSTEM
	 *      inform that atomic access is enabled for both CPU and GPU.
	 *      For some platforms, this may be required for correctness
	 *      and this hint will influence migration policy.
	 *      This hint is not allowed unless placement list includes SMEM,
	 *      and is not allowed for exported buffer objects (prime_export)
	 *	with placement list of LMEM + SMEM and returns error (-EPERM).
	 * ADVISE_ATOMIC_DEVICE
	 *      inform that atomic access is enabled for GPU devices. For
	 *      some platforms, this may be required for correctness and
	 *      this hint will influence migration policy.
	 * ADVISE_ATOMIC_NONE
	 *	clears above ATOMIC_SYSTEM / ATOMIC_DEVICE hint.
	 * PREFERRED_LOCATION
	 *	sets the preferred memory class and instance for this object's
	 *	backing store.  This is a hint only and not guaranteed to be
	 *	honored.  It is an error to choose a memory region that was not
	 *	part of the original set of placements for the GEM object.
	 *	If choosing a preferred location that is in conflict with the
	 *	use of ATOMIC_SYSTEM or ATOMIC_DEVICE, the atomic hint will
	 *	always be honored first.
	 *	To clear the current preferred location, specify memory class
	 *	as I915_MEMORY_CLASS_NONE.
	 */
	__u32 attribute;
#define PRELIM_I915_VM_ADVISE				(1 << 16)
#define PRELIM_I915_VM_ADVISE_ATOMIC_NONE		(PRELIM_I915_VM_ADVISE | 0)
#define PRELIM_I915_VM_ADVISE_ATOMIC_SYSTEM		(PRELIM_I915_VM_ADVISE | 1)
#define PRELIM_I915_VM_ADVISE_ATOMIC_DEVICE		(PRELIM_I915_VM_ADVISE | 2)
	/**
	 * Attributes to apply to address range or buffer object. Different from
	 * above ADVISE_ATOMIC, SET_ATOMIC guarantees the correctness of atomic
	 * operation instead of just a hint/advise. This is mainly to fix the
	 * naming problem of above API: atomics should be a guarantee, not a
	 * hint/advise.
	 *
	 * Since currently driver already implements ADVISE_ATOMIC APIs as a
	 * guarantee, so just define SET_ATOMIC APIs the same behavior as
	 * ADVISE_ATOMIC APIs.
	 */
#define PRELIM_I915_VM_SET_ATOMIC_NONE		PRELIM_I915_VM_ADVISE_ATOMIC_NONE
#define PRELIM_I915_VM_SET_ATOMIC_SYSTEM	PRELIM_I915_VM_ADVISE_ATOMIC_SYSTEM
#define PRELIM_I915_VM_SET_ATOMIC_DEVICE	PRELIM_I915_VM_ADVISE_ATOMIC_DEVICE
#define PRELIM_I915_VM_ADVISE_PREFERRED_LOCATION	(PRELIM_I915_VM_ADVISE | 3)

	/** Preferred location (memory region) for object backing */
	struct prelim_drm_i915_gem_memory_class_instance region;

	__u32 rsvd[2];
};

/**
 * struct prelim_drm_i915_gem_wait_user_fence
 *
 * Wait on user fence. User fence can be woken up either by,
 *    1. GPU context indicated by 'ctx_id', or,
 *    2. Kerrnel driver async worker upon PRELIM_I915_UFENCE_WAIT_SOFT.
 *       'ctx_id' is ignored when this flag is set.
 *
 * Wakeup when below condition is true.
 * (*addr & MASK) OP (VALUE & MASK)
 *
 */
struct prelim_drm_i915_gem_wait_user_fence {
	__u64 extensions;
	__u64 addr;
	__u32 ctx_id;
	__u16 op;
#define PRELIM_I915_UFENCE		(1 << 8)
#define PRELIM_I915_UFENCE_WAIT_EQ	(PRELIM_I915_UFENCE | 0)
#define PRELIM_I915_UFENCE_WAIT_NEQ	(PRELIM_I915_UFENCE | 1)
#define PRELIM_I915_UFENCE_WAIT_GT	(PRELIM_I915_UFENCE | 2)
#define PRELIM_I915_UFENCE_WAIT_GTE	(PRELIM_I915_UFENCE | 3)
#define PRELIM_I915_UFENCE_WAIT_LT	(PRELIM_I915_UFENCE | 4)
#define PRELIM_I915_UFENCE_WAIT_LTE	(PRELIM_I915_UFENCE | 5)
#define PRELIM_I915_UFENCE_WAIT_BEFORE	(PRELIM_I915_UFENCE | 6)
#define PRELIM_I915_UFENCE_WAIT_AFTER	(PRELIM_I915_UFENCE | 7)
	__u16 flags;
#define PRELIM_I915_UFENCE_WAIT_SOFT	(1 << 15)
#define PRELIM_I915_UFENCE_WAIT_ABSTIME	(1 << 14)
	__u64 value;
	__u64 mask;
#define PRELIM_I915_UFENCE_WAIT_U8     0xffu
#define PRELIM_I915_UFENCE_WAIT_U16    0xffffu
#define PRELIM_I915_UFENCE_WAIT_U32    0xfffffffful
#define PRELIM_I915_UFENCE_WAIT_U64    0xffffffffffffffffull
	__s64 timeout;
};

/*
 * This extension allows user to attach a pair of <addr, value> to an execbuf.
 * When that execbuf is finished by GPU HW, the value is written to addr.
 * So after execbuf is submitted, user can poll addr to know whether execbuf
 * has been finished or not. User space can also call i915_gem_wait_user_fence_ioctl
 * (with PRELIM_I915_UFENCE_WAIT_EQ operation) to wait for finishing of execbuf.
 * This ioctl can sleep so it is more efficient than a busy polling.
 * So this serves as synchronization purpose. It is similar to DRM_IOCTL_I915_GEM_WAIT,
 * which is prohibited by compute context. The method introduced here can be use for
 * both compute and non-compute context.
 */
struct prelim_drm_i915_gem_execbuffer_ext_user_fence {
#define PRELIM_DRM_I915_GEM_EXECBUFFER_EXT_USER_FENCE (PRELIM_I915_USER_EXT | 1)
	struct i915_user_extension base;

	/**
	 * A virtual address mapped to current process's GPU address space.
	 * addr has to be qword aligned. address has to be a a valid gpu
	 * virtual address at the time of batch buffer completion.
	 */
	__u64 addr;

	/**
	 * value to be written to above address after execbuf finishes.
	 */
	__u64 value;
	/**
	 * for future extensions. Currently not used.
	 */
	__u64 rsvd;
};

/* Deprecated in favor of prelim_drm_i915_vm_bind_ext_user_fence */
struct prelim_drm_i915_vm_bind_ext_sync_fence {
#define PRELIM_I915_VM_BIND_EXT_SYNC_FENCE     (PRELIM_I915_USER_EXT | 0)
	struct i915_user_extension base;
	__u64 addr;
	__u64 val;
};

struct prelim_drm_i915_vm_bind_ext_user_fence {
#define PRELIM_I915_VM_BIND_EXT_USER_FENCE     (PRELIM_I915_USER_EXT | 3)
	struct i915_user_extension base;
	__u64 addr;
	__u64 val;
	__u64 rsvd;
};

struct prelim_drm_i915_gem_vm_control {
#define PRELIM_I915_VM_CREATE_FLAGS_DISABLE_SCRATCH	(1 << 16)
#define PRELIM_I915_VM_CREATE_FLAGS_ENABLE_PAGE_FAULT	(1 << 17)
#define PRELIM_I915_VM_CREATE_FLAGS_USE_VM_BIND		(1 << 18)
#define PRELIM_I915_VM_CREATE_FLAGS_UNKNOWN		(~(GENMASK(18, 16)))
};

struct prelim_drm_i915_gem_vm_region_ext {
#define PRELIM_I915_GEM_VM_CONTROL_EXT_REGION	(PRELIM_I915_USER_EXT | 0)
	struct i915_user_extension base;
	/* memory region: to find gt to create vm on */
	struct prelim_drm_i915_gem_memory_class_instance region;
	__u32 pad;
};

struct prelim_drm_i915_vm_bind_ext_set_pat {
#define PRELIM_I915_VM_BIND_EXT_SET_PAT	(PRELIM_I915_USER_EXT | 2)
       struct i915_user_extension base;
       __u64 pat_index;
};

/**
 * struct prelim_drm_i915_gem_clos_reserve
 *
 * Allows clients to request reservation of one free CLOS, to use in subsequent
 * Cache Reservations.
 *
 */
struct prelim_drm_i915_gem_clos_reserve {
	__u16 clos_index;
	__u16 pad16;
};

/**
 * struct prelim_drm_i915_gem_clos_free
 *
 * Free off a previously reserved CLOS set. Any corresponding Cache Reservations
 * that are active for the CLOS are automatically dropped and returned to the
 * Shared set.
 *
 * The clos_index indicates the CLOS set which is being released and must
 * correspond to a CLOS index previously reserved.
 *
 */
struct prelim_drm_i915_gem_clos_free {
	__u16 clos_index;
	__u16 pad16;
};

/**
 * struct prelim_drm_i915_gem_cache_reserve
 *
 * Allows clients to request, or release, reservation of one or more cache ways,
 * within a previously reserved CLOS set.
 *
 * If num_ways = 0, i915 will drop any existing Reservation for the specified
 * clos_index and cache_level. The requested clos_index and cache_level Waymasks
 * will then track the Shared set once again.
 *
 * Otherwise, the requested number of Ways will be removed from the Shared set
 * for the requested cache level, and assigned to the Cache and CLOS specified
 * by cache_level/clos_index.
 *
 */
struct prelim_drm_i915_gem_cache_reserve {
	__u16 clos_index;
	__u16 cache_level; /* e.g. 3 for L3 */
	__u16 num_ways;
	__u16 pad16;
};

/**
 * struct prelim_drm_i915_gem_vm_prefetch
 *
 * Prefetch an address range to a memory region, support both
 * system allocator and runtime allocator.
 */
struct prelim_drm_i915_gem_vm_prefetch {
	/** Memory region to prefetch to **/
	__u32 region;

	/**
	 * Destination vm id to prefetch to.
	 * Only valid for runtime allocator.
	 * System allocator doesn't need a vm_id, should be set to 0.
	 */
	__u32 vm_id;

	/** VA start to prefetch **/
	__u64 start;

	/** VA length to prefetch **/
	__u64 length;
};

struct prelim_drm_i915_gem_vm_param {
	__u32 vm_id;
	__u32 rsvd;

#define PRELIM_I915_VM_PARAM		(1ull << 63)
#define PRELIM_I915_GEM_VM_PARAM_SVM	(1 << 16)
	__u64 param;

	__u64 value;
};

#endif /* __I915_DRM_PRELIM_H__ */
