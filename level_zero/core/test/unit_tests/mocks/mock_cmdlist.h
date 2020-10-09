/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "level_zero/core/source/cmdlist/cmdlist_hw.h"
#include "level_zero/core/source/cmdlist/cmdlist_hw_immediate.h"
#include "level_zero/core/test/unit_tests/mock.h"
#include "level_zero/core/test/unit_tests/mocks/mock_device.h"
#include "level_zero/core/test/unit_tests/white_box.h"

namespace NEO {
class GraphicsAllocation;
}

namespace L0 {
struct Device;

namespace ult {

template <GFXCORE_FAMILY gfxCoreFamily>
struct WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>
    : public ::L0::CommandListCoreFamily<gfxCoreFamily> {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    using BaseClass = ::L0::CommandListCoreFamily<gfxCoreFamily>;
    using BaseClass::appendBlitFill;
    using BaseClass::appendCopyImageBlit;
    using BaseClass::appendEventForProfiling;
    using BaseClass::appendEventForProfilingCopyCommand;
    using BaseClass::appendLaunchKernelWithParams;
    using BaseClass::appendMemoryCopyBlit;
    using BaseClass::appendMemoryCopyBlitRegion;
    using BaseClass::appendSignalEventPostWalker;
    using BaseClass::applyMemoryRangesBarrier;
    using BaseClass::commandListPerThreadScratchSize;
    using BaseClass::commandListPreemptionMode;
    using BaseClass::getAlignedAllocation;
    using BaseClass::getAllocationFromHostPtrMap;
    using BaseClass::getHostPtrAlloc;
    using BaseClass::hostPtrMap;
    using BaseClass::initialize;

    WhiteBox() : ::L0::CommandListCoreFamily<gfxCoreFamily>(BaseClass::defaultNumIddsPerBlock) {}
};

template <GFXCORE_FAMILY gfxCoreFamily>
using CommandListCoreFamily = WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>;

template <GFXCORE_FAMILY gfxCoreFamily>
struct WhiteBox<L0::CommandListCoreFamilyImmediate<gfxCoreFamily>>
    : public L0::CommandListCoreFamilyImmediate<gfxCoreFamily> {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    using BaseClass = L0::CommandListCoreFamilyImmediate<gfxCoreFamily>;

    WhiteBox() : BaseClass(BaseClass::defaultNumIddsPerBlock) {}
};

template <>
struct WhiteBox<::L0::CommandList> : public ::L0::CommandListImp {
    using BaseClass = ::L0::CommandListImp;
    using BaseClass::BaseClass;
    using BaseClass::commandContainer;
    using BaseClass::commandListPreemptionMode;
    using BaseClass::initialize;

    WhiteBox(Device *device);
    ~WhiteBox() override;
};

using CommandList = WhiteBox<::L0::CommandList>;

#define ADDMETHOD_NOBASE(funcName, retType, defaultReturn, funcParams) \
    retType funcName##Result = defaultReturn;                          \
    uint32_t funcName##Called = 0u;                                    \
    retType funcName funcParams override {                             \
        funcName##Called++;                                            \
        return funcName##Result;                                       \
    }

#define ADDMETHOD(funcName, retType, callBase, defaultReturn, funcParams, invokeParams) \
    retType funcName##Result = defaultReturn;                                           \
    bool funcName##CallBase = callBase;                                                 \
    uint32_t funcName##Called = 0u;                                                     \
    retType funcName funcParams override {                                              \
        funcName##Called++;                                                             \
        if (funcName##CallBase) {                                                       \
            return BaseClass::funcName invokeParams;                                    \
        }                                                                               \
        return funcName##Result;                                                        \
    }

struct MockCommandList : public CommandList {
    using BaseClass = CommandList;

    MockCommandList(Device *device = nullptr);
    ~MockCommandList() override;

    ADDMETHOD_NOBASE(close, ze_result_t, ZE_RESULT_SUCCESS, ());
    ADDMETHOD_NOBASE(destroy, ze_result_t, ZE_RESULT_SUCCESS, ());

    ADDMETHOD_NOBASE(appendLaunchKernel, ze_result_t, ZE_RESULT_SUCCESS,
                     (ze_kernel_handle_t hFunction,
                      const ze_group_count_t *pThreadGroupDimensions,
                      ze_event_handle_t hEvent, uint32_t numWaitEvents,
                      ze_event_handle_t *phWaitEvents));

    ADDMETHOD_NOBASE(appendLaunchCooperativeKernel, ze_result_t, ZE_RESULT_SUCCESS,
                     (ze_kernel_handle_t hKernel,
                      const ze_group_count_t *pLaunchFuncArgs,
                      ze_event_handle_t hSignalEvent,
                      uint32_t numWaitEvents,
                      ze_event_handle_t *phWaitEvents));

