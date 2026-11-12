/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
/* Copyright © 2026 Intel Corporation */
#ifndef _UAPI_FWCTL_XE_SYSCTRL_H_
#define _UAPI_FWCTL_XE_SYSCTRL_H_

#include <linux/bits.h>
#include <linux/types.h>

/**
 * enum fwctl_xe_sysctrl_uctx_caps - capability flags in fwctl_info_xe_sysctrl.uctx_caps
 *
 * @FWCTL_XE_SYSCTRL_CAP_ECC: ECC status query is supported on this platform
 * @FWCTL_XE_SYSCTRL_CAP_RAS_ERROR_INJECT: RAS error injection is supported;
 *      requires diag firmware to be loaded and ready
 */
enum fwctl_xe_sysctrl_uctx_caps {
	FWCTL_XE_SYSCTRL_CAP_ECC		= (1U << 0),
	FWCTL_XE_SYSCTRL_CAP_RAS_ERROR_INJECT	= (1U << 1),
};

/**
 * struct fwctl_info_xe_sysctrl - device data returned by FWCTL_INFO
 * @uctx_caps: Bitmask of available capabilities (see %fwctl_xe_sysctrl_uctx_caps)
 * @rsvd: Reserved, must be zero
 */
struct fwctl_info_xe_sysctrl {
	__u32 uctx_caps;
	__u32 rsvd[7];
};

/**
 * enum fwctl_xe_sysctrl_ras_inj_cap - RAS error injection capability bits
 *
 * Used both in the @capability_mask output of %FWCTL_XE_SYSCTRL_OP_RAS_GET_CAPS
 * and as the @error_inj input of %FWCTL_XE_SYSCTRL_OP_RAS_INJECT. Userspace
 * must confirm a bit is set in @capability_mask before injecting that class.
 *
 * @FWCTL_XE_SYSCTRL_RAS_INJ_CAP_COMPUTE_CORR: Compute block correctable error
 * @FWCTL_XE_SYSCTRL_RAS_INJ_CAP_COMPUTE_UCORR: Compute block uncorrectable error
 * @FWCTL_XE_SYSCTRL_RAS_INJ_CAP_MEMORY_UCORR: Memory subsystem uncorrectable error
 * @FWCTL_XE_SYSCTRL_RAS_INJ_CAP_FABRIC_UCORR: Fabric interconnect uncorrectable error
 * @FWCTL_XE_SYSCTRL_RAS_INJ_CAP_PCIE_CORR: PCIe subsystem correctable error
 * @FWCTL_XE_SYSCTRL_RAS_INJ_CAP_PCIE_UCORR: PCIe subsystem uncorrectable error
 * @FWCTL_XE_SYSCTRL_RAS_INJ_CAP_PLATFORM_UCORR: Platform-level uncorrectable error
 */
enum fwctl_xe_sysctrl_ras_inj_cap {
	FWCTL_XE_SYSCTRL_RAS_INJ_CAP_COMPUTE_CORR	= 0x0001U,
	FWCTL_XE_SYSCTRL_RAS_INJ_CAP_COMPUTE_UCORR	= 0x0002U,
	FWCTL_XE_SYSCTRL_RAS_INJ_CAP_MEMORY_UCORR	= 0x0004U,
	FWCTL_XE_SYSCTRL_RAS_INJ_CAP_FABRIC_UCORR	= 0x0008U,
	FWCTL_XE_SYSCTRL_RAS_INJ_CAP_PCIE_CORR		= 0x0010U,
	FWCTL_XE_SYSCTRL_RAS_INJ_CAP_PCIE_UCORR		= 0x0020U,
	FWCTL_XE_SYSCTRL_RAS_INJ_CAP_PLATFORM_UCORR	= 0x0040U,
};

/* FWCTL_XE_SYSCTRL_RAS_INJ_ALLOWED - bit in the RAS_GET_CAPS flags field
 * indicating RAS error injection is currently allowed by hardware security
 * policy.
 */
#define FWCTL_XE_SYSCTRL_RAS_INJ_ALLOWED	(1U << 0)

/**
 * enum fwctl_xe_sysctrl_op - RPC operation codes for xe_sysctrl fwctl
 *
 * @FWCTL_XE_SYSCTRL_OP_ECC_STATUS_GET: Read ECC feature state from GFSP firmware
 *	(GFSP runtime group, GET_FEATURE_CAPABILITY_STATE_INFO, BIT(0) = ECC).
 *	Requires: %FWCTL_XE_SYSCTRL_CAP_ECC in uctx_caps.
 *	Scope: %FWCTL_RPC_CONFIGURATION.
 *	in:  op, flags=0, data[0..13] ignored.
 *	out: data[0] = 1 if ECC is supported by this product, 0 otherwise.
 *	     data[1] = 1 if ECC is currently enabled, 0 otherwise.
 *
 * @FWCTL_XE_SYSCTRL_OP_RAS_GET_CAPS: Query RAS error injection capability from
 *	diag firmware (DIAG group, RAS_ERR_INJ_CAPABILITY command).
 *	Requires: %FWCTL_XE_SYSCTRL_CAP_RAS_ERROR_INJECT in uctx_caps.
 *	Scope: %FWCTL_RPC_CONFIGURATION.
 *	in:  op, flags=0, data[0..13] ignored.
 *	out: data[0] = capability_mask, an OR of %fwctl_xe_sysctrl_ras_inj_cap
 *	                bits indicating which injection classes are supported,
 *	     data[1] = flags; bit %FWCTL_XE_SYSCTRL_RAS_INJ_ALLOWED indicates
 *	                whether RAS error injection is currently allowed by
 *	                hardware security policy, other bits are reserved for
 *	                future use.
 *
 * @FWCTL_XE_SYSCTRL_OP_RAS_INJECT: Inject a RAS error via diag firmware
 *	(DIAG group, RAS_ERR_INJECT command).
 *	Requires: %FWCTL_XE_SYSCTRL_CAP_RAS_ERROR_INJECT in uctx_caps.
 *	Scope: %FWCTL_RPC_DEBUG_WRITE.
 *	in:  data[0] = error_inj, one of the %fwctl_xe_sysctrl_ras_inj_cap
 *	                bit values. Userspace must confirm this bit is set in
 *	                the capability_mask returned by
 *	                %FWCTL_XE_SYSCTRL_OP_RAS_GET_CAPS (and that its flags
 *	                field currently allows injection) before issuing this
 *	                request; behavior for an unsupported value is
 *	                firmware-defined,
 *	     data[1] = additional_params (reserved; 0 for most injection types).
 *	out: no additional output data (success indicated by ioctl return code).
 */
enum fwctl_xe_sysctrl_op {
	FWCTL_XE_SYSCTRL_OP_ECC_STATUS_GET	= 1,
	FWCTL_XE_SYSCTRL_OP_RAS_GET_CAPS	= 2,
	FWCTL_XE_SYSCTRL_OP_RAS_INJECT		= 3,
};

/**
 * struct fwctl_rpc_xe_sysctrl - RPC request/response envelope
 * @op: Operation code (see %fwctl_xe_sysctrl_op)
 * @flags: Must be 0
 * @data: Operation-specific payload (see %fwctl_xe_sysctrl_op for layout)
 */
struct fwctl_rpc_xe_sysctrl {
	__u32 op;
	__u32 flags;
	__u32 data[14];
};

#endif
