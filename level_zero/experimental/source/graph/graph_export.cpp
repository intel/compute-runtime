/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/experimental/source/graph/graph_export.h"

#include "shared/source/program/kernel_info.h"
#include "shared/source/utilities/io_functions.h"

#include "level_zero/core/source/cmdlist/cmdlist.h"
#include "level_zero/core/source/event/event.h"
#include "level_zero/core/source/kernel/kernel.h"
#include "level_zero/core/source/kernel/kernel_imp.h"
#include "level_zero/experimental/source/graph/graph.h"
#include "level_zero/ze_api.h"

#include "graph_export.inl"

#include <optional>
#include <sstream>

namespace L0 {

ze_result_t GraphDotExporter::exportToFile(const Graph &graph, const char *filePath) const {
    if (nullptr == filePath) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    FILE *file = NEO::IoFunctions::fopenPtr(filePath, "w");
    if (nullptr == file) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    std::string dotContent = exportToString(graph);
    size_t bytesWritten = NEO::IoFunctions::fwritePtr(dotContent.c_str(), 1, dotContent.size(), file);
    NEO::IoFunctions::fclosePtr(file);

    if (bytesWritten != dotContent.size()) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    return ZE_RESULT_SUCCESS;
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
    dot << "  nodesep=1;\n";
    dot << "  ranksep=1;\n";
    dot << "  node [shape=box, style=filled];\n";
    dot << "  edge [color=black];\n\n";
}

void GraphDotExporter::writeNodes(std::ostringstream &dot, const Graph &graph, uint32_t level, uint32_t subgraphId) const {
    const std::string indent(static_cast<size_t>(level + 1) * 2, ' ');
    dot << indent << "// Command nodes:\n";

    const auto &commands = graph.getCapturedCommands();
    for (CapturedCommandId cmdId = 0; cmdId < static_cast<uint32_t>(commands.size()); ++cmdId) {
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

    const auto &commands = graph.getCapturedCommands();
    dot << indent << "// Sequential edges:\n";

    for (CapturedCommandId cmdId = 1; cmdId < static_cast<uint32_t>(commands.size()); ++cmdId) {
        const std::string fromNode = generateNodeId(level, subgraphId, cmdId - 1);
        const std::string toNode = generateNodeId(level, subgraphId, cmdId);
        dot << indent << fromNode << " -> " << toNode << ";\n";
    }
}

void GraphDotExporter::writeForkJoinEdges(std::ostringstream &dot, const Graph &graph, uint32_t level, uint32_t subgraphId) const {
    const std::string indent(static_cast<size_t>(level + 1) * 2, ' ');

    const auto &joinedForks = graph.getJoinedForks();
    const auto &subGraphs = graph.getSubgraphs();

    dot << "\n"
        << indent << "// Fork/Join edges:\n";

    for (const auto &[forkCmdId, forkJoinInfo] : joinedForks) {
        const auto subgraphIndex = findSubgraphIndex(subGraphs, forkJoinInfo.forkDestiny);
        if (subgraphIndex && !forkJoinInfo.forkDestiny->getCapturedCommands().empty()) {
            const auto &subgraphCommands = forkJoinInfo.forkDestiny->getCapturedCommands();
            const std::string forkNode = generateNodeId(level, subgraphId, forkJoinInfo.forkCommandId);
            const std::string subgraphFirstNode = generateNodeId(level + 1, *subgraphIndex, 0);
            const std::string subgraphLastNode = generateNodeId(level + 1, *subgraphIndex, static_cast<uint32_t>(subgraphCommands.size()) - 1);
            const std::string joinNode = generateNodeId(level, subgraphId, forkJoinInfo.joinCommandId);

            dot << indent << forkNode << " -> " << subgraphFirstNode << ";\n";
            dot << indent << subgraphLastNode << " -> " << joinNode << ";\n";
        }
    }
}

void GraphDotExporter::writeUnjoinedForkEdges(std::ostringstream &dot, const Graph &graph, uint32_t level, uint32_t subgraphId) const {
    const std::string indent(static_cast<size_t>(level + 1) * 2, ' ');

    const auto &unjoinedForks = graph.getUnjoinedForks();
    const auto &subGraphs = graph.getSubgraphs();

    dot << "\n"
        << indent << "// Unjoined forks:\n";

    for (const auto &[cmdList, forkInfo] : unjoinedForks) {
        const auto subgraphIndex = findSubgraphIndexByCommandList(subGraphs, cmdList);
        if (subgraphIndex && !subGraphs[*subgraphIndex]->getCapturedCommands().empty()) {
            const std::string forkNode = generateNodeId(level, subgraphId, forkInfo.forkCommandId);
            const std::string subgraphFirstNode = generateNodeId(level + 1, *subgraphIndex, 0);
            dot << indent << forkNode << " -> " << subgraphFirstNode << " [color=red, label=\"unjoined fork\"];\n";
        }
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

void GraphDotExporter::writeSubgraphs(std::ostringstream &dot, const Graph &graph, uint32_t level) const {
    const auto &subGraphs = graph.getSubgraphs();
    if (subGraphs.empty()) {
        return;
    }

    const std::string indent(static_cast<size_t>(level + 1) * 2, ' ');
    dot << indent << "// Subgraphs:\n";

    for (uint32_t subgraphId = 0; subgraphId < static_cast<uint32_t>(subGraphs.size()); ++subgraphId) {
        const std::string clusterName = "cluster_" + generateSubgraphId(level + 1, subgraphId);

        dot << indent << "subgraph " << clusterName << " {\n";
        dot << indent << "  label=\"Subgraph " << (level + 1) << "-" << subgraphId << "\";\n";
        dot << indent << "  style=filled;\n";
        dot << indent << "  fillcolor=" << getSubgraphFillColor(level + 1) << ";\n\n";

        writeNodes(dot, *subGraphs[subgraphId], level + 1, subgraphId);
        writeEdges(dot, *subGraphs[subgraphId], level + 1, subgraphId);
        writeSubgraphs(dot, *subGraphs[subgraphId], level + 1);

        dot << indent << "  }\n\n";
    }
}

std::string GraphDotExporter::getCommandNodeLabel(const Graph &graph, CapturedCommandId cmdId, const std::string &indent) const {
    const auto &commands = graph.getCapturedCommands();
    const auto &cmd = commands[cmdId];

    std::string baseLabel;
    std::vector<std::pair<std::string, std::string>> params;

    switch (static_cast<CaptureApi>(cmd.index())) {
#define RR_CAPTURED_API(X)                                                                                                          \
    case CaptureApi::X:                                                                                                             \
        baseLabel = #X;                                                                                                             \
        params = GraphDumpHelper::extractParameters(std::get<static_cast<size_t>(CaptureApi::X)>(cmd), graph.getExternalStorage()); \
        break;

        RR_CAPTURED_APIS()
#undef RR_CAPTURED_API

    default:
        baseLabel = "Unknown";
        break;
    }

    if (params.empty()) {
        return "\"" + baseLabel + "\"";
    }

    std::ostringstream label;
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
    return label.str();
}

std::string GraphDotExporter::getCommandNodeAttributes(const Graph &graph, CapturedCommandId cmdId) const {
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

void addKernelInformation(std::vector<std::pair<std::string, std::string>> &params, const ze_kernel_handle_t kernelHandle) {
    auto *kernelImp = static_cast<L0::KernelImp *>(Kernel::fromHandle(kernelHandle));

    const auto &kernelImmutableData = kernelImp->getImmutableData();
    const auto &kernelName = kernelImmutableData->getKernelInfo()->kernelDescriptor.kernelMetadata.kernelName;
    const auto &kernelArgInfos = kernelImp->getKernelArgInfos();

    params.emplace_back("hKernel", formatPointer(kernelHandle));
    params.emplace_back("kernelName", kernelName);

    if (kernelArgInfos.size() > 0U) {
        kernelImp->populateMetadata();

        for (size_t i = 0; i < kernelImmutableData->getDescriptor().explicitArgsExtendedMetadata.size(); ++i) {
            const auto &arg = kernelImmutableData->getDescriptor().explicitArgsExtendedMetadata[i];
            params.emplace_back("arg[" + std::to_string(i) + "]", arg.type + " " + arg.argName);
        }
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
    // does not have closure specialization yet
    return {};
}

template <>
std::vector<std::pair<std::string, std::string>> extractParameters<CaptureApi::zeCommandListAppendLaunchKernelWithArguments>(
    const Closure<CaptureApi::zeCommandListAppendLaunchKernelWithArguments> &closure, const ClosureExternalStorage &storage) {
    // does not have closure specialization yet
    return {};
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

} // namespace GraphDumpHelper

} // namespace L0
