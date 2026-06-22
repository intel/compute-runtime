/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/debug_helpers.h"

#include "level_zero/api/opencl/test/common/fixtures/capturing_command_list_args.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"

#include <cstdint>
#include <utility>
#include <vector>

namespace NEO {
namespace LEO {
namespace ult {

#define LEO_CAPTURED_APIS(MACRO)                              \
    MACRO(appendMemoryCopy, AppendMemoryCopyArgs)             \
    MACRO(appendMemoryCopyRegion, AppendMemoryCopyRegionArgs) \
    MACRO(appendMemoryFill, AppendMemoryFillArgs)             \
    MACRO(appendBarrier, AppendBarrierArgs)                   \
    MACRO(hostSynchronize, HostSynchronizeArgs)

enum class ApiId : uint32_t {
#define LEO_CAPTURED_API_ID(name, argsType) name,
    LEO_CAPTURED_APIS(LEO_CAPTURED_API_ID)
#undef LEO_CAPTURED_API_ID
};

template <typename Args>
struct CapturedApi {
    size_t count() const { return records.size(); }
    bool wasCalled() const { return records.empty() == false; }

    const Args &operator[](size_t index) const { return records.at(index); }

    std::vector<Args> records{};
};
struct CapturingCommandList : public L0::ult::Mock<L0::ult::CommandList> {
    using BaseClass = L0::ult::Mock<L0::ult::CommandList>;

    CapturingCommandList() : BaseClass(nullptr) {}

    size_t totalCalls() const { return sequence.size(); }

    void clearCaptures() {
        sequence.clear();
#define LEO_CAPTURED_API_CLEAR(name, argsType) name##Args.records.clear();
        LEO_CAPTURED_APIS(LEO_CAPTURED_API_CLEAR)
#undef LEO_CAPTURED_API_CLEAR
    }

    std::vector<ApiId> sequence{};

#define LEO_CAPTURED_API_MEMBER(name, argsType) CapturedApi<argsType> name##Args{};
    LEO_CAPTURED_APIS(LEO_CAPTURED_API_MEMBER)
#undef LEO_CAPTURED_API_MEMBER

    ze_result_t appendMemoryCopy(void *dstptr, const void *srcptr, size_t size,
                                 ze_event_handle_t hEvent, uint32_t numWaitEvents,
                                 ze_event_handle_t *phWaitEvents,
                                 L0::CmdListMemoryCopyParams &memoryCopyParams) override {
        auto result = BaseClass::appendMemoryCopy(dstptr, srcptr, size, hEvent, numWaitEvents, phWaitEvents, memoryCopyParams);
        return record(this->appendMemoryCopyArgs, ApiId::appendMemoryCopy,
                      AppendMemoryCopyArgs{dstptr, srcptr, size, hEvent, numWaitEvents, phWaitEvents}, result);
    }

    ze_result_t appendMemoryCopyRegion(void *dstptr, const ze_copy_region_t *dstRegion, uint32_t dstPitch,
                                       uint32_t dstSlicePitch, const void *srcptr, const ze_copy_region_t *srcRegion,
                                       uint32_t srcPitch, uint32_t srcSlicePitch, ze_event_handle_t hSignalEvent,
                                       uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents,
                                       L0::CmdListMemoryCopyParams &memoryCopyParams) override {
        auto result = BaseClass::appendMemoryCopyRegion(dstptr, dstRegion, dstPitch, dstSlicePitch, srcptr, srcRegion,
                                                        srcPitch, srcSlicePitch, hSignalEvent, numWaitEvents, phWaitEvents, memoryCopyParams);
        return record(this->appendMemoryCopyRegionArgs, ApiId::appendMemoryCopyRegion,
                      AppendMemoryCopyRegionArgs{dstptr, dstRegion, dstPitch, dstSlicePitch, srcptr,
                                                 srcRegion, srcPitch, srcSlicePitch, hSignalEvent,
                                                 numWaitEvents, phWaitEvents},
                      result);
    }

    ze_result_t appendMemoryFill(void *ptr, const void *pattern, size_t patternSize, size_t size,
                                 ze_event_handle_t hEvent, uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents,
                                 L0::CmdListMemoryCopyParams &memoryCopyParams) override {
        auto result = BaseClass::appendMemoryFill(ptr, pattern, patternSize, size, hEvent, numWaitEvents, phWaitEvents, memoryCopyParams);
        return record(this->appendMemoryFillArgs, ApiId::appendMemoryFill,
                      AppendMemoryFillArgs{ptr, pattern, patternSize, size, hEvent, numWaitEvents, phWaitEvents},
                      result);
    }

    ze_result_t appendBarrier(ze_event_handle_t hSignalEvent, uint32_t numWaitEvents,
                              ze_event_handle_t *phWaitEvents, bool relaxedOrderingDispatch) override {
        auto result = BaseClass::appendBarrier(hSignalEvent, numWaitEvents, phWaitEvents, relaxedOrderingDispatch);
        return record(this->appendBarrierArgs, ApiId::appendBarrier,
                      AppendBarrierArgs{hSignalEvent, numWaitEvents, phWaitEvents, relaxedOrderingDispatch}, result);
    }

    ze_result_t hostSynchronize(uint64_t timeout) override {
        auto result = BaseClass::hostSynchronize(timeout);
        return record(this->hostSynchronizeArgs, ApiId::hostSynchronize, HostSynchronizeArgs{timeout}, result);
    }

  protected:
    template <typename Args>
    ze_result_t record(CapturedApi<Args> &api, ApiId id, Args args, ze_result_t baseResult) {
        api.records.push_back(std::move(args));
        sequence.push_back(id);
        return baseResult;
    }
};

#undef LEO_CAPTURED_APIS

} // namespace ult
} // namespace LEO
} // namespace NEO
