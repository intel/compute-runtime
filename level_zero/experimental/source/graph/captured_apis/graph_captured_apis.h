/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/utilities/stackvec.h"

#include <span>

namespace L0 {

struct Context;
struct Event;

#ifndef RR_CAPTURED_APIS_EXT
#define RR_CAPTURED_APIS_EXT()
#endif

#define RR_CAPTURED_APIS()                                            \
    RR_CAPTURED_API(zeCommandListAppendWriteGlobalTimestamp)          \
    RR_CAPTURED_API(zeCommandListAppendBarrier)                       \
    RR_CAPTURED_API(zeCommandListAppendMemoryRangesBarrier)           \
    RR_CAPTURED_API(zeCommandListAppendMemoryCopy)                    \
    RR_CAPTURED_API(zeCommandListAppendMemoryFill)                    \
    RR_CAPTURED_API(zeCommandListAppendMemoryCopyRegion)              \
    RR_CAPTURED_API(zeCommandListAppendMemoryCopyFromContext)         \
    RR_CAPTURED_API(zeCommandListAppendImageCopy)                     \
    RR_CAPTURED_API(zeCommandListAppendImageCopyRegion)               \
    RR_CAPTURED_API(zeCommandListAppendImageCopyToMemory)             \
    RR_CAPTURED_API(zeCommandListAppendImageCopyFromMemory)           \
    RR_CAPTURED_API(zeCommandListAppendMemoryPrefetch)                \
    RR_CAPTURED_API(zeCommandListAppendMemAdvise)                     \
    RR_CAPTURED_API(zeCommandListAppendSignalEvent)                   \
    RR_CAPTURED_API(zeCommandListAppendWaitOnEvents)                  \
    RR_CAPTURED_API(zeCommandListAppendEventReset)                    \
    RR_CAPTURED_API(zeCommandListAppendQueryKernelTimestamps)         \
    RR_CAPTURED_API(zeCommandListAppendLaunchKernel)                  \
    RR_CAPTURED_API(zeCommandListAppendLaunchCooperativeKernel)       \
    RR_CAPTURED_API(zeCommandListAppendLaunchKernelIndirect)          \
    RR_CAPTURED_API(zeCommandListAppendLaunchMultipleKernelsIndirect) \
    RR_CAPTURED_API(zeCommandListAppendSignalExternalSemaphoreExt)    \
    RR_CAPTURED_API(zeCommandListAppendWaitExternalSemaphoreExt)      \
    RR_CAPTURED_API(zeCommandListAppendImageCopyToMemoryExt)          \
    RR_CAPTURED_API(zeCommandListAppendImageCopyFromMemoryExt)        \
    RR_CAPTURED_APIS_EXT()

enum class CaptureApi {
#define RR_CAPTURED_API(X) X,
    RR_CAPTURED_APIS()
#undef RR_CAPTURED_API
};

struct CommandList;

struct ClosureExternalStorage {
    using EventsWaitListId = int64_t;
    using KernelStateId = int64_t;

    static constexpr EventsWaitListId invalidEventsWaitListId = -1;
    static constexpr KernelStateId invalidKernelStateId = -1;

    EventsWaitListId registerEventsWaitList(ze_event_handle_t *begin, ze_event_handle_t *end) {
        if (begin == end) {
            return invalidEventsWaitListId;
        }
        auto ret = waitEvents.size();
        waitEvents.insert(std::end(waitEvents), begin, end);
        return static_cast<EventsWaitListId>(ret);
    }

    KernelStateId registerKernelState(KernelMutableState &&state) {
        auto ret = kernelStates.size();
        kernelStates.push_back(std::move(state));
        return static_cast<KernelStateId>(ret);
    }

    ze_event_handle_t *getEventsWaitList(EventsWaitListId id) {
        if (id < 0) {
            return nullptr;
        }
        return waitEvents.data() + id;
    }

    KernelMutableState *getKernelMutableState(KernelStateId id) {
        if (id < 0) {
            return nullptr;
        }
        return kernelStates.data() + id;
    }

