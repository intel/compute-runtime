/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/utilities/mem_lifetime.h"
#include "shared/source/utilities/stackvec.h"

#include "level_zero/experimental/source/graph/graph_captured_apis.h"
#include "level_zero/ze_api.h"

#include <atomic>
#include <memory>
#include <mutex>
#include <span>
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

using ClosureVariants = std::variant<
#define RR_CAPTURED_API(X) Closure<CaptureApi::X>,
    RR_CAPTURED_APIS()
#undef RR_CAPTURED_API
        int>;
using CapturedCommand = ClosureVariants;

using CapturedCommandId = uint32_t;
using SubgraphCommandId = CapturedCommandId;
using GraphCommandId = CapturedCommandId;

struct Graph;

struct ForkInfo {
    CapturedCommandId forkCommandId = 0;
    ze_event_handle_t forkEvent = nullptr;
};

struct ForkJoinInfo {
    CapturedCommandId forkCommandId = 0;
    CapturedCommandId joinCommandId = 0;
    ze_event_handle_t forkEvent = nullptr;
    ze_event_handle_t joinEvent = nullptr;
    Graph *forkDestiny = nullptr;
};

// Contigous commands in the order of recording (i.e. in the order of host API invocations)
struct OrderedCommandsSegment {
    Graph *subgraph = nullptr;      // subgraph that recorded this segmnet
    GraphCommandId begin = 0;       // id of the segment's first command within the whole graph
    SubgraphCommandId subBegin = 0; // id of the segment within the subgraph
    uint32_t numCommands = 0;       // number of commands in this segment

    bool empty() const {
        return 0 == numCommands;
    }
};

// Thread-safe registry of all ordered segments of a graph
struct OrderedCommandsRegistry final {
    GraphCommandId acquireNextGraphCommandId() {
        return commandsCounter.fetch_add(1);
    }

    void registerSegment(const OrderedCommandsSegment &segment) {
        std::lock_guard lock{rangeLock};
        UNRECOVERABLE_IF(0 == segment.numCommands);
        UNRECOVERABLE_IF(segment.begin + segment.numCommands > commandsCounter.load());
        auto pos = std::lower_bound(registeredSegments.begin(), registeredSegments.end(), segment,
                                    [](const auto &a, const auto &b) { return a.begin < b.begin; });
        registeredSegments.insert(pos, segment);
    }

    void close() {
        this->closed = true;
    }

    auto begin() const {
        UNRECOVERABLE_IF(false == this->closed);
        return registeredSegments.begin();
    }

    auto end() const {
        UNRECOVERABLE_IF(false == this->closed);
        return registeredSegments.end();
    }

    bool empty() const {
        return begin() == end();
    }

  protected:
    std::atomic<GraphCommandId> commandsCounter;

    std::mutex rangeLock;
    std::vector<OrderedCommandsSegment> registeredSegments;
    bool closed = false;
};

struct Graph : _ze_graph_handle_t {
    Graph(L0::Context *ctx, bool preallocated, WeaklyShared<OrderedCommandsRegistry> &&orderedCommandsRegistry)
        : ctx(ctx), preallocated(preallocated),
          orderedCommands(std::move(orderedCommandsRegistry)) {
        if (orderedCommands.empty()) {
            orderedCommands = WeaklyShared<OrderedCommandsRegistry>(new OrderedCommandsRegistry);
        }
        commands.reserve(16);
        enabledGraphs();
    }

    Graph(L0::Context *ctx, bool preallocated) : Graph(ctx, preallocated, {}) {
    }

    ~Graph();

    Graph(const Graph &) = delete;
    Graph &operator=(const Graph &) = delete;

    static Graph *fromHandle(ze_graph_handle_t handle) {
        return static_cast<Graph *>(handle);
    }
    inline ze_graph_handle_t toHandle() { return this; }

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

