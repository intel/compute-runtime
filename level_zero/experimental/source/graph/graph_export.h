/*
 * Copyright (C) 2025 Intel Corporation
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

namespace L0 {

struct CommandList;

class GraphDotExporter {
  public:
    ze_result_t exportToFile(const Graph &graph, const char *filePath) const;

  protected:
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

    std::string getCommandNodeLabel(const Graph &graph, CapturedCommandId cmdId, const std::string &indent) const;
    std::string getCommandNodeAttributes(const Graph &graph, CapturedCommandId cmdId) const;
    std::string generateNodeId(uint32_t level, uint32_t subgraphId, CapturedCommandId cmdId) const;
    std::string generateSubgraphId(uint32_t level, uint32_t subgraphId) const;
    std::string getSubgraphFillColor(uint32_t level) const;
};

namespace GraphDumpHelper {

std::string formatPointer(const void *ptr);
std::string formatGroupCount(const ze_group_count_t &groupCount);
void addKernelInformation(std::vector<std::pair<std::string, std::string>> &params, const ze_kernel_handle_t kernelHandle);

} // namespace GraphDumpHelper

} // namespace L0