    ADDMETHOD_NOBASE(appendLaunchKernelIndirect, ze_result_t, ZE_RESULT_SUCCESS,
                     (ze_kernel_handle_t hFunction,
                      const ze_group_count_t *pDispatchArgumentsBuffer,
                      ze_event_handle_t hEvent,
                      uint32_t numWaitEvents,
                      ze_event_handle_t *phWaitEvents));

    ADDMETHOD_NOBASE(appendLaunchMultipleKernelsIndirect, ze_result_t, ZE_RESULT_SUCCESS,
                     (uint32_t numFunctions,
                      const ze_kernel_handle_t *phFunctions,
                      const uint32_t *pNumLaunchArguments,
                      const ze_group_count_t *pLaunchArgumentsBuffer,
                      ze_event_handle_t hEvent,
                      uint32_t numWaitEvents,
                      ze_event_handle_t *phWaitEvents));

    ADDMETHOD_NOBASE(appendEventReset, ze_result_t, ZE_RESULT_SUCCESS,
                     (ze_event_handle_t hEvent));

    ADDMETHOD_NOBASE(appendBarrier, ze_result_t, ZE_RESULT_SUCCESS,
                     (ze_event_handle_t hSignalEvent,
                      uint32_t numWaitEvents,
                      ze_event_handle_t *phWaitEvents));

    ADDMETHOD_NOBASE(appendMemoryRangesBarrier, ze_result_t, ZE_RESULT_SUCCESS,
                     (uint32_t numRanges,
                      const size_t *pRangeSizes,
                      const void **pRanges,
                      ze_event_handle_t hSignalEvent,
                      uint32_t numWaitEvents,
                      ze_event_handle_t *phWaitEvents));

    ADDMETHOD_NOBASE(appendImageCopyFromMemory, ze_result_t, ZE_RESULT_SUCCESS,
                     (ze_image_handle_t hDstImage,
                      const void *srcptr,
                      const ze_image_region_t *pDstRegion,
                      ze_event_handle_t hEvent,
                      uint32_t numWaitEvents,
                      ze_event_handle_t *phWaitEvents));

    ADDMETHOD_NOBASE(appendImageCopyToMemory, ze_result_t, ZE_RESULT_SUCCESS,
                     (void *dstptr,
                      ze_image_handle_t hSrcImage,
                      const ze_image_region_t *pSrcRegion,
                      ze_event_handle_t hEvent,
                      uint32_t numWaitEvents,
                      ze_event_handle_t *phWaitEvents));

    ADDMETHOD_NOBASE(appendImageCopyRegion, ze_result_t, ZE_RESULT_SUCCESS,
                     (ze_image_handle_t hDstImage,
                      ze_image_handle_t hSrcImage,
                      const ze_image_region_t *pDstRegion,
                      const ze_image_region_t *pSrcRegion,
                      ze_event_handle_t hSignalEvent,
                      uint32_t numWaitEvents,
                      ze_event_handle_t *phWaitEvents));

    ADDMETHOD_NOBASE(appendImageCopy, ze_result_t, ZE_RESULT_SUCCESS,
                     (ze_image_handle_t hDstImage,
                      ze_image_handle_t hSrcImage,
                      ze_event_handle_t hEvent,
                      uint32_t numWaitEvents,
                      ze_event_handle_t *phWaitEvents));

    ADDMETHOD_NOBASE(appendMemAdvise, ze_result_t, ZE_RESULT_SUCCESS,
                     (ze_device_handle_t hDevice,
                      const void *ptr,
                      size_t size,
                      ze_memory_advice_t advice));

    ADDMETHOD_NOBASE(appendMemoryCopy, ze_result_t, ZE_RESULT_SUCCESS,
                     (void *dstptr,
                      const void *srcptr,
                      size_t size,
                      ze_event_handle_t hEvent,
                      uint32_t numWaitEvents,
                      ze_event_handle_t *phWaitEvents));

    ADDMETHOD_NOBASE(appendPageFaultCopy, ze_result_t, ZE_RESULT_SUCCESS,
                     (NEO::GraphicsAllocation * dstptr,
                      NEO::GraphicsAllocation *srcptr,
                      size_t size,
                      bool flushHost));

    ADDMETHOD_NOBASE(appendMemoryCopyRegion, ze_result_t, ZE_RESULT_SUCCESS,
                     (void *dstptr,
                      const ze_copy_region_t *dstRegion,
                      uint32_t dstPitch,
                      uint32_t dstSlicePitch,
                      const void *srcptr,
                      const ze_copy_region_t *srcRegion,
                      uint32_t srcPitch,
                      uint32_t srcSlicePitch,
                      ze_event_handle_t hSignalEvent));

    ADDMETHOD_NOBASE(appendMemoryPrefetch, ze_result_t, ZE_RESULT_SUCCESS,
                     (const void *ptr,
                      size_t count));

