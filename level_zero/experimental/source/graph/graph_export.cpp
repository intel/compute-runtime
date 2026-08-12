/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/experimental/source/graph/graph_export.h"

#include "shared/source/os_interface/sys_calls_common.h"
#include "shared/source/program/kernel_info.h"
#include "shared/source/utilities/io_functions.h"

#include "level_zero/core/source/cmdlist/cmdlist.h"
#include "level_zero/core/source/event/event.h"
#include "level_zero/core/source/kernel/kernel.h"
#include "level_zero/core/source/kernel/kernel_imp.h"
#include "level_zero/experimental/source/graph/graph.h"
#include "level_zero/ze_api.h"

#include <algorithm>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace L0 {

ze_result_t GraphDotExporter::exportToFile(const Graph &graph, const char *filePath) const {
    if (nullptr == filePath) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    FILE *file = NEO::IoFunctions::fopenPtr(filePath, "w");
    if (nullptr == file) {
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Failed to open file %s for writing graph content\n", filePath);
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    std::string dotContent = exportToString(graph);
    size_t bytesWritten = NEO::IoFunctions::fwritePtr(dotContent.c_str(), sizeof(char), dotContent.size(), file);
    NEO::IoFunctions::fclosePtr(file);

    if (bytesWritten != dotContent.size()) {
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Failed to write graph content to file %s\n", filePath);
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    return ZE_RESULT_SUCCESS;
}

std::string getGraphDumpDefaultFileName(const Graph &graph, const ExecutableGraph &executableGraph) {
    auto processName = NEO::SysCalls::getProcessName();
    if (processName.empty()) {
        processName = "unknown";
    }
    return processName + "_" + std::to_string(NEO::SysCalls::getProcessId()) + "_" + std::to_string(graph.getId()) +
           "_" + GraphDumpHelper::formatPointer(&graph) + "_" + GraphDumpHelper::formatPointer(&executableGraph) + ".dot";
}

void dumpGraphOnInstantiate(const Graph &graph, const ExecutableGraph &executableGraph) {
    const auto fileName = getGraphDumpDefaultFileName(graph, executableGraph);
    GraphDotExporter exporter{GraphExportStyle::detailed, GraphExportEventNodes::hideInternal};
    exporter.exportToFile(graph, fileName.c_str());
}

std::string GraphDotExporter::exportToString(const Graph &graph) const {
    std::ostringstream dot;

    writeHeader(dot);
    writeNodes(dot, graph, 0, 0);
    writeEdges(dot, graph, 0, 0);
    writeSubgraphs(dot, graph, 0);

    dot << "}\n";
    return dot.str();
}

void GraphDotExporter::writeHeader(std::ostringstream &dot) const {
    dot << "digraph \"graph\" {\n";
    dot << "  rankdir=TB;\n";
    dot << "  dpi=300;\n";

    if (exportStyle == GraphExportStyle::simple) {
        dot << "  node [style=filled];\n";
    } else if (exportStyle == GraphExportStyle::detailed) {
        dot << "  nodesep=1;\n";
        dot << "  ranksep=1;\n";
        dot << "  node [shape=box, style=filled];\n";
    }

    dot << "  edge [color=black];\n\n";
}

void GraphDotExporter::writeNodes(std::ostringstream &dot, const Graph &graph, uint32_t level, uint32_t subgraphId) const {
    const std::string indent(static_cast<size_t>(level + 1) * 2, ' ');
    dot << indent << "// Command nodes:\n";

    for (const auto cmdId : collectVisibleCommands(graph)) {
        const std::string nodeId = generateNodeId(level, subgraphId, cmdId);
        const std::string label = getCommandNodeLabel(graph, cmdId, indent);
        const std::string attributes = getCommandNodeAttributes(graph, cmdId);

        dot << indent << nodeId << " [label=" << label << attributes << "];\n";
    }
    dot << "\n";
}

void GraphDotExporter::writeEdges(std::ostringstream &dot, const Graph &graph, uint32_t level, uint32_t subgraphId) const {
    writeSequentialEdges(dot, graph, level, subgraphId);
    writeForkJoinEdges(dot, graph, level, subgraphId);
    writeUnjoinedForkEdges(dot, graph, level, subgraphId);

    dot << "\n";
}

void GraphDotExporter::writeSequentialEdges(std::ostringstream &dot, const Graph &graph, uint32_t level, uint32_t subgraphId) const {
    const std::string indent(static_cast<size_t>(level + 1) * 2, ' ');

    const auto visibleCommands = collectVisibleCommands(graph);

    if (visibleCommands.size() > 1) {
        dot << indent << "// Sequential edges:\n";
    }

    for (size_t i = 1; i < visibleCommands.size(); ++i) {
        const std::string fromNode = generateNodeId(level, subgraphId, visibleCommands[i - 1]);
        const std::string toNode = generateNodeId(level, subgraphId, visibleCommands[i]);
        dot << indent << fromNode << " -> " << toNode << ";\n";
    }
}

void GraphDotExporter::writeForkJoinEdges(std::ostringstream &dot, const Graph &graph, uint32_t level, uint32_t subgraphId) const {
    const std::string indent(static_cast<size_t>(level + 1) * 2, ' ');

    const auto &potentialJoins = graph.getJoinedForks();
    const auto &subGraphs = graph.getSubgraphs();

    if (!potentialJoins.empty()) {
        dot << "\n"
            << indent << "// Fork/Join edges:\n";
    }

    const auto visibleCommands = collectVisibleCommands(graph);

    for (const auto &[forkCmdId, forkJoinInfo] : potentialJoins) {
        const auto subgraphIndex = findSubgraphIndex(subGraphs, forkJoinInfo.forkDestiny);
        if (false == subgraphIndex.has_value()) {
            continue;
        }

        const auto subgraphVisibleCommands = collectVisibleCommands(*forkJoinInfo.forkDestiny);
        if (subgraphVisibleCommands.empty()) {
            continue;
        }

        const auto forkCommandId = findVisibleCommandAtOrBefore(visibleCommands, forkJoinInfo.forkSignalCommandId);
        if (forkCommandId) {
            const std::string forkNode = generateNodeId(level, subgraphId, *forkCommandId);
            const std::string subgraphFirstNode = generateNodeId(level + 1, *subgraphIndex, subgraphVisibleCommands.front());
            dot << indent << forkNode << " -> " << subgraphFirstNode << ";\n";
        }

        const auto joinCommandId = findVisibleCommandAtOrAfter(visibleCommands, forkJoinInfo.joinWaitCommandId);
        if (joinCommandId) {
            const std::string subgraphLastNode = generateNodeId(level + 1, *subgraphIndex, subgraphVisibleCommands.back());
            const std::string joinNode = generateNodeId(level, subgraphId, *joinCommandId);
            dot << indent << subgraphLastNode << " -> " << joinNode << ";\n";
        }
    }
}

void GraphDotExporter::writeUnjoinedForkEdges(std::ostringstream &dot, const Graph &graph, uint32_t level, uint32_t subgraphId) const {
    const std::string indent(static_cast<size_t>(level + 1) * 2, ' ');

    const auto &unjoinedForks = graph.getUnjoinedForks();
    const auto &subGraphs = graph.getSubgraphs();

    if (!unjoinedForks.empty()) {
        dot << "\n"
            << indent << "// Unjoined forks:\n";
    }

    const auto visibleCommands = collectVisibleCommands(graph);

    for (const auto &[cmdList, forkInfo] : unjoinedForks) {
        const auto subgraphIndex = findSubgraphIndexByCommandList(subGraphs, cmdList);
        if (false == subgraphIndex.has_value()) {
            continue;
        }

        const auto subgraphVisibleCommands = collectVisibleCommands(*subGraphs[*subgraphIndex]);
        const auto forkCommandId = findVisibleCommandAtOrBefore(visibleCommands, forkInfo.forkSignalCommandId);
        if (subgraphVisibleCommands.empty() || (false == forkCommandId.has_value())) {
            continue;
        }

        const std::string forkNode = generateNodeId(level, subgraphId, *forkCommandId);
        const std::string subgraphFirstNode = generateNodeId(level + 1, *subgraphIndex, subgraphVisibleCommands.front());
        dot << indent << forkNode << " -> " << subgraphFirstNode << " [color=red, label=\"unjoined fork\"];\n";
    }
}

std::optional<uint32_t> GraphDotExporter::findSubgraphIndex(std::span<Graph *const> subGraphs, const Graph *targetGraph) const {
    for (uint32_t i = 0; i < static_cast<uint32_t>(subGraphs.size()); ++i) {
        if (subGraphs[i] == targetGraph) {
            return i;
        }
    }
    return std::nullopt;
}

std::optional<uint32_t> GraphDotExporter::findSubgraphIndexByCommandList(std::span<Graph *const> subGraphs, const L0::CommandList *cmdList) const {
    for (uint32_t i = 0; i < static_cast<uint32_t>(subGraphs.size()); ++i) {
        if (subGraphs[i]->getExecutionTarget() == cmdList) {
            return i;
        }
    }
    return std::nullopt;
}

namespace {

std::vector<ze_event_handle_t> getEventOnlyCommandEvents(const CapturedCommand &cmd, const ClosureExternalStorage &storage) {
    std::vector<ze_event_handle_t> events;

    auto addEvent = [&events](ze_event_handle_t event) {
        if (nullptr != event) {
            events.push_back(event);
        }
    };
    auto addWaitEvents = [&events, &storage](const auto &closure, uint32_t numEvents) {
        const auto *waitEvents = storage.getEventsList(closure.indirectArgs.waitEvents);
        for (uint32_t i = 0; i < numEvents; ++i) {
            if (nullptr != waitEvents[i]) {
                events.push_back(waitEvents[i]);
            }
        }
    };

    switch (static_cast<CaptureApi>(cmd.index())) {
    case CaptureApi::zeCommandListAppendSignalEvent: {
        const auto &closure = std::get<static_cast<size_t>(CaptureApi::zeCommandListAppendSignalEvent)>(cmd);
        addEvent(closure.apiArgs.hEvent);
        break;
    }
    case CaptureApi::zeCommandListAppendWaitOnEvents: {
        const auto &closure = std::get<static_cast<size_t>(CaptureApi::zeCommandListAppendWaitOnEvents)>(cmd);
        addWaitEvents(closure, closure.apiArgs.numEvents);
        break;
    }
    case CaptureApi::zeCommandListAppendBarrier: {
        const auto &closure = std::get<static_cast<size_t>(CaptureApi::zeCommandListAppendBarrier)>(cmd);
        addEvent(closure.apiArgs.hSignalEvent);
        addWaitEvents(closure, closure.apiArgs.numWaitEvents);
        break;
    }
    default:
        break;
    }

    return events;
}

bool isUsedOnlyForInternalDependencyTracking(const CapturedCommand &cmd, const ClosureExternalStorage &storage,
                                             std::span<const ze_event_handle_t> internalDependencyEvents) {
    const auto commandEvents = getEventOnlyCommandEvents(cmd, storage);
    if (commandEvents.empty()) {
        return false;
    }

    return std::all_of(commandEvents.begin(), commandEvents.end(), [internalDependencyEvents](ze_event_handle_t event) {
        return std::find(internalDependencyEvents.begin(), internalDependencyEvents.end(), event) != internalDependencyEvents.end();
    });
}

} // namespace

GraphDotExporter::InternalCommandsSet GraphDotExporter::collectInternalDependencyCommands(const Graph &graph) const {
    InternalCommandsSet internalCommands;
    if (GraphExportEventNodes::show == exportEventNodes) {
        return internalCommands;
    }

    const auto &commands = graph.getCapturedCommands();
    std::unordered_map<CapturedCommandId, std::vector<ze_event_handle_t>> internalDependencyEvents;
    auto trackDependencyEvent = [&internalDependencyEvents, &commands](CapturedCommandId cmdId, ze_event_handle_t dependencyEvent) {
        if (cmdId >= commands.size()) {
            return;
        }
        if ((nullptr == dependencyEvent) || L0::Event::fromHandle(dependencyEvent)->isExternalEvent()) {
            return;
        }
        internalDependencyEvents[cmdId].push_back(dependencyEvent);
    };

    for (const auto &[_, forkJoinInfo] : graph.getJoinedForks()) {
        trackDependencyEvent(forkJoinInfo.forkSignalCommandId, forkJoinInfo.forkEvent);
        trackDependencyEvent(forkJoinInfo.joinWaitCommandId, forkJoinInfo.joinEvent);
    }
    for (const auto &[_, forkInfo] : graph.getUnjoinedForks()) {
        trackDependencyEvent(forkInfo.forkSignalCommandId, forkInfo.forkEvent);
    }

    // operations which link this subgraph with its parent are tracked by the parent
    const auto *parentGraph = graph.getParentGraph();
    if (nullptr != parentGraph) {
        constexpr CapturedCommandId forkWaitCommandId = 0;
        for (const auto &[_, forkJoinInfo] : parentGraph->getJoinedForks()) {
            if (forkJoinInfo.forkDestiny != &graph) {
                continue;
            }
            trackDependencyEvent(forkWaitCommandId, forkJoinInfo.forkEvent);
            trackDependencyEvent(forkJoinInfo.joinSignalCommandId, forkJoinInfo.joinEvent);
        }
        for (const auto &[cmdList, forkInfo] : parentGraph->getUnjoinedForks()) {
            if (cmdList != graph.getExecutionTarget()) {
                continue;
            }
            trackDependencyEvent(forkWaitCommandId, forkInfo.forkEvent);
        }
    }

    for (const auto &[cmdId, dependencyEvents] : internalDependencyEvents) {
        if (isUsedOnlyForInternalDependencyTracking(commands[cmdId], graph.getExternalStorage(), dependencyEvents)) {
            internalCommands.insert(cmdId);
        }
    }

    return internalCommands;
}

std::vector<CapturedCommandId> GraphDotExporter::collectVisibleCommands(const Graph &graph) const {
    const auto internalCommands = collectInternalDependencyCommands(graph);
    const auto &commands = graph.getCapturedCommands();

    std::vector<CapturedCommandId> visibleCommands;
    visibleCommands.reserve(commands.size());
    for (CapturedCommandId cmdId = 0; cmdId < static_cast<uint32_t>(commands.size()); ++cmdId) {
        if (internalCommands.count(cmdId) > 0) {
            continue;
        }
        visibleCommands.push_back(cmdId);
    }

    return visibleCommands;
}

std::optional<CapturedCommandId> GraphDotExporter::findVisibleCommandAtOrBefore(std::span<const CapturedCommandId> visibleCommands, CapturedCommandId cmdId) {
    auto it = std::upper_bound(visibleCommands.begin(), visibleCommands.end(), cmdId);
    if (it == visibleCommands.begin()) {
        return std::nullopt;
    }
    return *(it - 1);
}

std::optional<CapturedCommandId> GraphDotExporter::findVisibleCommandAtOrAfter(std::span<const CapturedCommandId> visibleCommands, CapturedCommandId cmdId) {
    auto it = std::lower_bound(visibleCommands.begin(), visibleCommands.end(), cmdId);
    if (it == visibleCommands.end()) {
        return std::nullopt;
    }
    return *it;
}

void GraphDotExporter::writeSubgraphs(std::ostringstream &dot, const Graph &graph, uint32_t level) const {
    const auto &subGraphs = graph.getSubgraphs();
    if (subGraphs.empty()) {
        return;
    }

    const std::string indent(static_cast<size_t>(level + 1) * 2, ' ');
    dot << indent << "// Subgraphs:\n";

    for (uint32_t subgraphId = 0; subgraphId < static_cast<uint32_t>(subGraphs.size()); ++subgraphId) {
        if (exportStyle == GraphExportStyle::detailed) {
            const std::string clusterName = "cluster_" + generateSubgraphId(level + 1, subgraphId);
            dot << indent << "subgraph " << clusterName << " {\n";
            dot << indent << "  label=\"Subgraph " << (level + 1) << "-" << subgraphId << "\";\n";
            dot << indent << "  style=filled;\n";
            dot << indent << "  fillcolor=" << getSubgraphFillColor(level + 1) << ";\n\n";
        }

        writeNodes(dot, *subGraphs[subgraphId], level + 1, subgraphId);
        writeEdges(dot, *subGraphs[subgraphId], level + 1, subgraphId);
        writeSubgraphs(dot, *subGraphs[subgraphId], level + 1);

        if (exportStyle == GraphExportStyle::detailed) {
            dot << indent << "  }\n\n";
        }
    }
}

std::string GraphDotExporter::getCommandNodeLabel(const Graph &graph, CapturedCommandId cmdId, const std::string_view indent) const {
    const auto &commands = graph.getCapturedCommands();
    const auto &cmd = commands[cmdId];

    std::string_view baseLabel;
    std::vector<std::pair<std::string, std::string>> params;

    switch (static_cast<CaptureApi>(cmd.index())) {
#define RR_CAPTURED_API(X)                                                                                                              \
    case CaptureApi::X:                                                                                                                 \
        baseLabel = #X;                                                                                                                 \
        if (exportStyle == GraphExportStyle::detailed) {                                                                                \
            params = GraphDumpHelper::extractParameters(std::get<static_cast<size_t>(CaptureApi::X)>(cmd), graph.getExternalStorage()); \
        }                                                                                                                               \
        break;

        RR_CAPTURED_APIS()
#undef RR_CAPTURED_API

    default:
        baseLabel = "Unknown";
        break;
    }

    std::ostringstream label;
    if (exportStyle == GraphExportStyle::simple) {
        label << "\"";
        label << "TYPE = " << baseLabel;
        std::string_view kernelName = GraphDumpHelper::getKernelName(cmd);
        if (!kernelName.empty()) {
            label << "\\nNAME = " << kernelName;
        }
        label << "\"";
    } else if (exportStyle == GraphExportStyle::detailed) {
        label << "\n";
        label << indent << "<"
              << "<TABLE BORDER=\"0\" CELLBORDER=\"1\" CELLSPACING=\"0\" CELLPADDING=\"4\">\n";
        label << indent << "  <TR><TD COLSPAN=\"2\"><B>" << baseLabel << "</B></TD></TR>\n";

        for (const auto &param : params) {
            label << indent << "  ";
            label << "<TR><TD ALIGN=\"LEFT\" BGCOLOR=\"azure\">" << param.first << "</TD>";
            label << "<TD ALIGN=\"LEFT\" BGCOLOR=\"white\"><FONT FACE=\"monospace\">" << param.second << "</FONT></TD></TR>\n";
        }

        label << indent << "</TABLE>"
              << ">, shape=plain";
    }

    return label.str();
}

std::string GraphDotExporter::getCommandNodeAttributes(const Graph &graph, CapturedCommandId cmdId) const {
    if (exportStyle == GraphExportStyle::simple) {
        return ", fillcolor=white";
    }

    const auto &commands = graph.getCapturedCommands();
    const auto &cmd = commands[cmdId];

    switch (static_cast<CaptureApi>(cmd.index())) {
    case CaptureApi::zeCommandListAppendMemoryCopy:
    case CaptureApi::zeCommandListAppendMemoryCopyRegion:
    case CaptureApi::zeCommandListAppendMemoryCopyFromContext:
    case CaptureApi::zeCommandListAppendMemoryFill:
        return ", fillcolor=lightblue";

    case CaptureApi::zeCommandListAppendBarrier:
    case CaptureApi::zeCommandListAppendMemoryRangesBarrier:
        return ", fillcolor=orange";

    case CaptureApi::zeCommandListAppendSignalEvent:
    case CaptureApi::zeCommandListAppendWaitOnEvents:
    case CaptureApi::zeCommandListAppendEventReset:
        return ", fillcolor=yellow";

    case CaptureApi::zeCommandListAppendImageCopy:
    case CaptureApi::zeCommandListAppendImageCopyRegion:
    case CaptureApi::zeCommandListAppendImageCopyToMemory:
    case CaptureApi::zeCommandListAppendImageCopyFromMemory:
    case CaptureApi::zeCommandListAppendImageCopyToMemoryExt:
    case CaptureApi::zeCommandListAppendImageCopyFromMemoryExt:
        return ", fillcolor=lightgreen";

    case CaptureApi::zeCommandListAppendWriteGlobalTimestamp:
    case CaptureApi::zeCommandListAppendQueryKernelTimestamps:
        return ", fillcolor=pink";

    case CaptureApi::NoopedCommandListFailedFunction:
        return ", fillcolor=red";

    default:
        return ", fillcolor=aliceblue";
    }
}

std::string GraphDotExporter::generateNodeId(uint32_t level, uint32_t subgraphId, CapturedCommandId cmdId) const {
    std::ostringstream oss;
    oss << "L" << level << "_S" << subgraphId << "_C" << cmdId;
    return oss.str();
}

std::string GraphDotExporter::generateSubgraphId(uint32_t level, uint32_t subgraphId) const {
    std::ostringstream oss;
    oss << "L" << level << "_S" << subgraphId;
    return oss.str();
}

std::string GraphDotExporter::getSubgraphFillColor(uint32_t level) const {
    const std::vector<std::string> colors = {
        "grey90", // Level 1
        "grey80", // Level 2
        "grey70", // Level 3
        "grey60", // Level 4
        "grey50"  // Level 5+
    };

    size_t colorIndex = static_cast<size_t>(level) - 1;
    if (colorIndex >= colors.size()) {
        colorIndex = colors.size() - 1;
    }

    return colors[colorIndex];
}

namespace GraphDumpHelper {

std::string formatPointer(const void *ptr) {
    std::ostringstream addr;
    addr << "0x" << std::hex << std::uppercase << reinterpret_cast<uintptr_t>(ptr);
    return addr.str();
}

std::string formatGroupCount(const ze_group_count_t &groupCount) {
    return std::to_string(groupCount.groupCountX) + "x" +
           std::to_string(groupCount.groupCountY) + "x" +
           std::to_string(groupCount.groupCountZ);
}

std::string formatGroupSize(const ze_group_size_t &groupSize) {
    return std::to_string(groupSize.groupSizeX) + "x" +
           std::to_string(groupSize.groupSizeY) + "x" +
           std::to_string(groupSize.groupSizeZ);
}

template <typename ApiArgsT>
std::vector<std::pair<std::string, std::string>> createBaseParams(const ApiArgsT &apiArgs) {
    return {{"hCommandList", formatPointer(apiArgs.hCommandList)}};
}

template <typename Closure>
void addCommonEventParameters(std::vector<std::pair<std::string, std::string>> &params,
                              const Closure &closure,
                              const ClosureExternalStorage &storage) {
    if constexpr (HasHSignalEvent<typename Closure::ApiArgs>) {
        if (closure.apiArgs.hSignalEvent != nullptr) {
            params.emplace_back("hSignalEvent", formatPointer(closure.apiArgs.hSignalEvent));
        }
    }

    if constexpr (requires { closure.indirectArgs.waitEvents; }) {
        auto *events = storage.getEventsList(closure.indirectArgs.waitEvents);
        if constexpr (HasPhWaitEvents<typename Closure::ApiArgs>) {
            if (closure.apiArgs.numWaitEvents > 0) {
                params.emplace_back("numWaitEvents", std::to_string(closure.apiArgs.numWaitEvents));
                params.emplace_back("phWaitEvents", formatPointer(static_cast<const void *>(events)));
                for (uint32_t i = 0; i < closure.apiArgs.numWaitEvents; ++i) {
                    std::string eventKey = "phWaitEvents[" + std::to_string(i) + "]";
                    params.emplace_back(eventKey, formatPointer(events[i]));
                }
            }
        }
        if constexpr (HasPhEvents<typename Closure::ApiArgs>) {
            if (closure.apiArgs.numEvents > 0) {
                params.emplace_back("numEvents", std::to_string(closure.apiArgs.numEvents));
                params.emplace_back("phEvents", formatPointer(static_cast<const void *>(events)));
                for (uint32_t i = 0; i < closure.apiArgs.numEvents; ++i) {
                    std::string eventKey = "phEvents[" + std::to_string(i) + "]";
                    params.emplace_back(eventKey, formatPointer(events[i]));
                }
            }
        }
    }
}

template <typename ApiArgsT>
void addOptionalRegionParameters(std::vector<std::pair<std::string, std::string>> &params, const ApiArgsT &apiArgs) {
    if constexpr (requires { apiArgs.pDstRegion; }) {
        if (apiArgs.pDstRegion != nullptr) {
            params.emplace_back("pDstRegion", formatPointer(apiArgs.pDstRegion));
        }
    }
    if constexpr (requires { apiArgs.pSrcRegion; }) {
        if (apiArgs.pSrcRegion != nullptr) {
            params.emplace_back("pSrcRegion", formatPointer(apiArgs.pSrcRegion));
        }
    }
}

std::string_view getKernelName(const ze_kernel_handle_t kernelHandle) {
    auto *kernelImp = static_cast<L0::KernelImp *>(Kernel::fromHandle(kernelHandle));
    const auto &kernelImmutableData = kernelImp->getImmutableData();
    return kernelImmutableData->getKernelInfo()->kernelDescriptor.kernelMetadata.kernelName;
}

std::string_view getKernelName(const CapturedCommand &cmd) {
    return std::visit([](const auto &closure) -> std::string_view {
        if constexpr (requires { closure.apiArgs.kernelHandle; }) {
            return getKernelName(closure.apiArgs.kernelHandle);
        } else {
            return "";
        }
    },
                      cmd);
}

void addKernelInformation(std::vector<std::pair<std::string, std::string>> &params, const ze_kernel_handle_t kernelHandle) {
    params.emplace_back("hKernel", formatPointer(kernelHandle));
    params.emplace_back("kernelName", getKernelName(kernelHandle));

    auto *kernelImp = static_cast<L0::KernelImp *>(Kernel::fromHandle(kernelHandle));
    const auto &kernelArgInfos = kernelImp->getKernelArgInfos();

    if (kernelArgInfos.size() > 0U) {
        kernelImp->populateMetadata();
        const auto &kernelImmutableData = kernelImp->getImmutableData();

        for (size_t i = 0; i < kernelImmutableData->getDescriptor().explicitArgsExtendedMetadata.size(); ++i) {
            const auto &arg = kernelImmutableData->getDescriptor().explicitArgsExtendedMetadata[i];
            params.emplace_back("arg[" + std::to_string(i) + "]", arg.type + " " + arg.argName);
        }
    }
}

void addLaunchKernelExtensionParameters(std::vector<std::pair<std::string, std::string>> &params, const void *pNext) {
    if (pNext == nullptr) {
        params.emplace_back("pNext", "nullptr");
        return;
    }

    params.emplace_back("pNext", formatPointer(pNext));

    const auto *baseDesc = reinterpret_cast<const ze_base_desc_t *>(pNext);
    while (baseDesc != nullptr) {
        const auto stypeValue = std::to_string(static_cast<uint32_t>(baseDesc->stype));

        if (baseDesc->stype == ZE_STRUCTURE_TYPE_COMMAND_LIST_APPEND_PARAM_COOPERATIVE_DESC) {
            const auto *cooperativeDesc = reinterpret_cast<const ze_command_list_append_launch_kernel_param_cooperative_desc_t *>(baseDesc);
            params.emplace_back("cooperative.stype", stypeValue);
            params.emplace_back("cooperative.isCooperative", (cooperativeDesc->isCooperative != 0) ? "true" : "false");
        } else {
            addLaunchKernelAdditionalExtensionParameters(params, baseDesc);
        }

        baseDesc = reinterpret_cast<const ze_base_desc_t *>(baseDesc->pNext);
    }
}

void addMemoryTransferExtensionParameters(std::vector<std::pair<std::string, std::string>> &params, const void *pNext) {
    if (pNext == nullptr) {
        params.emplace_back("pNext", "nullptr");
        return;
    }

    params.emplace_back("pNext", formatPointer(pNext));

    const auto *baseDesc = reinterpret_cast<const ze_base_desc_t *>(pNext);
    while (baseDesc != nullptr) {
        addMemoryTransferAdditionalExtensionParameters(params, baseDesc);

        baseDesc = reinterpret_cast<const ze_base_desc_t *>(baseDesc->pNext);
    }
}

template <>
std::vector<std::pair<std::string, std::string>> extractParameters<CaptureApi::zeCommandListAppendMemoryCopy>(
    const Closure<CaptureApi::zeCommandListAppendMemoryCopy> &closure, const ClosureExternalStorage &storage) {

    auto params = createBaseParams(closure.apiArgs);
    params.emplace_back("dstptr", formatPointer(closure.apiArgs.dstptr));
    params.emplace_back("srcptr", formatPointer(closure.apiArgs.srcptr));
    params.emplace_back("size", std::to_string(closure.apiArgs.size));

    addCommonEventParameters(params, closure, storage);

    return params;
}

template <>
std::vector<std::pair<std::string, std::string>> extractParameters<CaptureApi::zeCommandListAppendMemoryFill>(
    const Closure<CaptureApi::zeCommandListAppendMemoryFill> &closure, const ClosureExternalStorage &storage) {

    auto params = createBaseParams(closure.apiArgs);
    params.emplace_back("ptr", formatPointer(closure.apiArgs.ptr));
    params.emplace_back("pattern", formatPointer(closure.indirectArgs.pattern.data()));
    params.emplace_back("patternSize", std::to_string(closure.indirectArgs.pattern.size()));
    params.emplace_back("size", std::to_string(closure.apiArgs.size));

    addCommonEventParameters(params, closure, storage);

    return params;
}

template <>
std::vector<std::pair<std::string, std::string>> extractParameters<CaptureApi::zeCommandListAppendLaunchKernel>(
    const Closure<CaptureApi::zeCommandListAppendLaunchKernel> &closure, const ClosureExternalStorage &storage) {

    auto params = createBaseParams(closure.apiArgs);
    addKernelInformation(params, closure.apiArgs.kernelHandle);
    params.emplace_back("launchFuncArgs", formatGroupCount(closure.indirectArgs.launchKernelArgs));
    addCommonEventParameters(params, closure, storage);

    return params;
}

template <>
std::vector<std::pair<std::string, std::string>> extractParameters<CaptureApi::zeCommandListAppendBarrier>(
    const Closure<CaptureApi::zeCommandListAppendBarrier> &closure, const ClosureExternalStorage &storage) {

    auto params = createBaseParams(closure.apiArgs);
    addCommonEventParameters(params, closure, storage);

    return params;
}

template <>
std::vector<std::pair<std::string, std::string>> extractParameters<CaptureApi::zeCommandListAppendWaitOnEvents>(
    const Closure<CaptureApi::zeCommandListAppendWaitOnEvents> &closure, const ClosureExternalStorage &storage) {

    auto params = createBaseParams(closure.apiArgs);
    addCommonEventParameters(params, closure, storage);

    return params;
}

template <>
std::vector<std::pair<std::string, std::string>> extractParameters<CaptureApi::zeCommandListAppendWriteGlobalTimestamp>(
    const Closure<CaptureApi::zeCommandListAppendWriteGlobalTimestamp> &closure, const ClosureExternalStorage &storage) {

    auto params = createBaseParams(closure.apiArgs);
    params.emplace_back("dstptr", formatPointer(closure.apiArgs.dstptr));

    addCommonEventParameters(params, closure, storage);

    return params;
}

template <>
std::vector<std::pair<std::string, std::string>> extractParameters<CaptureApi::zeCommandListAppendMemoryRangesBarrier>(
    const Closure<CaptureApi::zeCommandListAppendMemoryRangesBarrier> &closure, const ClosureExternalStorage &storage) {

    auto params = createBaseParams(closure.apiArgs);
    params.emplace_back("numRanges", std::to_string(closure.indirectArgs.rangeSizes.size()));
    params.emplace_back("pRangeSizes", formatPointer(closure.indirectArgs.rangeSizes.data()));
    params.emplace_back("pRanges", formatPointer(static_cast<const void *>(closure.indirectArgs.ranges.data())));

    addCommonEventParameters(params, closure, storage);

    return params;
}

template <>
std::vector<std::pair<std::string, std::string>> extractParameters<CaptureApi::zeCommandListAppendMemoryCopyRegion>(
    const Closure<CaptureApi::zeCommandListAppendMemoryCopyRegion> &closure, const ClosureExternalStorage &storage) {

    auto params = createBaseParams(closure.apiArgs);
    params.emplace_back("dstptr", formatPointer(closure.apiArgs.dstptr));
    params.emplace_back("dstRegion", formatPointer(closure.apiArgs.dstRegion));
    params.emplace_back("dstPitch", std::to_string(closure.apiArgs.dstPitch));
    params.emplace_back("dstSlicePitch", std::to_string(closure.apiArgs.dstSlicePitch));
    params.emplace_back("srcptr", formatPointer(closure.apiArgs.srcptr));
    params.emplace_back("srcRegion", formatPointer(closure.apiArgs.srcRegion));
    params.emplace_back("srcPitch", std::to_string(closure.apiArgs.srcPitch));
    params.emplace_back("srcSlicePitch", std::to_string(closure.apiArgs.srcSlicePitch));

    addCommonEventParameters(params, closure, storage);

    return params;
}

template <>
std::vector<std::pair<std::string, std::string>> extractParameters<CaptureApi::zeCommandListAppendMemoryCopyFromContext>(
    const Closure<CaptureApi::zeCommandListAppendMemoryCopyFromContext> &closure, const ClosureExternalStorage &storage) {

    auto params = createBaseParams(closure.apiArgs);
    params.emplace_back("dstptr", formatPointer(closure.apiArgs.dstptr));
    params.emplace_back("hContextSrc", formatPointer(closure.apiArgs.hContextSrc));
    params.emplace_back("srcptr", formatPointer(closure.apiArgs.srcptr));
    params.emplace_back("size", std::to_string(closure.apiArgs.size));

    addCommonEventParameters(params, closure, storage);

    return params;
}

template <>
std::vector<std::pair<std::string, std::string>> extractParameters<CaptureApi::zeCommandListAppendImageCopy>(
    const Closure<CaptureApi::zeCommandListAppendImageCopy> &closure, const ClosureExternalStorage &storage) {

    auto params = createBaseParams(closure.apiArgs);
    params.emplace_back("hDstImage", formatPointer(closure.apiArgs.hDstImage));
    params.emplace_back("hSrcImage", formatPointer(closure.apiArgs.hSrcImage));

    addCommonEventParameters(params, closure, storage);

    return params;
}

template <>
std::vector<std::pair<std::string, std::string>> extractParameters<CaptureApi::zeCommandListAppendImageCopyRegion>(
    const Closure<CaptureApi::zeCommandListAppendImageCopyRegion> &closure, const ClosureExternalStorage &storage) {

    auto params = createBaseParams(closure.apiArgs);
    params.emplace_back("hDstImage", formatPointer(closure.apiArgs.hDstImage));
    params.emplace_back("hSrcImage", formatPointer(closure.apiArgs.hSrcImage));

    addOptionalRegionParameters(params, closure.apiArgs);
    addCommonEventParameters(params, closure, storage);

    return params;
}

template <>
std::vector<std::pair<std::string, std::string>> extractParameters<CaptureApi::zeCommandListAppendImageCopyToMemory>(
    const Closure<CaptureApi::zeCommandListAppendImageCopyToMemory> &closure, const ClosureExternalStorage &storage) {

    auto params = createBaseParams(closure.apiArgs);
    params.emplace_back("dstptr", formatPointer(closure.apiArgs.dstptr));
    params.emplace_back("hSrcImage", formatPointer(closure.apiArgs.hSrcImage));

    addOptionalRegionParameters(params, closure.apiArgs);
    addCommonEventParameters(params, closure, storage);

    return params;
}

template <>
std::vector<std::pair<std::string, std::string>> extractParameters<CaptureApi::zeCommandListAppendImageCopyFromMemory>(
    const Closure<CaptureApi::zeCommandListAppendImageCopyFromMemory> &closure, const ClosureExternalStorage &storage) {

    auto params = createBaseParams(closure.apiArgs);
    params.emplace_back("hDstImage", formatPointer(closure.apiArgs.hDstImage));
    params.emplace_back("srcptr", formatPointer(closure.apiArgs.srcptr));

    addOptionalRegionParameters(params, closure.apiArgs);
    addCommonEventParameters(params, closure, storage);

    return params;
}

template <>
std::vector<std::pair<std::string, std::string>> extractParameters<CaptureApi::zeCommandListAppendMemoryPrefetch>(
    const Closure<CaptureApi::zeCommandListAppendMemoryPrefetch> &closure, const ClosureExternalStorage &storage) {

    auto params = createBaseParams(closure.apiArgs);
    params.emplace_back("ptr", formatPointer(closure.apiArgs.ptr));
    params.emplace_back("size", std::to_string(closure.apiArgs.size));

    return params;
}

template <>
std::vector<std::pair<std::string, std::string>> extractParameters<CaptureApi::zeCommandListAppendMemAdvise>(
    const Closure<CaptureApi::zeCommandListAppendMemAdvise> &closure, const ClosureExternalStorage &storage) {

    auto params = createBaseParams(closure.apiArgs);
    params.emplace_back("hDevice", formatPointer(closure.apiArgs.hDevice));
    params.emplace_back("ptr", formatPointer(closure.apiArgs.ptr));
    params.emplace_back("size", std::to_string(closure.apiArgs.size));
    params.emplace_back("advice", std::to_string(static_cast<uint32_t>(closure.apiArgs.advice)));

    return params;
}

template <>
std::vector<std::pair<std::string, std::string>> extractParameters<CaptureApi::zeCommandListAppendSignalEvent>(
    const Closure<CaptureApi::zeCommandListAppendSignalEvent> &closure, const ClosureExternalStorage &storage) {

    auto params = createBaseParams(closure.apiArgs);
    params.emplace_back("hEvent", formatPointer(closure.apiArgs.hEvent));

    return params;
}

template <>
std::vector<std::pair<std::string, std::string>> extractParameters<CaptureApi::zeCommandListAppendEventReset>(
    const Closure<CaptureApi::zeCommandListAppendEventReset> &closure, const ClosureExternalStorage &storage) {

    auto params = createBaseParams(closure.apiArgs);
    params.emplace_back("hEvent", formatPointer(closure.apiArgs.hEvent));

    return params;
}

template <>
std::vector<std::pair<std::string, std::string>> extractParameters<CaptureApi::zeCommandListAppendQueryKernelTimestamps>(
    const Closure<CaptureApi::zeCommandListAppendQueryKernelTimestamps> &closure, const ClosureExternalStorage &storage) {

    auto params = createBaseParams(closure.apiArgs);
    params.emplace_back("dstptr", formatPointer(closure.apiArgs.dstptr));

    if (closure.apiArgs.pOffsets != nullptr) {
        params.emplace_back("pOffsets", formatPointer(closure.apiArgs.pOffsets));
    }

    addCommonEventParameters(params, closure, storage);

    return params;
}

template <>
std::vector<std::pair<std::string, std::string>> extractParameters<CaptureApi::zeCommandListAppendLaunchCooperativeKernel>(
    const Closure<CaptureApi::zeCommandListAppendLaunchCooperativeKernel> &closure, const ClosureExternalStorage &storage) {

    auto params = createBaseParams(closure.apiArgs);
    addKernelInformation(params, closure.apiArgs.kernelHandle);
    params.emplace_back("launchFuncArgs", formatGroupCount(closure.indirectArgs.launchKernelArgs));
    addCommonEventParameters(params, closure, storage);

    return params;
}

template <>
std::vector<std::pair<std::string, std::string>> extractParameters<CaptureApi::zeCommandListAppendLaunchKernelIndirect>(
    const Closure<CaptureApi::zeCommandListAppendLaunchKernelIndirect> &closure, const ClosureExternalStorage &storage) {

    auto params = createBaseParams(closure.apiArgs);
    addKernelInformation(params, closure.apiArgs.kernelHandle);
    params.emplace_back("pLaunchArgumentsBuffer", formatPointer(closure.apiArgs.launchArgsBuffer));
    addCommonEventParameters(params, closure, storage);

    return params;
}

template <>
std::vector<std::pair<std::string, std::string>> extractParameters<CaptureApi::zeCommandListAppendLaunchKernelWithParameters>(
    const Closure<CaptureApi::zeCommandListAppendLaunchKernelWithParameters> &closure, const ClosureExternalStorage &storage) {

    auto params = createBaseParams(closure.apiArgs);
    addKernelInformation(params, closure.apiArgs.kernelHandle);
    params.emplace_back("groupCounts", formatGroupCount(closure.indirectArgs.groupCounts));

    addLaunchKernelExtensionParameters(params, closure.indirectArgs.pNext);

    addCommonEventParameters(params, closure, storage);

    return params;
}

template <>
std::vector<std::pair<std::string, std::string>> extractParameters<CaptureApi::zeCommandListAppendLaunchKernelWithArguments>(
    const Closure<CaptureApi::zeCommandListAppendLaunchKernelWithArguments> &closure, const ClosureExternalStorage &storage) {
    auto params = createBaseParams(closure.apiArgs);
    addKernelInformation(params, closure.apiArgs.kernelHandle);
    params.emplace_back("groupCounts", formatGroupCount(closure.apiArgs.groupCounts));
    params.emplace_back("groupSizes", formatGroupSize(closure.apiArgs.groupSizes));

    if (closure.apiArgs.pArguments != nullptr) {
        params.emplace_back("pArguments", formatPointer(static_cast<const void *>(closure.apiArgs.pArguments)));
    } else {
        params.emplace_back("pArguments", "nullptr");
    }

    addLaunchKernelExtensionParameters(params, closure.indirectArgs.pNext);

    addCommonEventParameters(params, closure, storage);

    return params;
}

template <>
std::vector<std::pair<std::string, std::string>> extractParameters<CaptureApi::zeCommandListAppendLaunchMultipleKernelsIndirect>(
    const Closure<CaptureApi::zeCommandListAppendLaunchMultipleKernelsIndirect> &closure, const ClosureExternalStorage &storage) {

    auto params = createBaseParams(closure.apiArgs);
    params.emplace_back("numKernels", std::to_string(closure.apiArgs.numKernels));
    params.emplace_back("phKernels", formatPointer(static_cast<const void *>(closure.apiArgs.phKernels)));
    params.emplace_back("pCountBuffer", formatPointer(closure.apiArgs.pCountBuffer));
    params.emplace_back("pLaunchArgumentsBuffer", formatPointer(closure.apiArgs.launchArgsBuffer));

    addCommonEventParameters(params, closure, storage);

    return params;
}

template <>
std::vector<std::pair<std::string, std::string>> extractParameters<CaptureApi::zeCommandListAppendSignalExternalSemaphoreExt>(
    const Closure<CaptureApi::zeCommandListAppendSignalExternalSemaphoreExt> &closure, const ClosureExternalStorage &storage) {

    auto params = createBaseParams(closure.apiArgs);
    params.emplace_back("numSemaphores", std::to_string(closure.apiArgs.numSemaphores));
    params.emplace_back("phSemaphores", formatPointer(static_cast<const void *>(closure.apiArgs.phSemaphores)));
    for (uint32_t i = 0; i < closure.apiArgs.numSemaphores; ++i) {
        std::string semaphoreKey = "phSemaphores[" + std::to_string(i) + "]";
        params.emplace_back(semaphoreKey, formatPointer(closure.indirectArgs.semaphores[i]));
    }
    params.emplace_back("signalParams", formatPointer(&closure.indirectArgs.signalParams));

    addCommonEventParameters(params, closure, storage);

    return params;
}

template <>
std::vector<std::pair<std::string, std::string>> extractParameters<CaptureApi::zeCommandListAppendWaitExternalSemaphoreExt>(
    const Closure<CaptureApi::zeCommandListAppendWaitExternalSemaphoreExt> &closure, const ClosureExternalStorage &storage) {

    auto params = createBaseParams(closure.apiArgs);
    params.emplace_back("numSemaphores", std::to_string(closure.apiArgs.numSemaphores));
    params.emplace_back("phSemaphores", formatPointer(static_cast<const void *>(closure.apiArgs.phSemaphores)));
    for (uint32_t i = 0; i < closure.apiArgs.numSemaphores; ++i) {
        std::string semaphoreKey = "phSemaphores[" + std::to_string(i) + "]";
        params.emplace_back(semaphoreKey, formatPointer(closure.indirectArgs.semaphores[i]));
    }
    params.emplace_back("waitParams", formatPointer(&closure.indirectArgs.waitParams));

    addCommonEventParameters(params, closure, storage);

    return params;
}

template <>
std::vector<std::pair<std::string, std::string>> extractParameters<CaptureApi::zeCommandListAppendImageCopyToMemoryExt>(
    const Closure<CaptureApi::zeCommandListAppendImageCopyToMemoryExt> &closure, const ClosureExternalStorage &storage) {

    auto params = createBaseParams(closure.apiArgs);
    params.emplace_back("dstptr", formatPointer(closure.apiArgs.dstptr));
    params.emplace_back("hSrcImage", formatPointer(closure.apiArgs.hSrcImage));
    params.emplace_back("destRowPitch", std::to_string(closure.apiArgs.destRowPitch));
    params.emplace_back("destSlicePitch", std::to_string(closure.apiArgs.destSlicePitch));

    addOptionalRegionParameters(params, closure.apiArgs);
    addCommonEventParameters(params, closure, storage);

    return params;
}

template <>
std::vector<std::pair<std::string, std::string>> extractParameters<CaptureApi::zeCommandListAppendImageCopyFromMemoryExt>(
    const Closure<CaptureApi::zeCommandListAppendImageCopyFromMemoryExt> &closure, const ClosureExternalStorage &storage) {

    auto params = createBaseParams(closure.apiArgs);
    params.emplace_back("hDstImage", formatPointer(closure.apiArgs.hDstImage));
    params.emplace_back("srcptr", formatPointer(closure.apiArgs.srcptr));
    params.emplace_back("srcRowPitch", std::to_string(closure.apiArgs.srcRowPitch));
    params.emplace_back("srcSlicePitch", std::to_string(closure.apiArgs.srcSlicePitch));

    addOptionalRegionParameters(params, closure.apiArgs);
    addCommonEventParameters(params, closure, storage);

    return params;
}

template <>
std::vector<std::pair<std::string, std::string>> extractParameters<CaptureApi::zetCommandListAppendMetricStreamerMarker>(
    const Closure<CaptureApi::zetCommandListAppendMetricStreamerMarker> &closure, const ClosureExternalStorage &storage) {

    auto params = createBaseParams(closure.apiArgs);
    params.emplace_back("hMetricStreamer", formatPointer(closure.apiArgs.hMetricStreamer));
    params.emplace_back("value", std::to_string(closure.apiArgs.value));

    addCommonEventParameters(params, closure, storage);

    return params;
}

template <>
std::vector<std::pair<std::string, std::string>> extractParameters<CaptureApi::zetCommandListAppendMetricQueryBegin>(
    const Closure<CaptureApi::zetCommandListAppendMetricQueryBegin> &closure, const ClosureExternalStorage &storage) {

    auto params = createBaseParams(closure.apiArgs);
    params.emplace_back("hMetricQuery", formatPointer(closure.apiArgs.hMetricQuery));

    addCommonEventParameters(params, closure, storage);

    return params;
}

template <>
std::vector<std::pair<std::string, std::string>> extractParameters<CaptureApi::zetCommandListAppendMetricQueryEnd>(
    const Closure<CaptureApi::zetCommandListAppendMetricQueryEnd> &closure, const ClosureExternalStorage &storage) {

    auto params = createBaseParams(closure.apiArgs);
    params.emplace_back("hMetricQuery", formatPointer(closure.apiArgs.hMetricQuery));

    addCommonEventParameters(params, closure, storage);

    return params;
}

template <>
std::vector<std::pair<std::string, std::string>> extractParameters<CaptureApi::zetCommandListAppendMetricMemoryBarrier>(
    const Closure<CaptureApi::zetCommandListAppendMetricMemoryBarrier> &closure, const ClosureExternalStorage &storage) {

    auto params = createBaseParams(closure.apiArgs);

    addCommonEventParameters(params, closure, storage);

    return params;
}

template <>
std::vector<std::pair<std::string, std::string>> extractParameters<CaptureApi::zetCommandListAppendMarkerExp>(
    const Closure<CaptureApi::zetCommandListAppendMarkerExp> &closure, const ClosureExternalStorage &storage) {

    auto params = createBaseParams(closure.apiArgs);
    params.emplace_back("hMetricGroup", formatPointer(closure.apiArgs.hMetricGroup));
    params.emplace_back("value", std::to_string(closure.apiArgs.value));

    addCommonEventParameters(params, closure, storage);

    return params;
}

template <>
std::vector<std::pair<std::string, std::string>> extractParameters<CaptureApi::zeCommandListAppendMemoryCopyWithParameters>(
    const Closure<CaptureApi::zeCommandListAppendMemoryCopyWithParameters> &closure, const ClosureExternalStorage &storage) {

    auto params = createBaseParams(closure.apiArgs);
    params.emplace_back("dstptr", formatPointer(closure.apiArgs.dstptr));
    params.emplace_back("srcptr", formatPointer(closure.apiArgs.srcptr));
    params.emplace_back("size", std::to_string(closure.apiArgs.size));

    addMemoryTransferExtensionParameters(params, closure.indirectArgs.pNext);

    addCommonEventParameters(params, closure, storage);

    return params;
}

template <>
std::vector<std::pair<std::string, std::string>> extractParameters<CaptureApi::zeCommandListAppendMemoryFillWithParameters>(
    const Closure<CaptureApi::zeCommandListAppendMemoryFillWithParameters> &closure, const ClosureExternalStorage &storage) {

    auto params = createBaseParams(closure.apiArgs);
    params.emplace_back("ptr", formatPointer(closure.apiArgs.ptr));
    params.emplace_back("pattern", formatPointer(closure.indirectArgs.pattern.data()));
    params.emplace_back("patternSize", std::to_string(closure.indirectArgs.pattern.size()));
    params.emplace_back("size", std::to_string(closure.apiArgs.size));

    addMemoryTransferExtensionParameters(params, closure.indirectArgs.pNext);

    addCommonEventParameters(params, closure, storage);

    return params;
}

template <>
std::vector<std::pair<std::string, std::string>> extractParameters<CaptureApi::zexCommandListAppendMemoryCopyWithParameters>(
    const Closure<CaptureApi::zexCommandListAppendMemoryCopyWithParameters> &closure, const ClosureExternalStorage &storage) {

    auto params = createBaseParams(closure.apiArgs);
    params.emplace_back("dstptr", formatPointer(closure.apiArgs.dstptr));
    params.emplace_back("srcptr", formatPointer(closure.apiArgs.srcptr));
    params.emplace_back("size", std::to_string(closure.apiArgs.size));

    addMemoryTransferExtensionParameters(params, closure.indirectArgs.pNext);

    addCommonEventParameters(params, closure, storage);

    return params;
}

template <>
std::vector<std::pair<std::string, std::string>> extractParameters<CaptureApi::zexCommandListAppendMemoryFillWithParameters>(
    const Closure<CaptureApi::zexCommandListAppendMemoryFillWithParameters> &closure, const ClosureExternalStorage &storage) {

    auto params = createBaseParams(closure.apiArgs);
    params.emplace_back("ptr", formatPointer(closure.apiArgs.ptr));
    params.emplace_back("pattern", formatPointer(closure.indirectArgs.pattern.data()));
    params.emplace_back("patternSize", std::to_string(closure.indirectArgs.pattern.size()));
    params.emplace_back("size", std::to_string(closure.apiArgs.size));

    addMemoryTransferExtensionParameters(params, closure.indirectArgs.pNext);

    addCommonEventParameters(params, closure, storage);

    return params;
}

template <>
std::vector<std::pair<std::string, std::string>> extractParameters<CaptureApi::zeCommandListAppendHostFunction>(
    const Closure<CaptureApi::zeCommandListAppendHostFunction> &closure, const ClosureExternalStorage &storage) {

    auto params = createBaseParams(closure.apiArgs);
    params.emplace_back("pHostFunction", formatPointer(reinterpret_cast<const void *>(closure.apiArgs.pHostFunction)));
    params.emplace_back("pUserData", formatPointer(closure.apiArgs.pUserData));
    params.emplace_back("pNext", formatPointer(closure.apiArgs.pNext));

    addCommonEventParameters(params, closure, storage);

    return params;
}

template <>
std::vector<std::pair<std::string, std::string>> extractParameters<CaptureApi::zeCommandListGetNextCommandIdExp>(
    const Closure<CaptureApi::zeCommandListGetNextCommandIdExp> &closure, const ClosureExternalStorage &) {

    auto params = createBaseParams(closure.apiArgs);
    params.emplace_back("flags", std::to_string(closure.indirectArgs.desc.flags));
    params.emplace_back("commandId", std::to_string(closure.indirectArgs.commandId));

    return params;
}

template <>
std::vector<std::pair<std::string, std::string>> extractParameters<CaptureApi::zeCommandListGetNextCommandIdWithKernelsExp>(
    const Closure<CaptureApi::zeCommandListGetNextCommandIdWithKernelsExp> &closure, const ClosureExternalStorage &) {

    auto params = createBaseParams(closure.apiArgs);
    params.emplace_back("flags", std::to_string(closure.indirectArgs.desc.flags));
    params.emplace_back("numKernels", std::to_string(closure.indirectArgs.kernels.size()));
    params.emplace_back("commandId", std::to_string(closure.indirectArgs.commandId));

    return params;
}

template <>
std::vector<std::pair<std::string, std::string>> extractParameters<CaptureApi::NoopedCommandListFailedFunction>(
    const Closure<CaptureApi::NoopedCommandListFailedFunction> &closure, const ClosureExternalStorage &storage) {

    auto params = createBaseParams(closure.apiArgs);
    std::string failedFunctionName(closure.apiArgs.failedFunctionName);
    params.emplace_back("failed_function_name", failedFunctionName);

    std::ostringstream oss;
    oss << "0x" << std::hex << closure.indirectArgs.failureResult;
    std::string hexStr = oss.str();
    params.emplace_back("failed_function_return_value", hexStr);

    return params;
}

} // namespace GraphDumpHelper

} // namespace L0
