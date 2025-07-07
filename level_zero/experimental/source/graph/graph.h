/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/utilities/range.h"
#include "shared/source/utilities/stackvec.h"

#include "level_zero/ze_api.h"

#include <atomic>
#include <memory>
#include <unordered_map>
#include <variant>
#include <vector>

typedef struct _ze_graph_handle_t *ze_graph_handle_t;
typedef struct _ze_executable_graph_handle_t *ze_executable_graph_handle_t;

struct _ze_graph_handle_t {
};

struct _ze_executable_graph_handle_t {
};

namespace L0 {

inline std::atomic<bool> processUsesGraphs{false};
inline void enabledGraphs() {
    bool graphsEnabled = false;
    processUsesGraphs.compare_exchange_weak(graphsEnabled, true, std::memory_order_relaxed);
}

inline bool areGraphsEnabled() {
    return processUsesGraphs.load();
}

struct Context;
struct Event;

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
    RR_CAPTURED_API(zeCommandListAppendImageCopyFromMemoryExt)

enum class CaptureApi {
#define RR_CAPTURED_API(X) X,
    RR_CAPTURED_APIS()
#undef RR_CAPTURED_API
};

struct CommandList;

template <CaptureApi api>
struct Closure {
    inline static constexpr bool isSupported = false;

    struct ApiArgs {
        template <typename ArgsT>
        ApiArgs(ArgsT...) {}

        ze_event_handle_t hSignalEvent = nullptr;
        uint32_t numWaitEvents = 0;
        ze_event_handle_t *phWaitEvents = nullptr;
    };

    Closure(const ApiArgs &apiArgs) {}

    ze_result_t instantiateTo(CommandList &executionTarget) const {
        DEBUG_BREAK_IF(true);
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }
};

template <CaptureApi api, typename... TArgs>
inline NEO::Range<ze_event_handle_t> getCommandsWaitEventsList(TArgs... args) {
    typename Closure<api>::ApiArgs structuredApiArgs{args...};
    return NEO::Range<ze_event_handle_t>{structuredApiArgs.phWaitEvents, structuredApiArgs.numWaitEvents};
}

template <CaptureApi api, typename... TArgs>
inline ze_event_handle_t getCommandsSignalEvent(TArgs... args) {
    typename Closure<api>::ApiArgs structuredApiArgs{args...};
    return structuredApiArgs.hSignalEvent;
}

template <>
struct Closure<CaptureApi::zeCommandListAppendMemoryCopy> {
    inline static constexpr bool isSupported = true;

    struct ApiArgs {
        ze_command_list_handle_t hCommandList;
        void *dstptr;
        const void *srcptr;
        size_t size;
        ze_event_handle_t hSignalEvent;
        uint32_t numWaitEvents;
        ze_event_handle_t *phWaitEvents;
    } apiArgs;

    struct IndirectArgs {
        StackVec<ze_event_handle_t, 8> waitEvents;
    } indirectArgs;

    Closure(const ApiArgs &apiArgs);

    ze_result_t instantiateTo(CommandList &executionTarget) const;
};

template <>
struct Closure<CaptureApi::zeCommandListAppendBarrier> {
    inline static constexpr bool isSupported = true;

    struct ApiArgs {
        ze_command_list_handle_t hCommandList;
        ze_event_handle_t hSignalEvent;
        uint32_t numWaitEvents;
        ze_event_handle_t *phWaitEvents;
    } apiArgs;

    struct IndirectArgs {
        StackVec<ze_event_handle_t, 8> waitEvents;
    } indirectArgs;

    Closure(const ApiArgs &apiArgs);

    ze_result_t instantiateTo(CommandList &executionTarget) const;
};

template <>
struct Closure<CaptureApi::zeCommandListAppendWaitOnEvents> {
    inline static constexpr bool isSupported = true;

    struct ApiArgs {
        ze_command_list_handle_t hCommandList;
        uint32_t numEvents;
        ze_event_handle_t *phEvents;
    } apiArgs;

    struct IndirectArgs {
        StackVec<ze_event_handle_t, 8> waitEvents;
    } indirectArgs;

    Closure(const ApiArgs &apiArgs);