  protected:
    std::vector<ze_event_handle_t> waitEvents;
    std::vector<KernelMutableState> kernelStates;
};

template <CaptureApi api>
struct Closure {
    static constexpr bool isSupported = false;

    struct ApiArgs {
        template <typename ArgsT>
        ApiArgs(ArgsT...) {}

        ze_event_handle_t hSignalEvent = nullptr;
        uint32_t numWaitEvents = 0;
        ze_event_handle_t *phWaitEvents = nullptr;
    };

    Closure(const ApiArgs &apiArgs, ClosureExternalStorage &externalStorage) {}

    ze_result_t instantiateTo(CommandList &executionTarget, ClosureExternalStorage &externalStorage) const {
        DEBUG_BREAK_IF(true);
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }
};

template <class ApiArgsT>
concept HasPhWaitEvents = requires(ApiArgsT &apiArgs) {
                              apiArgs.phWaitEvents;
                          };

template <class ApiArgsT>
concept HasPhEvents = requires(ApiArgsT &apiArgs) {
                          apiArgs.phEvents;
                      };

template <class ApiArgsT>
concept HasHSignalEvent = requires(ApiArgsT &apiArgs) {
                              apiArgs.hSignalEvent;
                          };

template <CaptureApi api, typename... TArgs>
    requires HasPhWaitEvents<typename Closure<api>::ApiArgs>
inline std::span<ze_event_handle_t> getCommandsWaitEventsList(TArgs... args) {
    typename Closure<api>::ApiArgs structuredApiArgs{args...};
    return std::span<ze_event_handle_t>{structuredApiArgs.phWaitEvents, structuredApiArgs.numWaitEvents};
}

template <CaptureApi api, typename... TArgs>
    requires(HasPhEvents<typename Closure<api>::ApiArgs> && (false == HasPhWaitEvents<typename Closure<api>::ApiArgs>))
inline std::span<ze_event_handle_t> getCommandsWaitEventsList(TArgs... args) {
    typename Closure<api>::ApiArgs structuredApiArgs{args...};
    return std::span<ze_event_handle_t>{structuredApiArgs.phEvents, structuredApiArgs.numEvents};
}

template <CaptureApi api, typename... TArgs>
    requires(false == (HasPhEvents<typename Closure<api>::ApiArgs> || HasPhWaitEvents<typename Closure<api>::ApiArgs>))
inline std::span<ze_event_handle_t> getCommandsWaitEventsList(TArgs... args) {
    return std::span<ze_event_handle_t>{};
}

template <CaptureApi api, typename... TArgs>
    requires HasHSignalEvent<typename Closure<api>::ApiArgs>
inline ze_event_handle_t getCommandsSignalEvent(TArgs... args) {
    typename Closure<api>::ApiArgs structuredApiArgs{args...};
    return structuredApiArgs.hSignalEvent;
}

template <CaptureApi api, typename... TArgs>
    requires(api == CaptureApi::zeCommandListAppendSignalEvent)
inline ze_event_handle_t getCommandsSignalEvent(TArgs... args) {
    typename Closure<api>::ApiArgs structuredApiArgs{args...};
    return structuredApiArgs.hEvent;
}

template <CaptureApi api, typename... TArgs>
    requires((false == HasHSignalEvent<typename Closure<api>::ApiArgs>) && (api != CaptureApi::zeCommandListAppendSignalEvent))
inline ze_event_handle_t getCommandsSignalEvent(TArgs... args) {
    return nullptr;
}

struct IndirectArgsWithWaitEvents {
    IndirectArgsWithWaitEvents() = default;
    template <typename ApiArgsT>
        requires HasPhWaitEvents<ApiArgsT>
    IndirectArgsWithWaitEvents(const ApiArgsT &apiArgs, ClosureExternalStorage &externalStorage) {
        waitEvents = externalStorage.registerEventsWaitList(apiArgs.phWaitEvents, apiArgs.phWaitEvents + apiArgs.numWaitEvents);
    }

