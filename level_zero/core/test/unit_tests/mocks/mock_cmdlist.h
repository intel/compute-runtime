/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "level_zero/core/source/cmdlist/cmdlist_hw.h"
#include "level_zero/core/test/unit_tests/mock.h"
#include "level_zero/core/test/unit_tests/mocks/mock_device.h"
#include "level_zero/core/test/unit_tests/white_box.h"

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winconsistent-missing-override"
#endif

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
    using BaseClass::appendLaunchFunctionWithParams;
    using BaseClass::commandListPreemptionMode;

    WhiteBox() : ::L0::CommandListCoreFamily<gfxCoreFamily>() {}
    virtual ~WhiteBox() {}
};

template <GFXCORE_FAMILY gfxCoreFamily>
using CommandListCoreFamily = WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>;

template <>
struct WhiteBox<::L0::CommandList> : public ::L0::CommandListImp {
    using BaseClass = ::L0::CommandListImp;
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

    MOCK_METHOD0(close, ze_result_t());
    MOCK_METHOD0(destroy, ze_result_t());
    MOCK_METHOD2(appendCommandLists,
                 ze_result_t(uint32_t numCommandLists, ze_command_list_handle_t *phCommandLists));
    MOCK_METHOD5(appendLaunchFunction,
                 ze_result_t(ze_kernel_handle_t hFunction,
                             const ze_group_count_t *pThreadGroupDimensions,
                             ze_event_handle_t hEvent, uint32_t numWaitEvents,
                             ze_event_handle_t *phWaitEvents));
    MOCK_METHOD5(appendLaunchCooperativeKernel,
                 ze_result_t(ze_kernel_handle_t hKernel,
                             const ze_group_count_t *pLaunchFuncArgs,
                             ze_event_handle_t hSignalEvent,
                             uint32_t numWaitEvents,
                             ze_event_handle_t *phWaitEvents));
    MOCK_METHOD5(appendLaunchFunctionIndirect,
                 ze_result_t(ze_kernel_handle_t hFunction,
                             const ze_group_count_t *pDispatchArgumentsBuffer,
                             ze_event_handle_t hEvent, uint32_t numWaitEvents,
                             ze_event_handle_t *phWaitEvents));
    MOCK_METHOD7(appendLaunchMultipleFunctionsIndirect,
                 ze_result_t(uint32_t numFunctions, const ze_kernel_handle_t *phFunctions,
                             const uint32_t *pNumLaunchArguments,
                             const ze_group_count_t *pLaunchArgumentsBuffer,
                             ze_event_handle_t hEvent, uint32_t numWaitEvents,
                             ze_event_handle_t *phWaitEvents));
    MOCK_METHOD1(appendEventReset, ze_result_t(ze_event_handle_t hEvent));
    MOCK_METHOD3(appendBarrier, ze_result_t(ze_event_handle_t hSignalEvent, uint32_t numWaitEvents,
                                            ze_event_handle_t *phWaitEvents));
    MOCK_METHOD6(appendMemoryRangesBarrier,
                 ze_result_t(uint32_t numRanges, const size_t *pRangeSizes, const void **pRanges,
                             ze_event_handle_t hSignalEvent, uint32_t numWaitEvents,
                             ze_event_handle_t *phWaitEvents));
    MOCK_METHOD6(appendImageCopyFromMemory,
                 ze_result_t(ze_image_handle_t hDstImage, const void *srcptr,
                             const ze_image_region_t *pDstRegion, ze_event_handle_t hEvent,
                             uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents));
    MOCK_METHOD6(appendImageCopyToMemory,
                 ze_result_t(void *dstptr, ze_image_handle_t hSrcImage,
                             const ze_image_region_t *pSrcRegion, ze_event_handle_t hEvent,
                             uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents));
    MOCK_METHOD7(appendImageCopyRegion,
                 ze_result_t(ze_image_handle_t hDstImage, ze_image_handle_t hSrcImage,
                             const ze_image_region_t *pDstRegion, const ze_image_region_t *pSrcRegion,
                             ze_event_handle_t hSignalEvent, uint32_t numWaitEvents,
                             ze_event_handle_t *phWaitEvents));
    MOCK_METHOD5(appendImageCopy,
                 ze_result_t(ze_image_handle_t hDstImage, ze_image_handle_t hSrcImage,
                             ze_event_handle_t hEvent, uint32_t numWaitEvents,
                             ze_event_handle_t *phWaitEvents));
    MOCK_METHOD4(appendMemAdvise, ze_result_t(ze_device_handle_t hDevice, const void *ptr,
                                              size_t size, ze_memory_advice_t advice));
    MOCK_METHOD6(appendMemoryCopy, ze_result_t(void *dstptr, const void *srcptr, size_t size,
                                               ze_event_handle_t hEvent, uint32_t numWaitEvents,
                                               ze_event_handle_t *phWaitEvents));
    MOCK_METHOD4(appendPageFaultCopy, ze_result_t(NEO::GraphicsAllocation *dstptr, NEO::GraphicsAllocation *srcptr, size_t size, bool flushHost));
    MOCK_METHOD9(appendMemoryCopyRegion, ze_result_t(void *dstptr,
                                                     const ze_copy_region_t *dstRegion,
                                                     uint32_t dstPitch,
                                                     uint32_t dstSlicePitch,
                                                     const void *srcptr,
                                                     const ze_copy_region_t *srcRegion,
                                                     uint32_t srcPitch,
                                                     uint32_t srcSlicePitch,
                                                     ze_event_handle_t hSignalEvent));
    MOCK_METHOD2(appendMemoryPrefetch, ze_result_t(const void *ptr, size_t count));
    MOCK_METHOD5(appendMemoryFill,
                 ze_result_t(void *ptr, const void *pattern,
                             size_t pattern_size, size_t size, ze_event_handle_t hEvent));
    MOCK_METHOD1(appendSignalEvent, ze_result_t(ze_event_handle_t hEvent));
    MOCK_METHOD2(appendWaitOnEvents, ze_result_t(uint32_t numEvents, ze_event_handle_t *phEvent));
    MOCK_METHOD2(reserveSpace, ze_result_t(size_t size, void **ptr));
    MOCK_METHOD0(reset, ze_result_t());
    MOCK_METHOD0(resetParameters, ze_result_t());

