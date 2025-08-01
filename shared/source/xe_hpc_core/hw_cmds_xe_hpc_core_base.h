/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/commands/bxml_generator_glue.h"
#include "shared/source/helpers/common_types.h"
#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/xe_hpc_core/hw_info.h"

#include "neo_igfxfmid.h"

#include <cstring>
#include <type_traits>

template <class T>
struct CmdParse;

namespace NEO {
struct XeHpcCore {
#include "shared/source/generated/xe_hpc_core/hw_cmds_generated_xe_hpc_core.inl"

    static constexpr uint32_t stateComputeModeForceNonCoherentMask = (0b11u << 3);
    static constexpr uint32_t stateComputeModeEuThreadSchedulingModeOverrideMask = (0b11u << 13);
    static constexpr uint32_t stateComputeModeLargeGrfModeMask = (1u << 15);
    static constexpr uint32_t bcsEngineCount = 9u;
    static constexpr uint32_t timestampPacketCount = 16u;

    static constexpr bool isUsingL3Control = false;
    static constexpr bool supportsSampler = false;
    static constexpr bool isUsingGenericMediaStateClear = true;
    static constexpr bool isUsingMiMemFence = true;
    static constexpr bool isUsingMiSetPredicate = true;
    static constexpr bool isUsingMiMathMocs = true;

    struct StateBaseAddressStateSupport {
        static constexpr bool bindingTablePoolBaseAddress = true;
    };

    struct PipelineSelectStateSupport {
        static constexpr bool systolicMode = true;
    };

    struct PreemptionDebugSupport {
        static constexpr bool preemptionMode = true;
        static constexpr bool stateSip = true;
        static constexpr bool csrSurface = false;
    };

    struct DataPortBindlessSurfaceExtendedMessageDescriptor {
        union {
            struct {
                uint32_t bindlessSurfaceOffset : 25;
                uint32_t reserved : 6;
            };
            uint32_t packed;
        };

        DataPortBindlessSurfaceExtendedMessageDescriptor() {
            packed = 0;
        }

        void setBindlessSurfaceOffset(uint32_t offsetInBindlessSurfaceHeapInBytes) {
            bindlessSurfaceOffset = offsetInBindlessSurfaceHeapInBytes >> 6;
        }

        uint32_t getBindlessSurfaceOffsetToPatch() {
            return bindlessSurfaceOffset << 6;
        }
    };
    static constexpr uint32_t cacheLineSize = 0x40;