    template <typename ApiArgsT>
        requires(HasPhEvents<ApiArgsT> && (false == HasPhWaitEvents<ApiArgsT>))
    IndirectArgsWithWaitEvents(const ApiArgsT &apiArgs, ClosureExternalStorage &externalStorage) {
        waitEvents = externalStorage.registerEventsWaitList(apiArgs.phEvents, apiArgs.phEvents + apiArgs.numEvents);
    }

    ClosureExternalStorage::EventsWaitListId waitEvents = ClosureExternalStorage::invalidEventsWaitListId;
};

struct EmptyIndirectArgs {
    template <typename ApiArgsT>
    EmptyIndirectArgs(const ApiArgsT &apiArgs, ClosureExternalStorage &externalStorage) {}
};

template <>
struct Closure<CaptureApi::zeCommandListAppendMemoryCopy> {
    static constexpr bool isSupported = true;

    struct ApiArgs {
        ze_command_list_handle_t hCommandList;
        void *dstptr;
        const void *srcptr;
        size_t size;
        ze_event_handle_t hSignalEvent;
        uint32_t numWaitEvents;
        ze_event_handle_t *phWaitEvents;
    } apiArgs;

    using IndirectArgs = IndirectArgsWithWaitEvents;
    IndirectArgs indirectArgs;

    Closure(const ApiArgs &apiArgs, ClosureExternalStorage &externalStorage) : apiArgs(apiArgs), indirectArgs(apiArgs, externalStorage) {}

    ze_result_t instantiateTo(CommandList &executionTarget, ClosureExternalStorage &externalStorage) const;
};

template <>
struct Closure<CaptureApi::zeCommandListAppendBarrier> {
    static constexpr bool isSupported = true;

    struct ApiArgs {
        ze_command_list_handle_t hCommandList;
        ze_event_handle_t hSignalEvent;
        uint32_t numWaitEvents;
        ze_event_handle_t *phWaitEvents;
    } apiArgs;

    using IndirectArgs = IndirectArgsWithWaitEvents;
    IndirectArgs indirectArgs;

    Closure(const ApiArgs &apiArgs, ClosureExternalStorage &externalStorage) : apiArgs(apiArgs), indirectArgs(apiArgs, externalStorage) {}

    ze_result_t instantiateTo(CommandList &executionTarget, ClosureExternalStorage &externalStorage) const;
};

template <>
struct Closure<CaptureApi::zeCommandListAppendWaitOnEvents> {
    static constexpr bool isSupported = true;

    struct ApiArgs {
        ze_command_list_handle_t hCommandList;
        uint32_t numEvents;
        ze_event_handle_t *phEvents;
    } apiArgs;

    using IndirectArgs = IndirectArgsWithWaitEvents;
    IndirectArgs indirectArgs;

    Closure(const ApiArgs &apiArgs, ClosureExternalStorage &externalStorage) : apiArgs(apiArgs), indirectArgs(apiArgs, externalStorage) {}

    ze_result_t instantiateTo(CommandList &executionTarget, ClosureExternalStorage &externalStorage) const;
};

template <>
struct Closure<CaptureApi::zeCommandListAppendWriteGlobalTimestamp> {
    static constexpr bool isSupported = true;

    struct ApiArgs {
        ze_command_list_handle_t hCommandList;
        uint64_t *dstptr;
        ze_event_handle_t hSignalEvent;
        uint32_t numWaitEvents;
        ze_event_handle_t *phWaitEvents;
    } apiArgs;

    using IndirectArgs = IndirectArgsWithWaitEvents;
    IndirectArgs indirectArgs;

    Closure(const ApiArgs &apiArgs, ClosureExternalStorage &externalStorage) : apiArgs(apiArgs), indirectArgs(apiArgs, externalStorage) {}

    ze_result_t instantiateTo(CommandList &executionTarget, ClosureExternalStorage &externalStorage) const;
};

template <>
struct Closure<CaptureApi::zeCommandListAppendMemoryRangesBarrier> {
    static constexpr bool isSupported = true;

