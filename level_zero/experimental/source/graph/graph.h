/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/utilities/stackvec.h"

#include "level_zero/ze_api.h"

#include <memory>
#include <variant>
#include <vector>

typedef struct _ze_graph_handle_t *ze_graph_handle_t;
typedef struct _ze_executable_graph_handle_t *ze_executable_graph_handle_t;

struct _ze_graph_handle_t {
};

struct _ze_executable_graph_handle_t {
};

namespace L0 {

struct Context;

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
    };

    Closure(const ApiArgs &apiArgs) {}

    ze_result_t instantiateTo(CommandList &executionTarget) const {
        DEBUG_BREAK_IF(true);
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }
};

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

using ClosureVariants = std::variant<
#define RR_CAPTURED_API(X) Closure<CaptureApi::X>,
    RR_CAPTURED_APIS()
#undef RR_CAPTURED_API
        int>;
using CapturedCommand = ClosureVariants;

struct Graph : _ze_graph_handle_t {
    Graph(L0::Context *ctx, bool preallocated) : ctx(ctx), preallocated(preallocated) {
        commands.reserve(16);
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

    L0::CommandList *getExecutionTarget() const {
        return executionTarget;
    }

    bool isSubGraph() const {
        return (nullptr != executionTarget);
    }

    void addSubGraph(Graph *subGraph) {
        subGraphs.push_back(subGraph);
    }

  protected:
    std::vector<CapturedCommand> commands;
    StackVec<Graph *, 16> subGraphs;

    L0::CommandList *captureSrc = nullptr;
    CaptureTargetDesc captureTargetDesc;
    L0::CommandList *executionTarget = nullptr;

    L0::Context *ctx = nullptr;
    bool preallocated = false;
};

struct ExecutableGraph : _ze_executable_graph_handle_t {
    ExecutableGraph() {
    }

    void instantiateFrom(Graph &graph);

    ~ExecutableGraph();

    static ExecutableGraph *fromHandle(ze_executable_graph_handle_t handle) {
        return static_cast<ExecutableGraph *>(handle);
    }

    bool empty() {
        return nullptr == hwCommands;
    }

    bool isSubGraph() const {
        return (nullptr != executionTarget);
    }

    const StackVec<std::unique_ptr<ExecutableGraph>, 16> &getSubgraphs() {
        return subGraphs;
    }

    ze_result_t execute(L0::CommandList *executionTarget, void *pNext, ze_event_handle_t hSignalEvent, uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents);

  protected:
    Graph *src = nullptr;
    L0::CommandList *executionTarget = nullptr;
    std::unique_ptr<L0::CommandList> hwCommands;

    StackVec<std::unique_ptr<ExecutableGraph>, 16> subGraphs;
};

} // namespace L0