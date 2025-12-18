/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/commands/bxml_generator_glue.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/helpers/is_pod_v.h"

#include "neo_igfxfmid.h"

#include <cstdint>
#include <cstring>
#include <type_traits>

template <class T>
struct CmdParse;
namespace NEO {

struct Xe3pCore {
#include "shared/source/generated/xe3p_core/hw_cmds_generated_xe3p_core.inl"

    static constexpr uint32_t stateComputeModeLSCSamplerBackingThresholdMask = (0b11u << 5);
    static constexpr uint32_t stateComputeModeEnableVariableRegisterSizeAllocationMask = (1u << 10);
    static constexpr uint32_t stateComputeModePipelinedEuThreadArbitrationMask = (1u << 12);
    static constexpr uint32_t stateComputeModeEuThreadSchedulingModeOverrideMask = (0b11u << 13);
    static constexpr uint32_t stateComputeModeLargeGrfModeMask = (1u << 15);
    // DW2
    static constexpr uint32_t stateComputeModeUavCoherencyModeMask = (1u << 6);
    static constexpr uint32_t stateComputeModeEnableOutOfBoundariesInTranslationExceptionMask = (1u << 7);
    static constexpr uint32_t stateComputeModePageFaultExceptionEnableMask = (1u << 9);
    static constexpr uint32_t stateComputeModeSystemMemoryReadFenceEnableMask = (1u << 11);
    static constexpr uint32_t stateComputeModeMemoryExceptionEnableMask = (1u << 13);
    static constexpr uint32_t stateComputeBreakpointsEnableMask = (1u << 14);
    static constexpr uint32_t stateComputeModeForceExternalHaltAndForceExceptionEnableMask = (1u << 15);
    static constexpr uint32_t bcsEngineCount = 9u;
    static constexpr uint32_t timestampPacketCount = 16u;

    static constexpr bool isUsingL3Control = false;
    static constexpr bool supportsSampler = true;
    static constexpr bool isUsingGenericMediaStateClear = false;
    static constexpr bool isUsingMiMemFence = true;
    static constexpr bool isUsingMiSetPredicate = true;
    static constexpr bool isUsingMiMathMocs = true;

    struct FrontEndStateSupport {
        static constexpr bool scratchSize = true;
        static constexpr bool privateScratchSize = true;
        static constexpr bool computeDispatchAllWalker = false;
        static constexpr bool disableEuFusion = false;
        static constexpr bool disableOverdispatch = true;
        static constexpr bool singleSliceDispatchCcsMode = false;
    };

    struct StateComputeModeStateSupport {
        static constexpr bool threadArbitrationPolicy = false;
        static constexpr bool coherencyRequired = true;
        static constexpr bool largeGrfMode = true;
        static constexpr bool zPassAsyncComputeThreadLimit = false;
        static constexpr bool pixelAsyncComputeThreadLimit = false;
        static constexpr bool devicePreemptionMode = false;
        static constexpr bool lscSamplerBackingThreshold = false;

        static constexpr bool allocationForScratchAndMidthreadPreemption = true;
        static constexpr bool enableVariableRegisterSizeAllocation = true;
        static constexpr bool enableL1FlushUavCoherencyMode = false;
        static constexpr bool enablePageFaultException = false;
        static constexpr bool enableSystemMemoryReadFence = false;
        static constexpr bool enableMemoryException = false;
        static constexpr bool enableBreakpoints = false;
        static constexpr bool enableForceExternalHaltAndForceException = false;
    };

    struct StateBaseAddressStateSupport {
        static constexpr bool bindingTablePoolBaseAddress = true;
    };

    struct PipelineSelectStateSupport {
        static constexpr bool systolicMode = false;
    };

    struct PreemptionDebugSupport {
        static constexpr bool preemptionMode = false;
        static constexpr bool stateSip = true;
        static constexpr bool csrSurface = true;
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