    struct ApiArgs {
        ze_command_list_handle_t hCommandList;
        uint32_t numRanges;
        const size_t *pRangeSizes;
        const void **pRanges;
        ze_event_handle_t hSignalEvent;
        uint32_t numWaitEvents;
        ze_event_handle_t *phWaitEvents;
    } apiArgs;

    struct IndirectArgs : IndirectArgsWithWaitEvents {
        IndirectArgs(const Closure::ApiArgs &apiArgs, ClosureExternalStorage &externalStorage) : IndirectArgsWithWaitEvents(apiArgs, externalStorage) {
            rangeSizes.resize(apiArgs.numRanges);
            ranges.resize(apiArgs.numRanges);
            std::copy_n(apiArgs.pRangeSizes, apiArgs.numRanges, rangeSizes.begin());
            std::copy_n(apiArgs.pRanges, apiArgs.numRanges, ranges.begin());
        }
        StackVec<size_t, 1> rangeSizes;
        StackVec<const void *, 1> ranges;
    } indirectArgs;

    Closure(const ApiArgs &apiArgs, ClosureExternalStorage &externalStorage) : apiArgs(apiArgs), indirectArgs(apiArgs, externalStorage) {}

    ze_result_t instantiateTo(CommandList &executionTarget, ClosureExternalStorage &externalStorage) const;
};

template <>
struct Closure<CaptureApi::zeCommandListAppendMemoryFill> {
    static constexpr bool isSupported = true;

    struct ApiArgs {
        ze_command_list_handle_t hCommandList;
        void *ptr;
        const void *pattern;
        size_t patternSize;
        size_t size;
        ze_event_handle_t hSignalEvent;
        uint32_t numWaitEvents;
        ze_event_handle_t *phWaitEvents;
    } apiArgs;

    struct IndirectArgs : IndirectArgsWithWaitEvents {
        IndirectArgs(const Closure::ApiArgs &apiArgs, ClosureExternalStorage &externalStorage) : IndirectArgsWithWaitEvents(apiArgs, externalStorage) {
            pattern.resize(apiArgs.patternSize);
            memcpy_s(pattern.data(), pattern.size(), apiArgs.pattern, apiArgs.patternSize);
        }
        StackVec<uint8_t, 16> pattern;
    } indirectArgs;

    Closure(const ApiArgs &apiArgs, ClosureExternalStorage &externalStorage) : apiArgs(apiArgs), indirectArgs(apiArgs, externalStorage) {}

    ze_result_t instantiateTo(CommandList &executionTarget, ClosureExternalStorage &externalStorage) const;
};

template <>
struct Closure<CaptureApi::zeCommandListAppendMemoryCopyRegion> {
    static constexpr bool isSupported = true;

    struct ApiArgs {
        ze_command_list_handle_t hCommandList;
        void *dstptr;
        const ze_copy_region_t *dstRegion;
        uint32_t dstPitch;
        uint32_t dstSlicePitch;
        const void *srcptr;
        const ze_copy_region_t *srcRegion;
        uint32_t srcPitch;
        uint32_t srcSlicePitch;
        ze_event_handle_t hSignalEvent;
        uint32_t numWaitEvents;
        ze_event_handle_t *phWaitEvents;
    } apiArgs;

    struct IndirectArgs : IndirectArgsWithWaitEvents {
        IndirectArgs(const Closure::ApiArgs &apiArgs, ClosureExternalStorage &externalStorage) : IndirectArgsWithWaitEvents(apiArgs, externalStorage) {
            dstRegion = *apiArgs.dstRegion;
            srcRegion = *apiArgs.srcRegion;
        }
        ze_copy_region_t dstRegion;
        ze_copy_region_t srcRegion;
    } indirectArgs;

    Closure(const ApiArgs &apiArgs, ClosureExternalStorage &externalStorage) : apiArgs(apiArgs), indirectArgs(apiArgs, externalStorage) {}

    ze_result_t instantiateTo(CommandList &executionTarget, ClosureExternalStorage &externalStorage) const;
};

template <>
struct Closure<CaptureApi::zeCommandListAppendMemoryCopyFromContext> {
    static constexpr bool isSupported = true;

