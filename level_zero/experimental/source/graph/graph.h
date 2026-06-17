/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/utilities/mem_lifetime.h"
#include "shared/source/utilities/stackvec.h"

#include "level_zero/driver_experimental/zex_visit.h"
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

typedef void(ZE_CALLBACK *zex_mem_graph_free_callback_fn_t)(void *pUserData);

namespace L0 {

inline std::atomic<bool> processUsesGraphs{false};
inline void enabledGraphs() {
    bool graphsEnabled = false;
    processUsesGraphs.compare_exchange_weak(graphsEnabled, true, std::memory_order_relaxed);
}

inline bool areGraphsEnabled() {
    return processUsesGraphs.load();
}

inline std::atomic<uint64_t> graphIdCounter{1};
inline uint64_t getNextGraphId() {
    return graphIdCounter.fetch_add(1, std::memory_order_relaxed);
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
using MclCommandId = uint64_t;

struct Graph;
struct VisitContext;

struct ForkInfo {
    CapturedCommandId forkSignalCommandId = 0;
    ze_event_handle_t forkEvent = nullptr;
};

struct ForkJoinInfo {
    CapturedCommandId forkSignalCommandId = 0; // parent
    CapturedCommandId joinWaitCommandId = 0;   // parent
    CapturedCommandId joinSignalCommandId = 0; // child
    ze_event_handle_t forkEvent = nullptr;
    ze_event_handle_t joinEvent = nullptr;
    Graph *forkDestiny = nullptr; // child
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

struct RecordedApiCommands {
    MOCKABLE_VIRTUAL ~RecordedApiCommands() = default;

    RecordedApiCommands() {
        commands.reserve(16);
    }

    bool empty() const {
        return commands.empty();
    }

    size_t size() const {
        return commands.size();
    }

    void reset() {
        commands.clear();
        mclMap.clear();
        externalStorage.reset();
        brokenCapture = false;
    }

    CapturedCommand *getCommandByMclId(MclCommandId mclCommandId) {
        auto it = mclMap.find(mclCommandId);
        if (it == mclMap.end()) {
            return nullptr;
        }
        CapturedCommandId cmdId = it->second; // this the getNextCommandID call
        ++cmdId;                              // this is the actual command referred by the MCL command id
        if (cmdId >= commands.size()) {
            return nullptr;
        }
        return &commands[cmdId];
    }

    void updateKernelArgument(MclCommandId commandId, uint32_t argIndex, size_t argSize, const void *argValue);
    void updateKernelGroupCount(MclCommandId commandId, const ze_group_count_t *pGroupCount);
    void updateKernelGroupSize(MclCommandId commandId, const uint32_t *groupSize);
    void updateKernelGlobalOffset(MclCommandId commandId, const uint32_t *globalOffset);
    void updateSignalEvent(MclCommandId commandId, ze_event_handle_t signalEvent);
    void updateWaitEvents(MclCommandId commandId, uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents);
    void updateKernel(MclCommandId commandId, ze_kernel_handle_t kernelHandle);

    MOCKABLE_VIRTUAL ze_result_t visit(const ze_visit_ext_desc_t *desc);
    ze_result_t visit(CapturedCommandId id, VisitContext &visitCtx);

    template <CaptureApi api, typename... TArgs>
    ze_result_t capture(TArgs... apiArgs) {
        using ApiArgsT = typename Closure<api>::ApiArgs;
        if (false == Closure<api>::isSupported) {
            brokenCapture = true;
            return ZE_RESULT_ERROR_UNSUPPORTED_ENUMERATION;
        }
        auto capturedArgs = ApiArgsT{apiArgs...};
        commands.push_back(CapturedCommand{Closure<api>(capturedArgs, externalStorage)});
        if (externalStorage.lastResult != ZE_RESULT_SUCCESS) {
            Closure<CaptureApi::NoopedCommandListFailedFunction>::ApiArgs noopedArgs{capturedArgs.hCommandList, CaptureApiStrings::names[static_cast<size_t>(api)]};
            *commands.rbegin() = CapturedCommand{Closure<CaptureApi::NoopedCommandListFailedFunction>(noopedArgs, externalStorage)};
            brokenCapture = true;
            return externalStorage.lastResult;
        }
        if constexpr ((api == CaptureApi::zeCommandListGetNextCommandIdExp) || (api == CaptureApi::zeCommandListGetNextCommandIdWithKernelsExp)) {
            mclMap[*capturedArgs.pCommandId] = static_cast<CapturedCommandId>(commands.size() - 1);
        }
        return ZE_RESULT_SUCCESS;
    }

    std::vector<CapturedCommand> commands;
    std::unordered_map<MclCommandId, CapturedCommandId> mclMap;
    ClosureExternalStorage externalStorage;
    bool brokenCapture = false;
};

struct Graph : _ze_graph_handle_t {
    Graph(L0::Context *ctx, bool preallocated, WeaklyShared<OrderedCommandsRegistry> &&orderedCommandsRegistry)
        : ctx(ctx), preallocated(preallocated),
          orderedCommands(std::move(orderedCommandsRegistry)) {
        if (orderedCommands.empty()) {
            orderedCommands = WeaklyShared<OrderedCommandsRegistry>(new OrderedCommandsRegistry);
        }
        enabledGraphs();
    }

    Graph(L0::Context *ctx, bool preallocated) : Graph(ctx, preallocated, {}) {
    }

    MOCKABLE_VIRTUAL ~Graph();
    Graph(const Graph &) = delete;
    Graph &operator=(const Graph &) = delete;
    Graph(Graph &&) = delete;
    Graph &operator=(Graph &&) = delete;

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

        auto graphCommandId = orderedCommands->acquireNextGraphCommandId();
        if (segments.empty() || (graphCommandId != segments.rbegin()->begin + segments.rbegin()->numCommands)) { // new segment
            segments.push_back(OrderedCommandsSegment{.subgraph = this, .begin = graphCommandId, .subBegin = static_cast<uint32_t>(recordedApiCommands.size()), .numCommands = 1});
        } else {
            ++segments.rbegin()->numCommands;
        }

        return recordedApiCommands.capture<api>(apiArgs...);
    }

    const std::vector<CapturedCommand> &getCapturedCommands() const {
        return recordedApiCommands.commands;
    }

    const StackVec<Graph *, 16> &getSubgraphs() const {
        return subGraphs;
    }

    const std::unordered_map<CapturedCommandId, ForkJoinInfo> &getJoinedForks() const {
        return potentialJoins;
    }

    const std::unordered_map<Graph *, ForkJoinInfo> &getResolvedJoins() const {
        return resolvedJoins;
    }

    const std::unordered_map<L0::Event *, CapturedCommandId> &getRecordedSignals() const {
        return recordedSignals;
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

    uint64_t getId() const {
        const Graph *root = this;
        while (nullptr != root->parentGraph) {
            root = root->parentGraph;
        }
        return root->id;
    }

    struct CaptureTargetDesc {
        ze_device_handle_t hDevice = nullptr;
        ze_command_list_desc_t desc{};
    };

    const CaptureTargetDesc &getCaptureTargetDesc() const {
        return captureTargetDesc;
    }

    bool empty() const {
        return recordedApiCommands.empty();
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

    ze_result_t pauseCapturing();
    ze_result_t resumeCapturing();

    void tryJoinOnNextCommand(L0::CommandList &childCmdList, L0::Event &joinEvent);
    void forkTo(L0::CommandList &childCmdList, Graph *&child, L0::Event &forkEvent);
    void registerSignallingEventFromPreviousCommand(L0::Event &ev);
    ClosureExternalStorage &getExternalStorage() {
        return recordedApiCommands.externalStorage;
    }

    const ClosureExternalStorage &getExternalStorage() const {
        return recordedApiCommands.externalStorage;
    }

    bool isValidJoinCommand(CapturedCommandId commandId) const {
        if (commandId + 1 == recordedApiCommands.size()) {
            return true; // last command
        }
        ++commandId;
        while (commandId < recordedApiCommands.size()) {
            const auto &cmd = recordedApiCommands.commands[commandId];
            if (false == isAllowedPostJoin(static_cast<CaptureApi>(cmd.index()))) {
                return false;
            }
            ++commandId;
        }
        return true;
    }

    bool isLastCommand(CapturedCommandId commandId) const {
        return (commandId + 1 == recordedApiCommands.size());
    }

    const OrderedCommandsRegistry &getOrderedCommands() const {
        return *orderedCommands;
    }

    bool isMultiEngineGraph() const {
        return multiEngineGraph;
    }

    MOCKABLE_VIRTUAL ze_result_t visit(const ze_visit_ext_desc_t *desc);

    ze_result_t addDestructorCallback(zex_mem_graph_free_callback_fn_t pfnCallback, void *pUserData, void *pNext) {
        if (pNext) {
            return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
        }
        std::lock_guard lock(destructorCallbacksMutex);
        destructorCallbacks.push_back({pfnCallback, pUserData});
        return ZE_RESULT_SUCCESS;
    }

    Graph *getParentGraph() const {
        return parentGraph;
    }

    L0::CommandList *getCaptureSource() const {
        return captureSrc;
    }

    L0::CommandList *getPrimaryCaptureSource() const {
        return primaryCaptureSrc;
    }

  protected:
    void setCaptureTargetRecursively(bool attach);
    void setRecordedSignalsRecursively(bool attach);
    void unregisterSignallingEvents();

    RecordedApiCommands recordedApiCommands;
    CaptureTargetDesc captureTargetDesc;

    std::vector<OrderedCommandsSegment> segments;
    StackVec<Graph *, 16> subGraphs;

    std::unordered_map<L0::Event *, CapturedCommandId> recordedSignals;
    std::unordered_map<L0::CommandList *, ForkInfo> unjoinedForks;
    std::unordered_map<CapturedCommandId, ForkJoinInfo> potentialJoins;
    std::unordered_map<Graph *, ForkJoinInfo> resolvedJoins;

    L0::CommandList *captureSrc = nullptr;
    L0::CommandList *primaryCaptureSrc = nullptr;
    L0::CommandList *executionTarget = nullptr;
    L0::Context *ctx = nullptr;
    Graph *parentGraph = nullptr;

    const uint64_t id = getNextGraphId();

    bool preallocated = false;
    bool wasCapturingStopped = false;
    bool multiEngineGraph = false;

    WeaklyShared<OrderedCommandsRegistry> orderedCommands; // shared between graph and subgraphs

    struct DestructorCallbackEntry {
        zex_mem_graph_free_callback_fn_t pfnCallback;
        void *pUserData;
    };
    std::mutex destructorCallbacksMutex;
    std::vector<DestructorCallbackEntry> destructorCallbacks;
};

void recordHandleWaitEventsFromNextCommand(L0::CommandList &srcCmdList, Graph *&captureTarget, std::span<ze_event_handle_t> events);
void recordHandleSignalEventFromPreviousCommand(L0::CommandList &srcCmdList, Graph &captureTarget, ze_event_handle_t event);

bool isGraphCapturingAllowed(const L0::CommandList &srcCmdList);
bool usesForkEvents(std::span<ze_event_handle_t> events);

template <CaptureApi api, typename... TArgs>
ze_result_t captureCommand(L0::CommandList &srcCmdList, Graph *&graphCaptureTarget, RecordedApiCommands *flatCaptureTarget, TArgs... apiArgs) {
    if (flatCaptureTarget) {
        flatCaptureTarget->capture<api>(apiArgs...);
    }

    if (false == areGraphsEnabled()) {
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    auto eventsWaitList = getCommandsWaitEventsList<api>(apiArgs...);
    if (false == isGraphCapturingAllowed(srcCmdList)) {
        // it's an error to try and fork to a cmdlist that doesn't support capturing
        return usesForkEvents(eventsWaitList) ? ZE_RESULT_ERROR_INVALID_COMMAND_LIST_TYPE : ZE_RESULT_ERROR_NOT_AVAILABLE;
    }
    if ((false == eventsWaitList.empty()) && ((nullptr == graphCaptureTarget) || (graphCaptureTarget->hasUnjoinedForks()))) { // either is not capturing and is potential fork or this can be a join operation
        recordHandleWaitEventsFromNextCommand(srcCmdList, graphCaptureTarget, eventsWaitList);
    }

    if (nullptr == graphCaptureTarget) {
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    auto ret = graphCaptureTarget->capture<api>(apiArgs...);
    if (ZE_RESULT_SUCCESS != ret) {
        return ret;
    }

    if (getCommandsSignalEvent<api>(apiArgs...)) {
        recordHandleSignalEventFromPreviousCommand(srcCmdList, *graphCaptureTarget, getCommandsSignalEvent<api>(apiArgs...));
    }
    return ZE_RESULT_SUCCESS;
}

struct ExecutableGraph;
using GraphSubmissionSegment = std::variant<L0::CommandList *, ExecutableGraph *>;
using GraphSubmissionChain = std::vector<GraphSubmissionSegment>;

void handleExternalCbEvent(L0::Event *event, CbExternalEventInstantiateContext &cbEventContext);

struct GraphInstatiateSettings {
    GraphInstatiateSettings() = default;
    GraphInstatiateSettings(void *pNext, bool multiEngineGraph);

    enum ForkPolicy {
        ForkPolicyMonolythicLevels, // build and submit monolythic commandlists for each level
        ForkPolicySplitLevels,      // split commandlists on forks and interleave submission with child graphs (prevents deadlocks when submitting to single HW queue)
        ForkPolicyFlat              // commands are interleaved like with ForkPolicySplitLevels, but the resulting command sequence (graph) is flattened to a single cmdlist
    };

    ForkPolicy forkPolicy = ForkPolicyFlat;
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
    ~ExecGraphBuilder();
    ExecGraphBuilder(const ExecGraphBuilder &) = delete;
    ExecGraphBuilder &operator=(const ExecGraphBuilder &) = delete;
    ExecGraphBuilder(ExecGraphBuilder &&) = delete;
    ExecGraphBuilder &operator=(ExecGraphBuilder &&) = delete;
    void finalize(const GraphInstatiateSettings &settings);

    L0::CommandList *getFlatCommandList() const {
        return flatCommandList;
    }

    void setFlatCommandList(L0::CommandList *newFlatCommandList) {
        flatCommandList = newFlatCommandList;
    }

    ExecSubGraphBuilder &getSubGraphBuilder(const Graph *src) {
        auto it = subgraphs.find(src);
        UNRECOVERABLE_IF(it == subgraphs.end());
        return it->second;
    }

    L0::EventPool *releaseTrailingEventsPool() {
        auto ret = this->trailingEventsPool;
        this->trailingEventsPool = nullptr;
        return ret;
    }

    std::vector<ze_event_handle_t> releaseTrailingEvents() {
        return {std::move(this->trailingEvents)};
    }

  protected:
    void createEventPoolForTrailingEvents(size_t numEvents);
    L0::Event *createTrailingEvent();

    Graph &rootSrc;
    ExecutableGraph &rootDst;
    L0::CommandList *flatCommandList = nullptr;
    std::unordered_map<const Graph *, ExecSubGraphBuilder> subgraphs;
    L0::EventPool *trailingEventsPool = nullptr;
    std::vector<ze_event_handle_t> trailingEvents;
};

struct ExecutableGraph : _ze_executable_graph_handle_t {
    ExecutableGraph(WeaklyShared<OrderedExecutableSegmentsList> &&orderedSegments,
                    WeaklyShared<ExternalCbEventInfoContainer> &&externalCbEvent);
    ExecutableGraph() : ExecutableGraph(WeaklyShared(new OrderedExecutableSegmentsList),
                                        WeaklyShared(new ExternalCbEventInfoContainer)) {}
    ExecutableGraph(const ExecutableGraph &) = delete;
    ExecutableGraph &operator=(const ExecutableGraph &) = delete;
    ExecutableGraph(ExecutableGraph &&) = delete;
    ExecutableGraph &operator=(ExecutableGraph &&) = delete;

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

    WeaklyShared<ExternalCbEventInfoContainer> getExternalCbEventInfoContainer() {
        return externalCbEventStorage;
    }

    bool segmentRequiresSeperateSubmission(GraphCommandId segmentStart) const {
        return this->myOrderedSegments.end() != this->myOrderedSegments.find(segmentStart);
    }

    WeaklyShared<OrderedExecutableSegmentsList> getOrderedCommands() {
        return orderedCommands;
    }

    Graph *getSourceGraph() const {
        return src;
    }

  protected:
    ze_result_t instantiateFrom(const OrderedCommandsSegment &segment, ExecGraphBuilder &builder, const GraphInstatiateSettings &settings);

    L0::CommandList *allocateAndAddCommandListSubmissionNode();

    Graph *src = nullptr;
    L0::CommandList *executionTarget = nullptr;
    std::vector<std::unique_ptr<L0::CommandList>> myCommandLists;
    std::unordered_map<GraphCommandId, L0::CommandList *> myOrderedSegments;

    StackVec<std::unique_ptr<ExecutableGraph>, 16> subGraphs;

    WeaklyShared<OrderedExecutableSegmentsList> orderedCommands; // shared between graph and subgraphs
    WeaklyShared<ExternalCbEventInfoContainer> externalCbEventStorage;

    L0::EventPool *trailingEventsPool = nullptr;
    std::vector<ze_event_handle_t> trailingEvents;

    bool usePatchingPreamble = true;
};

constexpr size_t maxVariantSize = 2 * 64;
#define RR_CAPTURED_API(X) \
    static_assert(sizeof(Closure<CaptureApi::X>) <= maxVariantSize, #X " is too big for common variant. Please export some of its state to ClosureExternalStorage");
RR_CAPTURED_APIS()
#undef RR_CAPTURED_API
} // namespace L0