        using ApiArgsT = typename Closure<api>::ApiArgs;
        auto capturedArgs = ApiArgsT{apiArgs...};
        auto graphCommandId = orderedCommands->acquireNextGraphCommandId();
        if (segments.empty() || (graphCommandId != segments.rbegin()->begin + segments.rbegin()->numCommands)) { // new segment
            segments.push_back(OrderedCommandsSegment{.subgraph = this, .begin = graphCommandId, .subBegin = static_cast<uint32_t>(commands.size()), .numCommands = 1});
        } else {
            ++segments.rbegin()->numCommands;
        }
        commands.push_back(CapturedCommand{Closure<api>(capturedArgs, externalStorage)});
        return ZE_RESULT_SUCCESS;
    }

    const std::vector<CapturedCommand> &getCapturedCommands() const {
        return commands;
    }

    const StackVec<Graph *, 16> &getSubgraphs() const {
        return subGraphs;
    }

    const std::unordered_map<CapturedCommandId, ForkJoinInfo> &getJoinedForks() const {
        return potentialJoins;
    }

    const std::unordered_map<L0::CommandList *, ForkInfo> &getUnjoinedForks() const {
        return unjoinedForks;
    }

    Graph *getJoinedForkTarget(CapturedCommandId cmdId) {
        auto it = potentialJoins.find(cmdId);
        if (potentialJoins.end() == it) {
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
        ze_command_list_desc_t desc{};
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

    bool valid() const {
        return unjoinedForks.empty() && std::all_of(subGraphs.begin(), subGraphs.end(), [](auto it) { return it->valid(); });
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
    ClosureExternalStorage &getExternalStorage() {
        return externalStorage;
    }

    const ClosureExternalStorage &getExternalStorage() const {
        return externalStorage;
    }

    bool isLastCommand(CapturedCommandId commandId) const {
        return commandId + 1 == commands.size();
    }

    const OrderedCommandsRegistry &getOrderedCommands() const {
        return *orderedCommands;
    }

  protected:
    void unregisterSignallingEvents();

    ClosureExternalStorage externalStorage;
    CaptureTargetDesc captureTargetDesc;

    std::vector<CapturedCommand> commands;
    std::vector<OrderedCommandsSegment> segments;
    StackVec<Graph *, 16> subGraphs;

    std::unordered_map<L0::Event *, CapturedCommandId> recordedSignals;
    std::unordered_map<L0::CommandList *, ForkInfo> unjoinedForks;
    std::unordered_map<CapturedCommandId, ForkJoinInfo> potentialJoins;

    L0::CommandList *captureSrc = nullptr;
    L0::CommandList *executionTarget = nullptr;
    L0::Context *ctx = nullptr;

    bool preallocated = false;
    bool wasCapturingStopped = false;

    WeaklyShared<OrderedCommandsRegistry> orderedCommands; // shared between graph and subgraphs
};

void recordHandleWaitEventsFromNextCommand(L0::CommandList &srcCmdList, Graph *&captureTarget, std::span<ze_event_handle_t> events);
void recordHandleSignalEventFromPreviousCommand(L0::CommandList &srcCmdList, Graph &captureTarget, ze_event_handle_t event);

bool isCapturingAllowed(const L0::CommandList &srcCmdList);
bool usesForkEvents(std::span<ze_event_handle_t> events);

template <CaptureApi api, typename... TArgs>
ze_result_t captureCommand(L0::CommandList &srcCmdList, Graph *&captureTarget, TArgs... apiArgs) {
    if (false == areGraphsEnabled()) {
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    auto eventsWaitList = getCommandsWaitEventsList<api>(apiArgs...);
    if (false == isCapturingAllowed(srcCmdList)) {
        // it's an error to try and fork to a cmdlist that doesn't support capturing
        return usesForkEvents(eventsWaitList) ? ZE_RESULT_ERROR_INVALID_COMMAND_LIST_TYPE : ZE_RESULT_ERROR_NOT_AVAILABLE;
    }
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

void handleExternalCbEvent(L0::Event *event, ExternalCbEventInfoContainer &container);

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

struct ExecutableGraph;
struct ExecSubGraphBuilder final {
    ExecutableGraph *dst = nullptr;
    L0::CommandList *currCmdList = nullptr;
};

// Contigous commands in the order of recording (i.e. in the order of host API invocations)
struct OrderedCommandsExecutableSegment final {
    ExecutableGraph *dst = nullptr;
    GraphCommandId segmentStart = 0;
};

// List of all ordered segments of a graph
using OrderedExecutableSegmentsList = std::vector<OrderedCommandsExecutableSegment>;

struct ExecGraphBuilder final {
    ExecGraphBuilder(Graph &rootSrc, ExecutableGraph &rootDst);
    void finalize();

    ExecSubGraphBuilder &getSubGraphBuilder(const Graph *src) {
        auto it = subgraphs.find(src);
        UNRECOVERABLE_IF(it == subgraphs.end());
        return it->second;
    }

  protected:
    Graph &rootSrc;
    ExecutableGraph &rootDst;
    std::unordered_map<const Graph *, ExecSubGraphBuilder> subgraphs;
};

struct ExecutableGraph : _ze_executable_graph_handle_t {
    ExecutableGraph(WeaklyShared<OrderedExecutableSegmentsList> &&orderedSegments);
    ExecutableGraph() : ExecutableGraph(WeaklyShared(new OrderedExecutableSegmentsList)) {}

    ze_result_t instantiateFrom(Graph &rootSrc, const GraphInstatiateSettings &settings);
    ze_result_t instantiateFrom(Graph &rootSrc) {
        return this->instantiateFrom(rootSrc, {});
    }

    ~ExecutableGraph();

    static ExecutableGraph *fromHandle(ze_executable_graph_handle_t handle) {
        return static_cast<ExecutableGraph *>(handle);
    }
    inline ze_executable_graph_handle_t toHandle() { return this; }

    bool empty() const {
        return myCommandLists.empty();
    }

    bool isSubGraph() const {
        return (nullptr != executionTarget);
    }

    void addSubgraph(std::unique_ptr<ExecutableGraph> subGraph) {
        subGraphs.push_back(std::move(subGraph));
    }

    const StackVec<std::unique_ptr<ExecutableGraph>, 16> &getSubgraphs() {
        return subGraphs;
    }

    ze_result_t execute(L0::CommandList *executionTarget, void *pNext, ze_event_handle_t hSignalEvent, uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents);
    ze_result_t executeSegment(L0::CommandList *executionTarget, GraphCommandId segmentStart, ze_event_handle_t hSignalEvent, uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents);
    ExternalCbEventInfoContainer &getExternalCbEventInfoContainer() {
        return externalCbEventStorage;
    }

    WeaklyShared<OrderedExecutableSegmentsList> getOrderedCommands() {
        return orderedCommands;
    }

  protected:
    ze_result_t instantiateFrom(const OrderedCommandsSegment &segment, ExecGraphBuilder &builder, const GraphInstatiateSettings &settings);

    L0::CommandList *allocateAndAddCommandListSubmissionNode();

    Graph *src = nullptr;
    L0::CommandList *executionTarget = nullptr;
    std::vector<std::unique_ptr<L0::CommandList>> myCommandLists;
    std::unordered_map<GraphCommandId, L0::CommandList *> myOrderedSegments;
    ExternalCbEventInfoContainer externalCbEventStorage;

    StackVec<std::unique_ptr<ExecutableGraph>, 16> subGraphs;

    bool multiEngineGraph = false;
    bool usePatchingPreamble = true;
    WeaklyShared<OrderedExecutableSegmentsList> orderedCommands; // shared between graph and subgraphs
};

constexpr size_t maxVariantSize = 2 * 64;
#define RR_CAPTURED_API(X) \
    static_assert(sizeof(Closure<CaptureApi::X>) <= maxVariantSize, #X " is too big for common variant. Please export some of its state to ClosureExternalStorage");
RR_CAPTURED_APIS()
#undef RR_CAPTURED_API
} // namespace L0