    struct ApiArgs {
        ze_command_list_handle_t hCommandList;
        void *dstptr;
        ze_context_handle_t hContextSrc;
        const void *srcptr;
        size_t size;
        ze_event_handle_t hSignalEvent;
        uint32_t numWaitEvents;
        ze_event_handle_t *phWaitEvents;
    } apiArgs;

    using IndirectArgs = IndirectArgsWithWaitEvents;
    IndirectArgs indirectArgs;

    Closure(const ApiArgs &apiArgs, ClosureExternalStorage &externalStorage) : apiArgs(apiArgs), indirectArgs(apiArgs, externalStorage) {}

    ze_result_t instantiateTo(CommandList &executionTarget, ClosureExternalStorage &externalStorage) const;
};

template <>
struct Closure<CaptureApi::zeCommandListAppendImageCopy> {
    static constexpr bool isSupported = true;

    struct ApiArgs {
        ze_command_list_handle_t hCommandList;
        ze_image_handle_t hDstImage;
        ze_image_handle_t hSrcImage;
        ze_event_handle_t hSignalEvent;
        uint32_t numWaitEvents;
        ze_event_handle_t *phWaitEvents;
    } apiArgs;

    using IndirectArgs = IndirectArgsWithWaitEvents;
    IndirectArgs indirectArgs;

    Closure(const ApiArgs &apiArgs, ClosureExternalStorage &externalStorage) : apiArgs(apiArgs), indirectArgs(apiArgs, externalStorage) {}

    ze_result_t instantiateTo(CommandList &executionTarget, ClosureExternalStorage &externalStorage) const;
};

template <>
struct Closure<CaptureApi::zeCommandListAppendImageCopyRegion> {
    static constexpr bool isSupported = true;

    struct ApiArgs {
        ze_command_list_handle_t hCommandList;
        ze_image_handle_t hDstImage;
        ze_image_handle_t hSrcImage;
        const ze_image_region_t *pDstRegion;
        const ze_image_region_t *pSrcRegion;
        ze_event_handle_t hSignalEvent;
        uint32_t numWaitEvents;
        ze_event_handle_t *phWaitEvents;
    } apiArgs;

    struct IndirectArgs : IndirectArgsWithWaitEvents {
        IndirectArgs(const Closure::ApiArgs &apiArgs, ClosureExternalStorage &externalStorage) : IndirectArgsWithWaitEvents(apiArgs, externalStorage) {
            dstRegion = *apiArgs.pDstRegion;
            srcRegion = *apiArgs.pSrcRegion;
        }
        ze_image_region_t dstRegion;
        ze_image_region_t srcRegion;
    } indirectArgs;

    Closure(const ApiArgs &apiArgs, ClosureExternalStorage &externalStorage) : apiArgs(apiArgs), indirectArgs(apiArgs, externalStorage) {}

    ze_result_t instantiateTo(CommandList &executionTarget, ClosureExternalStorage &externalStorage) const;
};

template <>
struct Closure<CaptureApi::zeCommandListAppendImageCopyToMemory> {
    static constexpr bool isSupported = true;

    struct ApiArgs {
        ze_command_list_handle_t hCommandList;
        void *dstptr;
        ze_image_handle_t hSrcImage;
        const ze_image_region_t *pSrcRegion;
        ze_event_handle_t hSignalEvent;
        uint32_t numWaitEvents;
        ze_event_handle_t *phWaitEvents;
    } apiArgs;

    struct IndirectArgs : IndirectArgsWithWaitEvents {
        IndirectArgs(const Closure::ApiArgs &apiArgs, ClosureExternalStorage &externalStorage) : IndirectArgsWithWaitEvents(apiArgs, externalStorage) {
            srcRegion = *apiArgs.pSrcRegion;
        }
        ze_image_region_t srcRegion;
    } indirectArgs;

    Closure(const ApiArgs &apiArgs, ClosureExternalStorage &externalStorage) : apiArgs(apiArgs), indirectArgs(apiArgs, externalStorage) {}