    ADDMETHOD_NOBASE(appendMemoryFill, ze_result_t, ZE_RESULT_SUCCESS,
                     (void *ptr,
                      const void *pattern,
                      size_t pattern_size,
                      size_t size,
                      ze_event_handle_t hEvent));

    ADDMETHOD_NOBASE(appendSignalEvent, ze_result_t, ZE_RESULT_SUCCESS,
                     (ze_event_handle_t hEvent));

    ADDMETHOD_NOBASE(appendWaitOnEvents, ze_result_t, ZE_RESULT_SUCCESS,
                     (uint32_t numEvents,
                      ze_event_handle_t *phEvent));

    ADDMETHOD_NOBASE(appendWriteGlobalTimestamp, ze_result_t, ZE_RESULT_SUCCESS,
                     (uint64_t * dstptr,
                      ze_event_handle_t hSignalEvent,
                      uint32_t numWaitEvents,
                      ze_event_handle_t *phWaitEvents));

    ADDMETHOD_NOBASE(appendQueryKernelTimestamps, ze_result_t, ZE_RESULT_SUCCESS,
                     (uint32_t numEvents,
                      ze_event_handle_t *phEvents,
                      void *dstptr,
                      const size_t *pOffsets,
                      ze_event_handle_t hSignalEvent,
                      uint32_t numWaitEvents,
                      ze_event_handle_t *phWaitEvents))

    ADDMETHOD_NOBASE(appendMemoryCopyFromContext, ze_result_t, ZE_RESULT_SUCCESS,
                     (void *dstptr,
                      ze_context_handle_t hContextSrc,
                      const void *srcptr,
                      size_t size,
                      ze_event_handle_t hSignalEvent,
                      uint32_t numWaitEvents,
                      ze_event_handle_t *phWaitEvents));

    ADDMETHOD_NOBASE(reserveSpace, ze_result_t, ZE_RESULT_SUCCESS,
                     (size_t size,
                      void **ptr));

    ADDMETHOD_NOBASE(reset, ze_result_t, ZE_RESULT_SUCCESS, ());

    ADDMETHOD_NOBASE(appendMetricMemoryBarrier, ze_result_t, ZE_RESULT_SUCCESS, ());

    ADDMETHOD_NOBASE(appendMetricStreamerMarker, ze_result_t, ZE_RESULT_SUCCESS,
                     (zet_metric_streamer_handle_t hMetricStreamer,
                      uint32_t value));

    ADDMETHOD_NOBASE(appendMetricQueryBegin, ze_result_t, ZE_RESULT_SUCCESS,
                     (zet_metric_query_handle_t hMetricQuery));

    ADDMETHOD_NOBASE(appendMetricQueryEnd, ze_result_t, ZE_RESULT_SUCCESS,
                     (zet_metric_query_handle_t hMetricQuery,
                      ze_event_handle_t hSignalEvent,
                      uint32_t numWaitEvents,
                      ze_event_handle_t *phWaitEvents));

    ADDMETHOD_NOBASE(appendMILoadRegImm, ze_result_t, ZE_RESULT_SUCCESS,
                     (uint32_t reg,
                      uint32_t value));

    ADDMETHOD_NOBASE(appendMILoadRegReg, ze_result_t, ZE_RESULT_SUCCESS,
                     (uint32_t reg1,
                      uint32_t reg2));

    ADDMETHOD_NOBASE(appendMILoadRegMem, ze_result_t, ZE_RESULT_SUCCESS,
                     (uint32_t reg1,
                      uint64_t address));

    ADDMETHOD_NOBASE(appendMIStoreRegMem, ze_result_t, ZE_RESULT_SUCCESS,
                     (uint32_t reg1,
                      uint64_t address));

    ADDMETHOD_NOBASE(appendMIMath, ze_result_t, ZE_RESULT_SUCCESS,
                     (void *aluArray,
                      size_t aluCount));

    ADDMETHOD_NOBASE(appendMIBBStart, ze_result_t, ZE_RESULT_SUCCESS,
                     (uint64_t address,
                      size_t predication,
                      bool secondLevel));

    ADDMETHOD_NOBASE(appendMIBBEnd, ze_result_t, ZE_RESULT_SUCCESS, ());

    ADDMETHOD_NOBASE(appendMINoop, ze_result_t, ZE_RESULT_SUCCESS, ());

    ADDMETHOD_NOBASE(appendPipeControl, ze_result_t, ZE_RESULT_SUCCESS,
                     (void *dstPtr,
                      uint64_t value));

    ADDMETHOD_NOBASE(executeCommandListImmediate, ze_result_t, ZE_RESULT_SUCCESS,
                     (bool perforMigration));

    ADDMETHOD_NOBASE(initialize, ze_result_t, ZE_RESULT_SUCCESS,
                     (L0::Device * device,
                      NEO::EngineGroupType engineGroupType));

    uint8_t *batchBuffer = nullptr;
    NEO::GraphicsAllocation *mockAllocation = nullptr;
};

} // namespace ult
} // namespace L0
