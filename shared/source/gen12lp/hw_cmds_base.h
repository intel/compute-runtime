/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/commands/bxml_generator_glue.h"
#include "shared/source/gen12lp/hw_info.h"
#include "shared/source/helpers/debug_helpers.h"

#include "igfxfmid.h"

#include <cstddef>
#include <type_traits>

template <class T>
struct CmdParse;

namespace NEO {
class LogicalStateHelper;
struct Gen12Lp {
#include "shared/source/generated/gen12lp/hw_cmds_generated_gen12lp.inl"

    static constexpr bool supportsSampler = true;
    static constexpr bool isUsingGenericMediaStateClear = true;
    static constexpr uint32_t stateComputeModeForceNonCoherentMask = (0b11u << 3);
    static constexpr bool isUsingMiMemFence = false;

    struct FrontEndStateSupport {
        static constexpr bool scratchSize = true;
        static constexpr bool privateScratchSize = false;
        static constexpr bool computeDispatchAllWalker = false;
        static constexpr bool disableEuFusion = true;
        static constexpr bool disableOverdispatch = false;
        static constexpr bool singleSliceDispatchCcsMode = false;
    };

    struct StateComputeModeStateSupport {
        static constexpr bool threadArbitrationPolicy = false;
        static constexpr bool coherencyRequired = true;
        static constexpr bool largeGrfMode = false;
        static constexpr bool zPassAsyncComputeThreadLimit = false;
        static constexpr bool pixelAsyncComputeThreadLimit = false;
        static constexpr bool devicePreemptionMode = false;
    };

    struct StateBaseAddressStateSupport {
        static constexpr bool globalAtomics = false;
        static constexpr bool statelessMocs = true;
    };

    struct PipelineSelectStateSupport {
        static constexpr bool modeSelected = true;
        static constexpr bool mediaSamplerDopClockGate = true;
        static constexpr bool systolicMode = false;
    };

    struct PreemptionDebugSupport {
        static constexpr bool preemptionMode = true;
        static constexpr bool stateSip = true;
        static constexpr bool csrSurface = true;
    };

    struct DataPortBindlessSurfaceExtendedMessageDescriptor {
        union {
            struct {
                uint32_t bindlessSurfaceOffset : 20;
                uint32_t reserved : 1;
                uint32_t executionUnitExtendedMessageDescriptorDefinition : 11;
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
            return bindlessSurfaceOffset << 12;
        }
    };

    static_assert(sizeof(DataPortBindlessSurfaceExtendedMessageDescriptor) == sizeof(DataPortBindlessSurfaceExtendedMessageDescriptor::packed), "");
};
struct Gen12LpFamily : public Gen12Lp {
    using PARSE = CmdParse<Gen12LpFamily>;
    using GfxFamily = Gen12LpFamily;
    using WALKER_TYPE = GPGPU_WALKER;
    using VFE_STATE_TYPE = MEDIA_VFE_STATE;
    using XY_COPY_BLT = typename GfxFamily::XY_BLOCK_COPY_BLT;
    using XY_COLOR_BLT = typename GfxFamily::XY_FAST_COLOR_BLT;
    using MI_STORE_REGISTER_MEM_CMD = typename GfxFamily::MI_STORE_REGISTER_MEM;
    using TimestampPacketType = uint32_t;
    using LogicalStateHelperHw = LogicalStateHelper;
    static const GPGPU_WALKER cmdInitGpgpuWalker;
    static const INTERFACE_DESCRIPTOR_DATA cmdInitInterfaceDescriptorData;
    static const MEDIA_INTERFACE_DESCRIPTOR_LOAD cmdInitMediaInterfaceDescriptorLoad;
    static const MEDIA_STATE_FLUSH cmdInitMediaStateFlush;
    static const MI_BATCH_BUFFER_END cmdInitBatchBufferEnd;
    static const MI_BATCH_BUFFER_START cmdInitBatchBufferStart;
    static const PIPE_CONTROL cmdInitPipeControl;
    static const STATE_COMPUTE_MODE cmdInitStateComputeMode;
    static const MI_SEMAPHORE_WAIT cmdInitMiSemaphoreWait;
    static const RENDER_SURFACE_STATE cmdInitRenderSurfaceState;
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
    static const MEDIA_VFE_STATE cmdInitMediaVfeState;
    static const STATE_BASE_ADDRESS cmdInitStateBaseAddress;
    static const MEDIA_SURFACE_STATE cmdInitMediaSurfaceState;
    static const SAMPLER_STATE cmdInitSamplerState;
    static const GPGPU_CSR_BASE_ADDRESS cmdInitGpgpuCsrBaseAddress;
    static const STATE_SIP cmdInitStateSip;
    static const BINDING_TABLE_STATE cmdInitBindingTableState;
    static const MI_USER_INTERRUPT cmdInitUserInterrupt;
    static const L3_CONTROL cmdInitL3ControlWithoutPostSync;
    static const L3_CONTROL cmdInitL3ControlWithPostSync;
    static const XY_BLOCK_COPY_BLT cmdInitXyBlockCopyBlt;
    static const XY_COPY_BLT cmdInitXyCopyBlt;
    static const MI_FLUSH_DW cmdInitMiFlushDw;
    static const XY_FAST_COLOR_BLT cmdInitXyColorBlt;

    static constexpr bool supportsCmdSet(GFXCORE_FAMILY cmdSetBaseFamily) {
        return cmdSetBaseFamily == IGFX_GEN8_CORE;
    }
};

} // namespace NEO