    ze_result_t instantiateTo(CommandList &executionTarget, ClosureExternalStorage &externalStorage) const;
};

template <>
struct Closure<CaptureApi::zeCommandListAppendImageCopyFromMemory> {
    static constexpr bool isSupported = true;

    struct ApiArgs {
        ze_command_list_handle_t hCommandList;
        ze_image_handle_t hDstImage;
        const void *srcptr;
        const ze_image_region_t *pDstRegion;
        ze_event_handle_t hSignalEvent;
        uint32_t numWaitEvents;
        ze_event_handle_t *phWaitEvents;
    } apiArgs;

    struct IndirectArgs : IndirectArgsWithWaitEvents {
        IndirectArgs(const Closure::ApiArgs &apiArgs, ClosureExternalStorage &externalStorage) : IndirectArgsWithWaitEvents(apiArgs, externalStorage) {
            dstRegion = *apiArgs.pDstRegion;
        }
        ze_image_region_t dstRegion;
    } indirectArgs;

    Closure(const ApiArgs &apiArgs, ClosureExternalStorage &externalStorage) : apiArgs(apiArgs), indirectArgs(apiArgs, externalStorage) {}

    ze_result_t instantiateTo(CommandList &executionTarget, ClosureExternalStorage &externalStorage) const;
};

template <>
struct Closure<CaptureApi::zeCommandListAppendMemoryPrefetch> {
    static constexpr bool isSupported = true;

    struct ApiArgs {
        ze_command_list_handle_t hCommandList;
        const void *ptr;
        size_t size;
    } apiArgs;

    using IndirectArgs = EmptyIndirectArgs;
    IndirectArgs indirectArgs;

    Closure(const ApiArgs &apiArgs, ClosureExternalStorage &externalStorage) : apiArgs(apiArgs), indirectArgs(apiArgs, externalStorage) {}

    ze_result_t instantiateTo(CommandList &executionTarget, ClosureExternalStorage &externalStorage) const;
};

template <>
struct Closure<CaptureApi::zeCommandListAppendMemAdvise> {
    static constexpr bool isSupported = true;

    struct ApiArgs {
        ze_command_list_handle_t hCommandList;
        ze_device_handle_t hDevice;
        const void *ptr;
        size_t size;
        ze_memory_advice_t advice;
    } apiArgs;

    using IndirectArgs = EmptyIndirectArgs;
    IndirectArgs indirectArgs;

    Closure(const ApiArgs &apiArgs, ClosureExternalStorage &externalStorage) : apiArgs(apiArgs), indirectArgs(apiArgs, externalStorage) {}

    ze_result_t instantiateTo(CommandList &executionTarget, ClosureExternalStorage &externalStorage) const;
};

template <>
struct Closure<CaptureApi::zeCommandListAppendSignalEvent> {
    static constexpr bool isSupported = true;

    struct ApiArgs {
        ze_command_list_handle_t hCommandList;
        ze_event_handle_t hEvent;
    } apiArgs;

    using IndirectArgs = EmptyIndirectArgs;
    IndirectArgs indirectArgs;

    Closure(const ApiArgs &apiArgs, ClosureExternalStorage &externalStorage) : apiArgs(apiArgs), indirectArgs(apiArgs, externalStorage) {}

    ze_result_t instantiateTo(CommandList &executionTarget, ClosureExternalStorage &externalStorage) const;
};

template <>
struct Closure<CaptureApi::zeCommandListAppendEventReset> {
    static constexpr bool isSupported = true;

    struct ApiArgs {
        ze_command_list_handle_t hCommandList;
        ze_event_handle_t hEvent;
    } apiArgs;

    using IndirectArgs = EmptyIndirectArgs;
    IndirectArgs indirectArgs;

    Closure(const ApiArgs &apiArgs, ClosureExternalStorage &externalStorage) : apiArgs(apiArgs), indirectArgs(apiArgs, externalStorage) {}

    ze_result_t instantiateTo(CommandList &executionTarget, ClosureExternalStorage &externalStorage) const;
};

template <>
struct Closure<CaptureApi::zeCommandListAppendQueryKernelTimestamps> {
    static constexpr bool isSupported = true;

