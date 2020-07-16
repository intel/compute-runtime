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
    using BaseClass::commandListPreemptionMode;
    using BaseClass::getAlignedAllocation;
    using BaseClass::getAllocationFromHostPtrMap;
    using BaseClass::getHostPtrAlloc;
    using BaseClass::hostPtrMap;

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

template <>
struct Mock<CommandList> : public CommandList {
    Mock(Device *device = nullptr);
    ~Mock() override;

    MOCK_METHOD(ze_result_t,
                close,
                (),
                (override));
    MOCK_METHOD(ze_result_t,
                destroy,
                (),
                (override));
    MOCK_METHOD(ze_result_t,
                appendLaunchKernel,
                (ze_kernel_handle_t hFunction,
                 const ze_group_count_t *pThreadGroupDimensions,
                 ze_event_handle_t hEvent, uint32_t numWaitEvents,
                 ze_event_handle_t *phWaitEvents),
                (override));
    MOCK_METHOD(ze_result_t,
                appendLaunchCooperativeKernel,
                (ze_kernel_handle_t hKernel,
                 const ze_group_count_t *pLaunchFuncArgs,
                 ze_event_handle_t hSignalEvent,
                 uint32_t numWaitEvents,
                 ze_event_handle_t *phWaitEvents),
                (override));
    MOCK_METHOD(ze_result_t,
                appendLaunchKernelIndirect,
                (ze_kernel_handle_t hFunction,
                 const ze_group_count_t *pDispatchArgumentsBuffer,
                 ze_event_handle_t hEvent,
                 uint32_t numWaitEvents,
                 ze_event_handle_t *phWaitEvents),
                (override));
    MOCK_METHOD(ze_result_t,
                appendLaunchMultipleKernelsIndirect,
                (uint32_t numFunctions,
                 const ze_kernel_handle_t *phFunctions,
                 const uint32_t *pNumLaunchArguments,
                 const ze_group_count_t *pLaunchArgumentsBuffer,
                 ze_event_handle_t hEvent,
                 uint32_t numWaitEvents,
                 ze_event_handle_t *phWaitEvents),
                (override));
    MOCK_METHOD(ze_result_t,
                appendEventReset,
                (ze_event_handle_t hEvent),
                (override));
    MOCK_METHOD(ze_result_t,
                appendBarrier,
                (ze_event_handle_t hSignalEvent,
                 uint32_t numWaitEvents,
                 ze_event_handle_t *phWaitEvents),
                (override));
    MOCK_METHOD(ze_result_t,
                appendMemoryRangesBarrier,
                (uint32_t numRanges,
                 const size_t *pRangeSizes,
                 const void **pRanges,
                 ze_event_handle_t hSignalEvent,
                 uint32_t numWaitEvents,
                 ze_event_handle_t *phWaitEvents),
                (override));
    MOCK_METHOD(ze_result_t,
                appendImageCopyFromMemory,
                (ze_image_handle_t hDstImage,
                 const void *srcptr,
                 const ze_image_region_t *pDstRegion,
                 ze_event_handle_t hEvent,
                 uint32_t numWaitEvents,
                 ze_event_handle_t *phWaitEvents),
                (override));
    MOCK_METHOD(ze_result_t,
                appendImageCopyToMemory,
                (void *dstptr,
                 ze_image_handle_t hSrcImage,
                 const ze_image_region_t *pSrcRegion,
                 ze_event_handle_t hEvent,
                 uint32_t numWaitEvents,
                 ze_event_handle_t *phWaitEvents),
                (override));
    MOCK_METHOD(ze_result_t,
                appendImageCopyRegion,
                (ze_image_handle_t hDstImage,
                 ze_image_handle_t hSrcImage,
                 const ze_image_region_t *pDstRegion,
                 const ze_image_region_t *pSrcRegion,
                 ze_event_handle_t hSignalEvent,
                 uint32_t numWaitEvents,
                 ze_event_handle_t *phWaitEvents),
                (override));
    MOCK_METHOD(ze_result_t,
                appendImageCopy,
                (ze_image_handle_t hDstImage,
                 ze_image_handle_t hSrcImage,
                 ze_event_handle_t hEvent,
                 uint32_t numWaitEvents,
                 ze_event_handle_t *phWaitEvents),
                (override));
    MOCK_METHOD(ze_result_t,
                appendMemAdvise,
                (ze_device_handle_t hDevice,
                 const void *ptr,
                 size_t size,
                 ze_memory_advice_t advice),
                (override));
    MOCK_METHOD(ze_result_t,
                appendMemoryCopy,
                (void *dstptr,
                 const void *srcptr,
                 size_t size,
                 ze_event_handle_t hEvent,
                 uint32_t numWaitEvents,
                 ze_event_handle_t *phWaitEvents),
                (override));
    MOCK_METHOD(ze_result_t,
                appendPageFaultCopy,
                (NEO::GraphicsAllocation * dstptr,
                 NEO::GraphicsAllocation *srcptr,
                 size_t size,
                 bool flushHost),
                (override));
    MOCK_METHOD(ze_result_t,
                appendMemoryCopyRegion,
                (void *dstptr,
                 const ze_copy_region_t *dstRegion,
                 uint32_t dstPitch,
                 uint32_t dstSlicePitch,
                 const void *srcptr,
                 const ze_copy_region_t *srcRegion,
                 uint32_t srcPitch,
                 uint32_t srcSlicePitch,
                 ze_event_handle_t hSignalEvent),
                (override));
    MOCK_METHOD(ze_result_t,
                appendMemoryPrefetch,
                (const void *ptr,
                 size_t count),
                (override));
    MOCK_METHOD(ze_result_t,
                appendMemoryFill,
                (void *ptr,
                 const void *pattern,
                 size_t pattern_size,
                 size_t size,
                 ze_event_handle_t hEvent),
                (override));
    MOCK_METHOD(ze_result_t,
                appendSignalEvent,
                (ze_event_handle_t hEvent),
                (override));
    MOCK_METHOD(ze_result_t,
                appendWaitOnEvents,
                (uint32_t numEvents,
                 ze_event_handle_t *phEvent),
                (override));
    MOCK_METHOD(ze_result_t,
                appendWriteGlobalTimestamp,
                (uint64_t * dstptr,
                 ze_event_handle_t hSignalEvent,
                 uint32_t numWaitEvents,
                 ze_event_handle_t *phWaitEvents),
                (override));
    MOCK_METHOD(ze_result_t,
                reserveSpace,
                (size_t size,
                 void **ptr),
                (override));
    MOCK_METHOD(ze_result_t,
                reset,
                (),
                (override));
    MOCK_METHOD(ze_result_t,
                appendMetricMemoryBarrier,
                (),
                (override));
    MOCK_METHOD(ze_result_t,
                appendMetricTracerMarker,
                (zet_metric_tracer_handle_t hMetricTracer,
                 uint32_t value),
                (override));
    MOCK_METHOD(ze_result_t,
                appendMetricQueryBegin,
                (zet_metric_query_handle_t hMetricQuery),
                (override));
    MOCK_METHOD(ze_result_t,
                appendMetricQueryEnd,
                (zet_metric_query_handle_t hMetricQuery,
                 ze_event_handle_t hCompletionEvent),
                (override));
    MOCK_METHOD(ze_result_t,
                appendMILoadRegImm,
                (uint32_t reg,
                 uint32_t value),
                (override));
    MOCK_METHOD(ze_result_t,
                appendMILoadRegReg,
                (uint32_t reg1,
                 uint32_t reg2),
                (override));
    MOCK_METHOD(ze_result_t,
                appendMILoadRegMem,
                (uint32_t reg1,
                 uint64_t address),
                (override));
    MOCK_METHOD(ze_result_t,
                appendMIStoreRegMem,
                (uint32_t reg1,
                 uint64_t address),
                (override));
    MOCK_METHOD(ze_result_t,
                appendMIMath,
                (void *aluArray,
                 size_t aluCount),
                (override));
    MOCK_METHOD(ze_result_t,
                appendMIBBStart,
                (uint64_t address,
                 size_t predication,
                 bool secondLevel),
                (override));
    MOCK_METHOD(ze_result_t,
                appendMIBBEnd,
                (),
                (override));
    MOCK_METHOD(ze_result_t,
                appendMINoop,
                (),
                (override));
    MOCK_METHOD(ze_result_t,
                executeCommandListImmediate,
                (bool perforMigration),
                (override));
    MOCK_METHOD(bool,
                initialize,
                (L0::Device * device,
                 bool onlyCopyBlit),
                (override));

    uint8_t *batchBuffer = nullptr;
    NEO::GraphicsAllocation *mockAllocation = nullptr;
};

} // namespace ult
} // namespace L0
