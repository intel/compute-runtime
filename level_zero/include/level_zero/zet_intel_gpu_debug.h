/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _ZET_INTEL_GPU_DEBUG_H
#define _ZET_INTEL_GPU_DEBUG_H
#if defined(__cplusplus)
#pragma once
#endif

#if defined(__cplusplus)
extern "C" {
#endif

#ifndef ZET_INTEL_GPU_DEBUG_MAJOR
#define ZET_INTEL_GPU_DEBUG_MAJOR 1
#endif // !ZET_INTEL_GPU_DEBUG_MAJOR

#ifndef ZET_INTEL_GPU_DEBUG_MINOR
#define ZET_INTEL_GPU_DEBUG_MINOR 1
#endif //! ZET_INTEL_GPU_DEBUG_MINOR

#if ZET_INTEL_GPU_DEBUG_MAJOR == 1
///////////////////////////////////////////////////////////////////////////////
/// @brief Supported device-specific register set types.
typedef enum _zet_debug_regset_type_intel_gpu_t {
#if ZET_INTEL_GPU_DEBUG_MINOR >= 0
    ZET_DEBUG_REGSET_TYPE_INVALID_INTEL_GPU = 0, ///< An invalid register set
    ZET_DEBUG_REGSET_TYPE_GRF_INTEL_GPU = 1,     ///< The general purpose register set
    ZET_DEBUG_REGSET_TYPE_ADDR_INTEL_GPU = 2,    ///< The address register set
    ZET_DEBUG_REGSET_TYPE_FLAG_INTEL_GPU = 3,    ///< The flag register set
    ZET_DEBUG_REGSET_TYPE_CE_INTEL_GPU = 4,      ///< The channel enable register set
    ZET_DEBUG_REGSET_TYPE_SR_INTEL_GPU = 5,      ///< The status register set
    ZET_DEBUG_REGSET_TYPE_CR_INTEL_GPU = 6,      ///< The control register set
    ZET_DEBUG_REGSET_TYPE_TDR_INTEL_GPU = 7,     ///< The thread dependency register set
    ZET_DEBUG_REGSET_TYPE_ACC_INTEL_GPU = 8,     ///< The accumulator register set
    ZET_DEBUG_REGSET_TYPE_MME_INTEL_GPU = 9,     ///< The mme register set
    ZET_DEBUG_REGSET_TYPE_SP_INTEL_GPU = 10,     ///< The stack pointer register set
    ZET_DEBUG_REGSET_TYPE_SBA_INTEL_GPU = 11,    ///< The state base address register set
    ZET_DEBUG_REGSET_TYPE_DBG_INTEL_GPU = 12,    ///< The debug register set
    ZET_DEBUG_REGSET_TYPE_FC_INTEL_GPU = 13,     ///< The flow control register set
#if ZET_INTEL_GPU_DEBUG_MINOR >= 1
    ZET_DEBUG_REGSET_TYPE_MSG_INTEL_GPU = 14,            ///< The msg register set
    ZET_DEBUG_REGSET_TYPE_MODE_FLAGS_INTEL_GPU = 15,     ///< System Routine mode flags register set
    ZET_DEBUG_REGSET_TYPE_DEBUG_SCRATCH_INTEL_GPU = 16,  ///< Debug Scratch register set
    ZET_DEBUG_REGSET_TYPE_THREAD_SCRATCH_INTEL_GPU = 17, ///< Thread Scratch register set
    ZET_DEBUG_REGSET_TYPE_SCALAR_INTEL_GPU = 18,         ///< The scalar register set
#endif                                                   // ZET_INTEL_GPU_DEBUG_MINOR >= 1
    ZET_DEBUG_REGSET_TYPE_FORCE_UINT32 = 0x7fffffff
#endif // ZET_INTEL_GPU_DEBUG_MINOR >= 0
} zet_debug_regset_type_intel_gpu_t;

#if ZET_INTEL_GPU_DEBUG_MINOR >= 1
////////////////////////////////////////////////////////////////////////////////
/// @brief ZET_DEBUG_REGSET_TYPE_MODE_FLAGS_INTEL_GPU flag values
typedef enum _zet_debug_mode_flags_intel_gpu_t {
    ZET_DEBUG_MODE_FLAG_HEAPLESS = 1 ///< Bit set indicates heapless mode
} zet_debug_mode_flags_intel_gpu_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief ZET_DEBUG_REGSET_TYPE_DEBUG_SCRATCH_INTEL_GPU register layout
typedef enum _zet_debug_debug_scratch_intel_gpu_t {
    ZET_DEBUG_DEBUG_SCRATCH_ADDRESS = 0, ///< VA of scratch area for debugger code injection.
                                         ///< VA is in default address space of device
    ZET_DEBUG_DEBUG_SCRATCH_SIZE = 1,    ///< Size of scratch area in bytes
} zet_debug_debug_scratch_intel_gpu_t;
#endif // ZET_INTEL_GPU_DEBUG_MINOR >= 1
////////////////////////////////////////////////////////////////////////////////
/// @brief SBA register set layout
typedef enum _zet_debug_sba_intel_gpu_t {
    ZET_DEBUG_SBA_GENERAL_STATE_INTEL_GPU = 0,    ///< GeneralStateBaseAddress
    ZET_DEBUG_SBA_SURFACE_STATE_INTEL_GPU = 1,    ///< SurfaceStateBaseAddress
    ZET_DEBUG_SBA_DYNAMIC_STATE_INTEL_GPU = 2,    ///< DynamicStateBaseAddress
    ZET_DEBUG_SBA_INDIRECT_OBJECT_INTEL_GPU = 3,  ///< IndirectObjectBaseAddress
    ZET_DEBUG_SBA_INSTRUCTION_INTEL_GPU = 4,      ///< InstructionBaseAddress
    ZET_DEBUG_SBA_BINDLESS_SURFACE_INTEL_GPU = 5, ///< BindlessSurfaceStateBaseAddress
    ZET_DEBUG_SBA_BINDLESS_SAMPLER_INTEL_GPU = 6, ///< BindlessSamplerStateBaseAddress
    ZET_DEBUG_SBA_BINDING_TABLE_INTEL_GPU = 7,    ///< BindingTableStateBaseAddress
    ZET_DEBUG_SBA_SCRATCH_SPACE_INTEL_GPU = 8,    ///< ScratchSpaceBaseAddress
    ZET_DEBUG_SBA_COUNT_INTEL_GPU = 9             ///< Number of registers in SBA register set
} zet_debug_sba_intel_gpu_t;
#endif // ZET_INTEL_GPU_DEBUG_MAJOR == 1

#if defined(__cplusplus)
} // extern "C"
#endif

#endif // _ZET_INTEL_GPU_DEBUG_H