    struct ApiArgs {
        ze_command_list_handle_t hCommandList;
        uint32_t numEvents;
        ze_event_handle_t *phEvents;
        void *dstptr;
        const size_t *pOffsets;
        ze_event_handle_t hSignalEvent;
        uint32_t numWaitEvents;
        ze_event_handle_t *phWaitEvents;
    } apiArgs;

    struct IndirectArgs : IndirectArgsWithWaitEvents {
        IndirectArgs(const Closure::ApiArgs &apiArgs, ClosureExternalStorage &externalStorage) : IndirectArgsWithWaitEvents(apiArgs, externalStorage) {
            events.resize(apiArgs.numEvents);
            offsets.resize(apiArgs.numEvents);
            std::copy_n(apiArgs.phEvents, apiArgs.numEvents, events.begin());
            if (apiArgs.pOffsets) {
                std::copy_n(apiArgs.pOffsets, apiArgs.numEvents, offsets.begin());
            }
        }

        StackVec<ze_event_handle_t, 1> events;
        StackVec<size_t, 1> offsets;
    } indirectArgs;

    Closure(const ApiArgs &apiArgs, ClosureExternalStorage &externalStorage) : apiArgs(apiArgs), indirectArgs(apiArgs, externalStorage) {}

    ze_result_t instantiateTo(CommandList &executionTarget, ClosureExternalStorage &externalStorage) const;
};

template <>
struct Closure<CaptureApi::zeCommandListAppendSignalExternalSemaphoreExt> {
    static constexpr bool isSupported = true;

    struct ApiArgs {
        ze_command_list_handle_t hCommandList;
        uint32_t numSemaphores;
        ze_external_semaphore_ext_handle_t *phSemaphores;
        ze_external_semaphore_signal_params_ext_t *signalParams;
        ze_event_handle_t hSignalEvent;
        uint32_t numWaitEvents;
        ze_event_handle_t *phWaitEvents;
    } apiArgs;

    struct IndirectArgs : IndirectArgsWithWaitEvents {
        IndirectArgs(const Closure::ApiArgs &apiArgs, ClosureExternalStorage &externalStorage) : IndirectArgsWithWaitEvents(apiArgs, externalStorage) {
            semaphores.resize(apiArgs.numSemaphores);
            std::copy_n(apiArgs.phSemaphores, apiArgs.numSemaphores, semaphores.begin());
            signalParams = *apiArgs.signalParams;
        }
        StackVec<ze_external_semaphore_ext_handle_t, 1> semaphores;
        ze_external_semaphore_signal_params_ext_t signalParams;
    } indirectArgs;

    Closure(const ApiArgs &apiArgs, ClosureExternalStorage &externalStorage) : apiArgs(apiArgs), indirectArgs(apiArgs, externalStorage) {}

    ze_result_t instantiateTo(CommandList &executionTarget, ClosureExternalStorage &externalStorage) const;
};

template <>
struct Closure<CaptureApi::zeCommandListAppendWaitExternalSemaphoreExt> {
    static constexpr bool isSupported = true;

    struct ApiArgs {
        ze_command_list_handle_t hCommandList;
        uint32_t numSemaphores;
        ze_external_semaphore_ext_handle_t *phSemaphores;
        ze_external_semaphore_wait_params_ext_t *waitParams;
        ze_event_handle_t hSignalEvent;
        uint32_t numWaitEvents;
        ze_event_handle_t *phWaitEvents;
    } apiArgs;

    struct IndirectArgs : IndirectArgsWithWaitEvents {
        IndirectArgs(const Closure::ApiArgs &apiArgs, ClosureExternalStorage &externalStorage) : IndirectArgsWithWaitEvents(apiArgs, externalStorage) {
            semaphores.resize(apiArgs.numSemaphores);
            std::copy_n(apiArgs.phSemaphores, apiArgs.numSemaphores, semaphores.begin());
            waitParams = *apiArgs.waitParams;
        }
        StackVec<ze_external_semaphore_ext_handle_t, 1> semaphores;
        ze_external_semaphore_wait_params_ext_t waitParams;
    } indirectArgs;