    ze_result_t instantiateTo(CommandList &executionTarget) const;
};

template <>
inline NEO::Range<ze_event_handle_t> getCommandsWaitEventsList<CaptureApi::zeCommandListAppendWaitOnEvents>(ze_command_list_handle_t, uint32_t numEvents, ze_event_handle_t *phEvents) {
    return NEO::Range<ze_event_handle_t>{phEvents, numEvents};
}

template <>
inline ze_event_handle_t getCommandsSignalEvent<CaptureApi::zeCommandListAppendWaitOnEvents>(ze_command_list_handle_t, uint32_t numEvents, ze_event_handle_t *phEvents) {
    return nullptr;
}

using ClosureVariants = std::variant<
#define RR_CAPTURED_API(X) Closure<CaptureApi::X>,
    RR_CAPTURED_APIS()
#undef RR_CAPTURED_API
        int>;
using CapturedCommand = ClosureVariants;

using CapturedCommandId = uint32_t;
struct Graph : _ze_graph_handle_t {
    Graph(L0::Context *ctx, bool preallocated) : ctx(ctx), preallocated(preallocated) {
        commands.reserve(16);
        enabledGraphs();
    }

    ~Graph();

    Graph(const Graph &) = delete;
    Graph &operator=(const Graph &) = delete;

    static Graph *fromHandle(ze_graph_handle_t handle) {
        return static_cast<Graph *>(handle);
    }

    bool wasPreallocated() const {
        return preallocated;
    }

    void startCapturingFrom(L0::CommandList &captureSrc, bool isSubGraph);
    void stopCapturing();

    template <CaptureApi api, typename... TArgs>
    ze_result_t capture(TArgs... apiArgs) {
        if (false == Closure<api>::isSupported) {
            return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
        }

        using ApirArgsT = typename Closure<api>::ApiArgs;
        auto capturedArgs = ApirArgsT{apiArgs...};
        commands.push_back(CapturedCommand{Closure<api>(capturedArgs)});
        return ZE_RESULT_SUCCESS;
    }

    const std::vector<CapturedCommand> &getCapturedCommands() {
        return commands;
    }

    Graph *getJoinedForkTarget(CapturedCommandId cmdId) {
        auto it = joinedForks.find(cmdId);
        if (joinedForks.end() == it) {
            return nullptr;
        }
        return it->second.forkDestiny;
    }

    const StackVec<Graph *, 16> &getSubgraphs() {
        return subGraphs;
    }

    L0::Context *getContext() const {
        return ctx;
    }

    struct CaptureTargetDesc {
        ze_device_handle_t hDevice = nullptr;
        ze_command_list_desc_t desc;
    };

    const CaptureTargetDesc &getCaptureTargetDesc() const {
        return captureTargetDesc;
    }

    bool empty() const {
        return commands.empty();
    }

    bool validForInstantiation() const {
        return closed() && unjoinedForks.empty() && std::all_of(subGraphs.begin(), subGraphs.end(), [](auto it) { return it->validForInstantiation(); });
    }

    bool closed() const {
        return wasCapturingStopped;
    }

    L0::CommandList *getExecutionTarget() const {
        return executionTarget;
    }

    bool isSubGraph() const {
        return (nullptr != executionTarget);
    }

    bool hasUnjoinedForks() const {
        return false == unjoinedForks.empty();
    }

    void tryJoinOnNextCommand(L0::CommandList &childCmdList, L0::Event &joinEvent);
    void forkTo(L0::CommandList &childCmdList, Graph *&child, L0::Event &forkEvent);
    void registerSignallingEventFromPreviousCommand(L0::Event &ev);

  protected:
    void unregisterSignallingEvents();

    std::vector<CapturedCommand> commands;
    StackVec<Graph *, 16> subGraphs;

    L0::CommandList *captureSrc = nullptr;
    CaptureTargetDesc captureTargetDesc;
    L0::CommandList *executionTarget = nullptr;

    L0::Context *ctx = nullptr;
    bool preallocated = false;
    bool wasCapturingStopped = false;

    std::unordered_map<L0::Event *, CapturedCommandId> recordedSignals;