    MOCK_METHOD0(appendMetricMemoryBarrier, ze_result_t());
    MOCK_METHOD2(appendMetricTracerMarker,
                 ze_result_t(zet_metric_tracer_handle_t hMetricTracer, uint32_t value));
    MOCK_METHOD1(appendMetricQueryBegin, ze_result_t(zet_metric_query_handle_t hMetricQuery));
    MOCK_METHOD2(appendMetricQueryEnd, ze_result_t(zet_metric_query_handle_t hMetricQuery,
                                                   ze_event_handle_t hCompletionEvent));
    MOCK_METHOD2(appendMILoadRegImm, ze_result_t(uint32_t reg, uint32_t value));
    MOCK_METHOD2(appendMILoadRegReg, ze_result_t(uint32_t reg1, uint32_t reg2));
    MOCK_METHOD2(appendMILoadRegMem, ze_result_t(uint32_t reg1, uint64_t address));
    MOCK_METHOD2(appendMIStoreRegMem, ze_result_t(uint32_t reg1, uint64_t address));
    MOCK_METHOD2(appendMIMath, ze_result_t(void *aluArray, size_t aluCount));
    MOCK_METHOD3(appendMIBBStart, ze_result_t(uint64_t address, size_t predication, bool secondLevel));
    MOCK_METHOD0(appendMIBBEnd, ze_result_t());
    MOCK_METHOD0(appendMINoop, ze_result_t());
    MOCK_METHOD1(executeCommandListImmediate, ze_result_t(bool perforMigration));
    MOCK_METHOD1(initialize, bool(L0::Device *device));

    uint8_t *batchBuffer = nullptr;
    NEO::GraphicsAllocation *mockAllocation = nullptr;
};

} // namespace ult
} // namespace L0