    static constexpr uint32_t cacheLineSize = 0x100;
    static_assert(sizeof(DataPortBindlessSurfaceExtendedMessageDescriptor) == sizeof(DataPortBindlessSurfaceExtendedMessageDescriptor::packed), "");
};

struct Xe3pCoreFamily : public Xe3pCore {
    using Parse = CmdParse<Xe3pCoreFamily>;
    using GfxFamily = Xe3pCoreFamily;
    using DefaultWalkerType = COMPUTE_WALKER_2;
    using PorWalkerType = COMPUTE_WALKER_2;
    using FrontEndStateCommand = CFE_STATE;
    using XY_BLOCK_COPY_BLT = typename GfxFamily::XY_BLOCK_COPY_BLT;
    using XY_COPY_BLT = typename GfxFamily::MEM_COPY;
    using XY_COLOR_BLT = typename GfxFamily::XY_FAST_COLOR_BLT;
    using MI_STORE_REGISTER_MEM_CMD = typename GfxFamily::MI_STORE_REGISTER_MEM;
    using TimestampPacketType = uint64_t;
    using StallingBarrierType = PIPE_CONTROL;
    static const COMPUTE_WALKER_2 cmdInitGpgpuWalker2;
    static const COMPUTE_WALKER cmdInitGpgpuWalker;
    static const CFE_STATE cmdInitCfeState;
    static const INTERFACE_DESCRIPTOR_DATA cmdInitInterfaceDescriptorData;
    static const INTERFACE_DESCRIPTOR_DATA_2 cmdInitInterfaceDescriptorData2;
    static const MI_BATCH_BUFFER_END cmdInitBatchBufferEnd;
    static const MI_BATCH_BUFFER_START cmdInitBatchBufferStart;
    static const PIPE_CONTROL cmdInitPipeControl;
    static const RESOURCE_BARRIER cmdInitResourceBarrier;
    static const STATE_COMPUTE_MODE cmdInitStateComputeMode;
    static const _3DSTATE_BINDING_TABLE_POOL_ALLOC cmdInitStateBindingTablePoolAlloc;
    static const MI_SEMAPHORE_WAIT cmdInitMiSemaphoreWait;
    static const MI_SEMAPHORE_WAIT_64 cmdInitMiSemaphoreWait64;
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
    static const STATE_CONTEXT_DATA_BASE_ADDRESS cmdInitStateContextDataBaseAddress;
    static const STATE_SYSTEM_MEM_FENCE_ADDRESS cmdInitStateSystemMemFenceAddress;
    static constexpr bool isQwordInOrderCounter = true;
    static constexpr bool walkerPostSyncSupport = true;
    static constexpr bool samplerArbitrationControl = true;
    static constexpr size_t indirectDataAlignment = MemoryConstants::cacheLineSize;
    static constexpr GFXCORE_FAMILY gfxCoreFamily = IGFX_XE3P_CORE;

    static constexpr bool supportsCmdSet(GFXCORE_FAMILY cmdSetBaseFamily) {
        return cmdSetBaseFamily == IGFX_XE_HP_CORE;
    }

    template <typename WalkerType = DefaultWalkerType>
    static constexpr size_t getInterfaceDescriptorSize() {
        if constexpr (std::is_same_v<WalkerType, COMPUTE_WALKER_2>) {
            return sizeof(INTERFACE_DESCRIPTOR_DATA_2);
        } else {
            return sizeof(INTERFACE_DESCRIPTOR_DATA);
        }
    }

    template <typename WalkerType = DefaultWalkerType>
    static WalkerType getInitGpuWalker() {
        if constexpr (std::is_same_v<WalkerType, COMPUTE_WALKER_2>) {
            return cmdInitGpgpuWalker2;
        } else {
            return cmdInitGpgpuWalker;
        }
    }

    template <typename InterfaceDescriptorType>
    static InterfaceDescriptorType getInitInterfaceDescriptor() {
        if constexpr (std::is_same_v<InterfaceDescriptorType, INTERFACE_DESCRIPTOR_DATA_2>) {
            return cmdInitInterfaceDescriptorData2;
        } else {
            return cmdInitInterfaceDescriptorData;
        }
    }

    template <typename WalkerType>
    static constexpr bool isHeaplessMode() {
        if constexpr (std::is_same_v<WalkerType, COMPUTE_WALKER_2>) {
            return true;
        } else {
            return false;
        }
    }

    static constexpr bool isHeaplessRequired() {
        return true;
    }

    template <typename InterfaceDescriptorType>
    static constexpr bool isInterfaceDescriptorHeaplessMode() {
        if constexpr (std::is_same_v<InterfaceDescriptorType, INTERFACE_DESCRIPTOR_DATA_2>) {
            return true;
        } else {
            return false;
        }
    }

    template <typename WalkerType>
    static constexpr auto getPostSyncType() {
        return std::decay_t<std::conditional_t<
            std::is_same_v<WalkerType, COMPUTE_WALKER_2>,
            POSTSYNC_DATA_2,
            POSTSYNC_DATA>>{};
    }

    template <typename PostSyncType>
    static constexpr bool isPostSyncData1() {
        return std::is_same_v<PostSyncType, POSTSYNC_DATA>;
    }
};

enum class MemoryCompressionState;

template <typename GfxFamily>
void setSbaStatelessCompressionParams(typename GfxFamily::STATE_BASE_ADDRESS *stateBaseAddress,
                                      MemoryCompressionState memoryCompressionState);
template <>
void setSbaStatelessCompressionParams<Xe3pCoreFamily>(typename Xe3pCoreFamily::STATE_BASE_ADDRESS *stateBaseAddress,
                                                      MemoryCompressionState memoryCompressionState);
} // namespace NEO