    struct ForkInfo {
        CapturedCommandId forkCommandId = 0;
        ze_event_handle_t forkEvent = nullptr;
    };

    std::unordered_map<L0::CommandList *, ForkInfo> unjoinedForks;

    struct ForkJoinInfo {
        CapturedCommandId forkCommandId = 0;
        CapturedCommandId joinCommandId = 0;
        ze_event_handle_t forkEvent = nullptr;
        ze_event_handle_t joinEvent = nullptr;
        Graph *forkDestiny = nullptr;
    };
    std::unordered_map<CapturedCommandId, ForkJoinInfo> joinedForks;
};

void recordHandleWaitEventsFromNextCommand(L0::CommandList &srcCmdList, Graph *&captureTarget, NEO::Range<ze_event_handle_t> events);
void recordHandleSignalEventFromPreviousCommand(L0::CommandList &srcCmdList, Graph &captureTarget, ze_event_handle_t event);

template <CaptureApi api, typename... TArgs>
ze_result_t captureCommand(L0::CommandList &srcCmdList, Graph *&captureTarget, TArgs... apiArgs) {
    if (false == areGraphsEnabled()) {
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    auto eventsWaitList = getCommandsWaitEventsList<api>(apiArgs...);
    if ((false == eventsWaitList.empty()) && ((nullptr == captureTarget) || (captureTarget->hasUnjoinedForks()))) { // either is not capturing and is potential fork or this can be a join operation
        recordHandleWaitEventsFromNextCommand(srcCmdList, captureTarget, eventsWaitList);
    }

    if (nullptr == captureTarget) {
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    auto ret = captureTarget->capture<api>(apiArgs...);
    if (ZE_RESULT_SUCCESS != ret) {
        return ret;
    }

    if (getCommandsSignalEvent<api>(apiArgs...)) {
        recordHandleSignalEventFromPreviousCommand(srcCmdList, *captureTarget, getCommandsSignalEvent<api>(apiArgs...));
    }
    return ZE_RESULT_SUCCESS;
}

struct ExecutableGraph;
using GraphSubmissionSegment = std::variant<L0::CommandList *, ExecutableGraph *>;
using GraphSubmissionChain = std::vector<GraphSubmissionSegment>;

struct GraphInstatiateSettings {
    GraphInstatiateSettings() = default;
    GraphInstatiateSettings(void *pNext) {
        UNRECOVERABLE_IF(nullptr != pNext);
    }

    enum ForkPolicy {
        ForkPolicyMonolythicLevels, // build and submit monolythic commandlists for each level
        ForkPolicySplitLevels       // split commandlists on forks and interleave submission with child graphs (prevents deadlocks when submitting to single HW queue)
    };

    ForkPolicy forkPolicy = ForkPolicySplitLevels;
};

struct ExecutableGraph : _ze_executable_graph_handle_t {
    ExecutableGraph() {
    }

    void instantiateFrom(Graph &graph, const GraphInstatiateSettings &settings);
    void instantiateFrom(Graph &graph) {
        instantiateFrom(graph, {});
    }

    ~ExecutableGraph();

    static ExecutableGraph *fromHandle(ze_executable_graph_handle_t handle) {
        return static_cast<ExecutableGraph *>(handle);
    }

    bool empty() {
        return myCommandLists.empty();
    }

    bool isSubGraph() const {
        return (nullptr != executionTarget);
    }

    const StackVec<std::unique_ptr<ExecutableGraph>, 16> &getSubgraphs() {
        return subGraphs;
    }

    ze_result_t execute(L0::CommandList *executionTarget, void *pNext, ze_event_handle_t hSignalEvent, uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents);

  protected:
    L0::CommandList *allocateAndAddCommandListSubmissionNode();
    void addSubGraphSubmissionNode(ExecutableGraph *subGraph);

    Graph *src = nullptr;
    L0::CommandList *executionTarget = nullptr;
    std::vector<std::unique_ptr<L0::CommandList>> myCommandLists;

    StackVec<std::unique_ptr<ExecutableGraph>, 16> subGraphs;

    GraphSubmissionChain submissionChain;
};

} // namespace L0