    static_assert(sizeof(DataPortBindlessSurfaceExtendedMessageDescriptor) == sizeof(DataPortBindlessSurfaceExtendedMessageDescriptor::packed), "");
};

struct XeHpcCoreFamily : public XeHpcCore {
    using Parse = CmdParse<XeHpcCoreFamily>;
    using GfxFamily = XeHpcCoreFamily;
    using DefaultWalkerType = COMPUTE_WALKER;
    using PorWalkerType = COMPUTE_WALKER;
    using FrontEndStateCommand = CFE_STATE;
    using XY_BLOCK_COPY_BLT = typename GfxFamily::XY_BLOCK_COPY_BLT;
    using XY_COPY_BLT = typename GfxFamily::MEM_COPY;
    using XY_COLOR_BLT = typename GfxFamily::XY_FAST_COLOR_BLT;
    using MI_STORE_REGISTER_MEM_CMD = typename GfxFamily::MI_STORE_REGISTER_MEM;
    using TimestampPacketType = uint32_t;
    using StallingBarrierType = PIPE_CONTROL;
    static const COMPUTE_WALKER cmdInitGpgpuWalker;
    static const CFE_STATE cmdInitCfeState;
    static const INTERFACE_DESCRIPTOR_DATA cmdInitInterfaceDescriptorData;
    static const MI_BATCH_BUFFER_END cmdInitBatchBufferEnd;
    static const MI_BATCH_BUFFER_START cmdInitBatchBufferStart;
    static const PIPE_CONTROL cmdInitPipeControl;
    static const STATE_COMPUTE_MODE cmdInitStateComputeMode;
    static const _3DSTATE_BINDING_TABLE_POOL_ALLOC cmdInitStateBindingTablePoolAlloc;
    static const MI_SEMAPHORE_WAIT cmdInitMiSemaphoreWait;
    static const RENDER_SURFACE_STATE cmdInitRenderSurfaceState;
    static const POSTSYNC_DATA cmdInitPostSyncData;
    static const MI_SET_PREDICATE cmdInitSetPredicate;
    static const MI_LOAD_REGISTER_IMM cmdInitLoadRegisterImm;
    static const MI_LOAD_REGISTER_REG cmdInitLoadRegisterReg;
    static const MI_LOAD_REGISTER_MEM cmdInitLoadRegisterMem;
    static const MI_STORE_DATA_IMM cmdInitStoreDataImm;
    static const MI_STORE_REGISTER_MEM cmdInitStoreRegisterMem;
    static const MI_NOOP cmdInitNoop;
    static const MI_REPORT_PERF_COUNT cmdInitReportPerfCount;
    static const MI_ATOMIC cmdInitAtomic;
    static const PIPELINE_SELECT cmdInitPipelineSelect;
    static const MI_ARB_CHECK cmdInitArbCheck;
    static const STATE_BASE_ADDRESS cmdInitStateBaseAddress;
    static const MEDIA_SURFACE_STATE cmdInitMediaSurfaceState;
    static const SAMPLER_STATE cmdInitSamplerState;
    static const BINDING_TABLE_STATE cmdInitBindingTableState;
    static const MI_USER_INTERRUPT cmdInitUserInterrupt;
    static const MI_CONDITIONAL_BATCH_BUFFER_END cmdInitConditionalBatchBufferEnd;
    static const MI_FLUSH_DW cmdInitMiFlushDw;
    static const XY_BLOCK_COPY_BLT cmdInitXyBlockCopyBlt;
    static const MEM_COPY cmdInitXyCopyBlt;
    static const XY_FAST_COLOR_BLT cmdInitXyColorBlt;
    static const STATE_PREFETCH cmdInitStatePrefetch;
    static const _3DSTATE_BTD cmd3dStateBtd;
    static const MI_MEM_FENCE cmdInitMemFence;
    static const MEM_SET cmdInitMemSet;
    static const STATE_SIP cmdInitStateSip;
    static const STATE_SYSTEM_MEM_FENCE_ADDRESS cmdInitStateSystemMemFenceAddress;
    static constexpr bool isQwordInOrderCounter = false;
    static constexpr bool walkerPostSyncSupport = true;
    static constexpr size_t indirectDataAlignment = COMPUTE_WALKER::INDIRECTDATASTARTADDRESS_ALIGN_SIZE;
    static constexpr GFXCORE_FAMILY gfxCoreFamily = IGFX_XE_HPC_CORE;

    static constexpr bool supportsCmdSet(GFXCORE_FAMILY cmdSetBaseFamily) {
        return cmdSetBaseFamily == IGFX_XE_HP_CORE;
    }

    template <typename WalkerType = DefaultWalkerType>
    static WalkerType getInitGpuWalker() {
        return cmdInitGpgpuWalker;
    }

    template <typename WalkerType = DefaultWalkerType>
    static constexpr size_t getInterfaceDescriptorSize() {
        return sizeof(INTERFACE_DESCRIPTOR_DATA);
    }

    template <typename InterfaceDescriptorType>
    static InterfaceDescriptorType getInitInterfaceDescriptor() {
        return cmdInitInterfaceDescriptorData;
    }

    template <typename WalkerType>
    static constexpr bool isHeaplessMode() {
        return false;
    }

    static constexpr bool isHeaplessRequired() {
        return false;
    }

    template <typename InterfaceDescriptorType>
    static constexpr bool isInterfaceDescriptorHeaplessMode() {
        return false;
    }

    template <typename WalkerType>
    static constexpr auto getPostSyncType() {
        return std::decay_t<POSTSYNC_DATA>{};
    }
};

} // namespace NEO
