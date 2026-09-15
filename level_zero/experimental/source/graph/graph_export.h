/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/ze_api.h"

#include "graph.h"

#include <optional>
#include <sstream>
#include <string>
#include <unordered_set>
#include <vector>

namespace L0 {

struct CommandList;

enum class GraphExportStyle : std::uint8_t {
    detailed,
    simple
};

enum class GraphExportEventNodes : std::uint8_t {
    hideInternal, // event operations used only for tracking dependencies between graph nodes are not dumped
    show          // all captured event operations are dumped as graph nodes
};

class GraphDotExporter {
  public:
    GraphDotExporter(GraphExportStyle style, GraphExportEventNodes eventNodes)
        : exportStyle(style), exportEventNodes(eventNodes) {}

    ze_result_t exportToFile(const Graph &graph, const char *filePath) const;

  protected:
    using InternalCommandsSet = std::unordered_set<CapturedCommandId>;

    std::string exportToString(const Graph &graph) const;

    void writeHeader(std::ostringstream &dot) const;
    void writeNodes(std::ostringstream &dot, const Graph &graph, uint32_t level, uint32_t subgraphId) const;
    void writeSubgraphs(std::ostringstream &dot, const Graph &graph, uint32_t level) const;
    void writeEdges(std::ostringstream &dot, const Graph &graph, uint32_t level, uint32_t subgraphId) const;
    void writeSequentialEdges(std::ostringstream &dot, const Graph &graph, uint32_t level, uint32_t subgraphId) const;
    void writeForkJoinEdges(std::ostringstream &dot, const Graph &graph, uint32_t level, uint32_t subgraphId) const;
    void writeUnjoinedForkEdges(std::ostringstream &dot, const Graph &graph, uint32_t level, uint32_t subgraphId) const;

    std::optional<uint32_t> findSubgraphIndex(std::span<Graph *const> subGraphs, const Graph *targetGraph) const;
    std::optional<uint32_t> findSubgraphIndexByCommandList(std::span<Graph *const> subGraphs, const L0::CommandList *cmdList) const;

    InternalCommandsSet collectInternalDependencyCommands(const Graph &graph) const;
    std::vector<CapturedCommandId> collectVisibleCommands(const Graph &graph) const;
    static std::optional<CapturedCommandId> findVisibleCommandAtOrBefore(std::span<const CapturedCommandId> visibleCommands, CapturedCommandId cmdId);
    static std::optional<CapturedCommandId> findVisibleCommandAtOrAfter(std::span<const CapturedCommandId> visibleCommands, CapturedCommandId cmdId);

    std::string getCommandNodeLabel(const Graph &graph, CapturedCommandId cmdId, const std::string_view indent) const;
    std::string getCommandNodeAttributes(const Graph &graph, CapturedCommandId cmdId) const;
    std::string generateNodeId(uint32_t level, uint32_t subgraphId, CapturedCommandId cmdId) const;
    std::string generateSubgraphId(uint32_t level, uint32_t subgraphId) const;
    std::string getSubgraphFillColor(uint32_t level) const;

  private:
    GraphExportStyle exportStyle;
    GraphExportEventNodes exportEventNodes;
};

std::string getGraphDumpDefaultFileName(const Graph &graph, const ExecutableGraph &executableGraph);

void dumpGraphOnInstantiate(const Graph &graph, const ExecutableGraph &executableGraph);

namespace GraphDumpHelper {

std::string formatPointer(const void *ptr);
std::string formatGroupCount(const ze_group_count_t &groupCount);
std::string formatGroupSize(const ze_group_size_t &groupSize);
std::string_view getKernelName(ze_kernel_handle_t kernelHandle);
std::string_view getKernelName(const CapturedCommand &cmd);
void addKernelInformation(std::vector<std::pair<std::string, std::string>> &params, ze_kernel_handle_t kernelHandle);
void addLaunchKernelExtensionParameters(std::vector<std::pair<std::string, std::string>> &params, const void *pNext);
void addMemoryTransferExtensionParameters(std::vector<std::pair<std::string, std::string>> &params, const void *pNext);

void addLaunchKernelAdditionalExtensionParameters(std::vector<std::pair<std::string, std::string>> &params, const ze_base_desc_t *baseDesc);
void addMemoryTransferAdditionalExtensionParameters(std::vector<std::pair<std::string, std::string>> &params, const ze_base_desc_t *baseDesc);

} // namespace GraphDumpHelper

} // namespace L0