    Closure(const ApiArgs &apiArgs, ClosureExternalStorage &externalStorage) : apiArgs(apiArgs), indirectArgs(apiArgs, externalStorage) {}

    ze_result_t instantiateTo(CommandList &executionTarget, ClosureExternalStorage &externalStorage) const;
};

template <>
struct Closure<CaptureApi::zeCommandListAppendImageCopyToMemoryExt> {
    static constexpr bool isSupported = true;

    struct ApiArgs {
        ze_command_list_handle_t hCommandList;
        void *dstptr;
        ze_image_handle_t hSrcImage;
        const ze_image_region_t *pSrcRegion;
        uint32_t destRowPitch;
        uint32_t destSlicePitch;
        ze_event_handle_t hSignalEvent;
        uint32_t numWaitEvents;
        ze_event_handle_t *phWaitEvents;
    } apiArgs;

    struct IndirectArgs : IndirectArgsWithWaitEvents {
        IndirectArgs(const Closure::ApiArgs &apiArgs, ClosureExternalStorage &externalStorage) : IndirectArgsWithWaitEvents(apiArgs, externalStorage) {
            srcRegion = *apiArgs.pSrcRegion;
        }
        ze_image_region_t srcRegion;
    } indirectArgs;

    Closure(const ApiArgs &apiArgs, ClosureExternalStorage &externalStorage) : apiArgs(apiArgs), indirectArgs(apiArgs, externalStorage) {}

    ze_result_t instantiateTo(CommandList &executionTarget, ClosureExternalStorage &externalStorage) const;
};

template <>
struct Closure<CaptureApi::zeCommandListAppendImageCopyFromMemoryExt> {
    static constexpr bool isSupported = true;

    struct ApiArgs {
        ze_command_list_handle_t hCommandList;
        ze_image_handle_t hDstImage;
        const void *srcptr;
        const ze_image_region_t *pDstRegion;
        uint32_t srcRowPitch;
        uint32_t srcSlicePitch;
        ze_event_handle_t hSignalEvent;
        uint32_t numWaitEvents;
        ze_event_handle_t *phWaitEvents;
    } apiArgs;

    struct IndirectArgs : IndirectArgsWithWaitEvents {
        IndirectArgs(const Closure::ApiArgs &apiArgs, ClosureExternalStorage &externalStorage) : IndirectArgsWithWaitEvents(apiArgs, externalStorage) {
            dstRegion = *apiArgs.pDstRegion;
        }
        ze_image_region_t dstRegion;
    } indirectArgs;

    Closure(const ApiArgs &apiArgs, ClosureExternalStorage &externalStorage) : apiArgs(apiArgs), indirectArgs(apiArgs, externalStorage) {}

    ze_result_t instantiateTo(CommandList &executionTarget, ClosureExternalStorage &externalStorage) const;
};

template <>
struct Closure<CaptureApi::zeCommandListAppendLaunchKernel> {
    inline static constexpr bool isSupported = true;

    struct ApiArgs {
        ze_command_list_handle_t hCommandList;
        ze_kernel_handle_t kernelHandle;
        const ze_group_count_t *launchKernelArgs;
        ze_event_handle_t hSignalEvent;
        uint32_t numWaitEvents;
        ze_event_handle_t *phWaitEvents;
    } apiArgs;

    struct IndirectArgs : IndirectArgsWithWaitEvents {
        IndirectArgs(const Closure::ApiArgs &apiArgs, ClosureExternalStorage &externalStorage);
        ze_group_count_t launchKernelArgs;
        ClosureExternalStorage::KernelStateId kernelState = ClosureExternalStorage::invalidKernelStateId;
    } indirectArgs;

    Closure(const ApiArgs &apiArgs, ClosureExternalStorage &externalStorage) : apiArgs(apiArgs), indirectArgs(apiArgs, externalStorage) {}

    ze_result_t instantiateTo(CommandList &executionTarget, ClosureExternalStorage &externalStorage) const;
};

} // namespace L0
