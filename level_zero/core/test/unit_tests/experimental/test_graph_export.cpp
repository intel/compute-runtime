/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/test/unit_tests/experimental/test_graph_export.h"

#include "shared/test/common/mocks/mock_io_functions.h"

#include "level_zero/core/test/unit_tests/fixtures/module_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_module.h"
#include "level_zero/experimental/source/graph/captured_apis/graph_captured_apis.h"
#include "level_zero/experimental/source/graph/graph_export.h"

#include "gtest/gtest.h"

using namespace NEO;

namespace L0 {
namespace ult {

TEST_F(GraphDotExporterTest, GivenNullFilePathWhenExportToFileThenReturnsInvalidArgument) {
    Graph testGraph{&ctx, true};
    auto result = exporter.exportToFile(testGraph, nullptr);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);
}

TEST_F(GraphDotExporterTest, GivenEmptyGraphWhenExportToStringThenContainsDigraphHeader) {
    Graph testGraph{&ctx, true};

    std::string dot = exporter.exportToString(testGraph);
    EXPECT_NE(dot.find("digraph \"graph\" {"), std::string::npos);
    EXPECT_NE(dot.find("rankdir=TB;"), std::string::npos);
    EXPECT_NE(dot.find("nodesep=1;"), std::string::npos);
    EXPECT_NE(dot.find("ranksep=1;"), std::string::npos);
    EXPECT_NE(dot.find("node [shape=box, style=filled];"), std::string::npos);
    EXPECT_NE(dot.find("edge [color=black];"), std::string::npos);
    EXPECT_NE(dot.find('}'), std::string::npos);
}

TEST_F(GraphDotExporterTest, GivenGraphWithSingleCommandWhenExportToStringThenContainsCommandNode) {
    Graph testGraph{&ctx, true};
    Mock<Event> event;
    Mock<CommandList> cmdlist;

    testGraph.capture<CaptureApi::zeCommandListAppendBarrier>(&cmdlist, &event, 0U, nullptr);
    testGraph.stopCapturing();

    std::string dot = exporter.exportToString(testGraph);
    EXPECT_NE(dot.find("zeCommandListAppendBarrier"), std::string::npos);
    EXPECT_NE(dot.find("L0_S0_C0"), std::string::npos);
}

TEST_F(GraphDotExporterTest, GivenGraphWithMultipleCommandsWhenExportToStringThenContainsSequentialEdges) {
    Graph testGraph{&ctx, true};
    Mock<Event> event;
    Mock<CommandList> cmdlist;

    testGraph.capture<CaptureApi::zeCommandListAppendBarrier>(&cmdlist, &event, 0U, nullptr);
    testGraph.capture<CaptureApi::zeCommandListAppendMemoryCopy>(&cmdlist, nullptr, nullptr, 0U, nullptr, 0U, nullptr);
    testGraph.stopCapturing();

    std::string dot = exporter.exportToString(testGraph);
    EXPECT_NE(dot.find("L0_S0_C0 -> L0_S0_C1"), std::string::npos);
}

TEST_F(GraphDotExporterTest, WhenWriteHeaderThenGeneratesValidDotHeader) {
    Graph testGraph{&ctx, true};

    std::ostringstream dot;
    exporter.writeHeader(dot);
    std::string header = dot.str();

    EXPECT_NE(header.find("digraph \"graph\" {"), std::string::npos);
    EXPECT_NE(header.find("rankdir=TB;"), std::string::npos);
    EXPECT_NE(header.find("nodesep=1;"), std::string::npos);
    EXPECT_NE(header.find("ranksep=1;"), std::string::npos);
    EXPECT_NE(header.find("node [shape=box, style=filled];"), std::string::npos);
    EXPECT_NE(header.find("edge [color=black];"), std::string::npos);
}

TEST_F(GraphDotExporterTest, GivenGraphWithCommandWhenWriteNodesThenGeneratesNodeDefinitions) {
    Graph testGraph{&ctx, true};
    Mock<Event> event;
    Mock<CommandList> cmdlist;

    testGraph.capture<CaptureApi::zeCommandListAppendBarrier>(&cmdlist, &event, 0U, nullptr);
    testGraph.stopCapturing();

    std::ostringstream dot;
    exporter.writeNodes(dot, testGraph, 0, 0);
    std::string output = dot.str();

    EXPECT_NE(output.find("L0_S0_C0"), std::string::npos);
    EXPECT_NE(output.find("zeCommandListAppendBarrier"), std::string::npos);
}

TEST_F(GraphDotExporterTest, GivenGraphWithMultipleCommandsWhenWriteEdgesThenGeneratesSequentialEdges) {
    Graph testGraph{&ctx, true};
    Mock<Event> event;
    Mock<CommandList> cmdlist;

    testGraph.capture<CaptureApi::zeCommandListAppendBarrier>(&cmdlist, &event, 0U, nullptr);
    testGraph.capture<CaptureApi::zeCommandListAppendMemoryCopy>(&cmdlist, nullptr, nullptr, 0U, nullptr, 0U, nullptr);
    testGraph.stopCapturing();

    std::ostringstream dot;
    exporter.writeEdges(dot, testGraph, 0, 0);
    std::string output = dot.str();

    EXPECT_NE(output.find("L0_S0_C0 -> L0_S0_C1"), std::string::npos);
}

TEST_F(GraphDotExporterTest, GivenGraphWithCommandWhenGetCommandNodeLabelThenReturnsCorrectLabel) {
    Graph testGraph{&ctx, true};
    Mock<Event> event;
    Mock<CommandList> cmdlist;

    testGraph.capture<CaptureApi::zeCommandListAppendBarrier>(&cmdlist, &event, 0U, nullptr);
    testGraph.stopCapturing();

    std::string label = exporter.getCommandNodeLabel(testGraph, 0, "");
    EXPECT_NE(label.find("zeCommandListAppendBarrier"), std::string::npos);
}

TEST_F(GraphDotExporterTest, GivenDifferentCommandTypesWhenGetCommandNodeAttributesThenReturnsCorrectColors) {
    Graph testGraph{&ctx, true};
    Mock<Event> event;
    Mock<CommandList> cmdlist;

    testGraph.capture<CaptureApi::zeCommandListAppendBarrier>(&cmdlist, &event, 0U, nullptr);
    testGraph.capture<CaptureApi::zeCommandListAppendMemoryCopy>(&cmdlist, nullptr, nullptr, 0U, nullptr, 0U, nullptr);
    testGraph.capture<CaptureApi::zeCommandListAppendSignalEvent>(&cmdlist, &event);
    testGraph.capture<CaptureApi::zeCommandListAppendImageCopy>(&cmdlist, nullptr, nullptr, nullptr, 0U, nullptr);
    testGraph.capture<CaptureApi::zeCommandListAppendWriteGlobalTimestamp>(&cmdlist, nullptr, nullptr, 0U, nullptr);

    testGraph.stopCapturing();

    EXPECT_EQ(exporter.getCommandNodeAttributes(testGraph, 0), ", fillcolor=orange");
    EXPECT_EQ(exporter.getCommandNodeAttributes(testGraph, 1), ", fillcolor=lightblue");
    EXPECT_EQ(exporter.getCommandNodeAttributes(testGraph, 2), ", fillcolor=yellow");
    EXPECT_EQ(exporter.getCommandNodeAttributes(testGraph, 3), ", fillcolor=lightgreen");
    EXPECT_EQ(exporter.getCommandNodeAttributes(testGraph, 4), ", fillcolor=pink");
}

TEST_F(GraphDotExporterTest, WhenGenerateNodeIdThenReturnsCorrectFormat) {
    EXPECT_EQ(exporter.generateNodeId(0, 0, 0), "L0_S0_C0");
    EXPECT_EQ(exporter.generateNodeId(1, 2, 3), "L1_S2_C3");
    EXPECT_EQ(exporter.generateNodeId(10, 20, 30), "L10_S20_C30");
}

TEST_F(GraphDotExporterTest, WhenGenerateSubgraphIdThenReturnsCorrectFormat) {
    EXPECT_EQ(exporter.generateSubgraphId(0, 0), "L0_S0");
    EXPECT_EQ(exporter.generateSubgraphId(1, 2), "L1_S2");
    EXPECT_EQ(exporter.generateSubgraphId(10, 20), "L10_S20");
}

TEST_F(GraphDotExporterTest, WhenGetSubgraphFillColorThenReturnsCorrectColors) {
    EXPECT_EQ(exporter.getSubgraphFillColor(1), "grey90");
    EXPECT_EQ(exporter.getSubgraphFillColor(2), "grey80");
    EXPECT_EQ(exporter.getSubgraphFillColor(3), "grey70");
    EXPECT_EQ(exporter.getSubgraphFillColor(4), "grey60");
    EXPECT_EQ(exporter.getSubgraphFillColor(5), "grey50");
}

TEST_F(GraphDotExporterTest, GivenDeepLevelWhenGetSubgraphFillColorThenReturnsDeepestColor) {
    EXPECT_EQ(exporter.getSubgraphFillColor(6), "grey50");
    EXPECT_EQ(exporter.getSubgraphFillColor(10), "grey50");
    EXPECT_EQ(exporter.getSubgraphFillColor(100), "grey50");
}

TEST_F(GraphDotExporterTest, GivenGraphWithoutSubgraphsWhenWriteSubgraphsThenGeneratesNoOutput) {
    Graph testGraph{&ctx, true};
    std::ostringstream dot;
    exporter.writeSubgraphs(dot, testGraph, 0);
    std::string output = dot.str();

    EXPECT_TRUE(output.empty());
}

TEST_F(GraphDotExporterTest, GivenGraphWithSubgraphsWhenWriteSubgraphsThenGeneratesSubgraphStructure) {
    Graph testGraph{&ctx, true};
    Mock<Event> forkEvent;
    Mock<Event> joinEvent;
    Mock<CommandList> mainCmdList;
    Mock<CommandList> subCmdList;

    Graph *testGraphPtr = &testGraph;
    captureCommand<CaptureApi::zeCommandListAppendBarrier>(mainCmdList, testGraphPtr, &mainCmdList, &forkEvent, 0U, nullptr);

    Graph *subGraph = nullptr;
    testGraph.forkTo(subCmdList, subGraph, forkEvent);
    ASSERT_NE(subGraph, nullptr);

    captureCommand<CaptureApi::zeCommandListAppendMemoryCopy>(subCmdList, subGraph, &subCmdList, nullptr, nullptr, 0U, nullptr, 0U, nullptr);
    captureCommand<CaptureApi::zeCommandListAppendBarrier>(subCmdList, subGraph, &subCmdList, &joinEvent, 0U, nullptr);

    testGraph.tryJoinOnNextCommand(subCmdList, joinEvent);
    captureCommand<CaptureApi::zeCommandListAppendBarrier>(mainCmdList, testGraphPtr, &mainCmdList, nullptr, 0U, nullptr);
    testGraph.stopCapturing();

    std::ostringstream dot;
    exporter.writeSubgraphs(dot, testGraph, 0);
    std::string output = dot.str();

    EXPECT_NE(output.find("// Subgraphs:"), std::string::npos);
    EXPECT_NE(output.find("subgraph cluster_L1_S0"), std::string::npos);
    EXPECT_NE(output.find("label=\"Subgraph 1-0\""), std::string::npos);
    EXPECT_NE(output.find("style=filled"), std::string::npos);
    EXPECT_NE(output.find("fillcolor=grey90"), std::string::npos);
}

TEST_F(GraphDotExporterTest, GivenGraphWithNestedSubgraphsWhenWriteSubgraphsThenGeneratesNestedStructure) {
    Graph testGraph{&ctx, true};
    Mock<Event> forkEvent1;
    Mock<Event> forkEvent2;
    Mock<Event> joinEvent1;
    Mock<Event> joinEvent2;
    Mock<CommandList> mainCmdList;
    Mock<CommandList> subCmdList1;
    Mock<CommandList> subCmdList2;

    Graph *testGraphPtr = &testGraph;
    captureCommand<CaptureApi::zeCommandListAppendBarrier>(mainCmdList, testGraphPtr, &mainCmdList, &forkEvent1, 0U, nullptr);

    Graph *subGraph1 = nullptr;
    testGraph.forkTo(subCmdList1, subGraph1, forkEvent1);
    ASSERT_NE(subGraph1, nullptr);

    captureCommand<CaptureApi::zeCommandListAppendMemoryCopy>(subCmdList1, subGraph1, &subCmdList1, nullptr, nullptr, 0U, nullptr, 0U, nullptr);

    Graph *subGraph2 = nullptr;
    captureCommand<CaptureApi::zeCommandListAppendBarrier>(subCmdList1, subGraph1, &subCmdList1, &forkEvent2, 0U, nullptr);
    subGraph1->forkTo(subCmdList2, subGraph2, forkEvent2);
    ASSERT_NE(subGraph2, nullptr);

    captureCommand<CaptureApi::zeCommandListAppendBarrier>(subCmdList2, subGraph2, &subCmdList2, &joinEvent2, 0U, nullptr);

    subGraph1->tryJoinOnNextCommand(subCmdList2, joinEvent2);
    captureCommand<CaptureApi::zeCommandListAppendBarrier>(subCmdList1, subGraph1, &subCmdList1, &joinEvent1, 0U, nullptr);

    testGraph.tryJoinOnNextCommand(subCmdList1, joinEvent1);
    testGraph.stopCapturing();

    std::ostringstream dot;
    exporter.writeSubgraphs(dot, testGraph, 0);
    std::string output = dot.str();

    EXPECT_NE(output.find("subgraph cluster_L1_S0"), std::string::npos);
    EXPECT_NE(output.find("subgraph cluster_L2_S0"), std::string::npos);
    EXPECT_NE(output.find("fillcolor=grey90"), std::string::npos);
    EXPECT_NE(output.find("fillcolor=grey80"), std::string::npos);
}

TEST_F(GraphDotExporterTest, GivenGraphWithAdjacentSubgraphsWhenWriteSubgraphsThenGeneratesMultipleSubgraphs) {
    Graph testGraph{&ctx, true};
    Mock<Event> forkEvent1;
    Mock<Event> forkEvent2;
    Mock<Event> joinEvent1;
    Mock<Event> joinEvent2;
    Mock<CommandList> mainCmdList;
    Mock<CommandList> subCmdList1;
    Mock<CommandList> subCmdList2;

    Graph *testGraphPtr = &testGraph;
    captureCommand<CaptureApi::zeCommandListAppendBarrier>(mainCmdList, testGraphPtr, &mainCmdList, &forkEvent1, 0U, nullptr);

    Graph *subGraph1 = nullptr;
    testGraph.forkTo(subCmdList1, subGraph1, forkEvent1);
    ASSERT_NE(subGraph1, nullptr);

    captureCommand<CaptureApi::zeCommandListAppendMemoryCopy>(subCmdList1, subGraph1, &subCmdList1, nullptr, nullptr, 0U, nullptr, 0U, nullptr);
    captureCommand<CaptureApi::zeCommandListAppendBarrier>(subCmdList1, subGraph1, &subCmdList1, &joinEvent1, 0U, nullptr);

    testGraph.tryJoinOnNextCommand(subCmdList1, joinEvent1);
    captureCommand<CaptureApi::zeCommandListAppendBarrier>(mainCmdList, testGraphPtr, &mainCmdList, &forkEvent2, 0U, nullptr);

    Graph *subGraph2 = nullptr;
    testGraph.forkTo(subCmdList2, subGraph2, forkEvent2);
    ASSERT_NE(subGraph2, nullptr);

    captureCommand<CaptureApi::zeCommandListAppendMemoryCopy>(subCmdList2, subGraph2, &subCmdList2, nullptr, nullptr, 0U, nullptr, 0U, nullptr);
    captureCommand<CaptureApi::zeCommandListAppendBarrier>(subCmdList2, subGraph2, &subCmdList2, &joinEvent2, 0U, nullptr);

    testGraph.tryJoinOnNextCommand(subCmdList2, joinEvent2);
    captureCommand<CaptureApi::zeCommandListAppendBarrier>(mainCmdList, testGraphPtr, &mainCmdList, nullptr, 0U, nullptr);
    testGraph.stopCapturing();

    std::ostringstream dot;
    exporter.writeSubgraphs(dot, testGraph, 0);
    std::string output = dot.str();

    EXPECT_NE(output.find("// Subgraphs:"), std::string::npos);
    EXPECT_NE(output.find("subgraph cluster_L1_S0"), std::string::npos);
    EXPECT_NE(output.find("subgraph cluster_L1_S1"), std::string::npos);
    EXPECT_NE(output.find("label=\"Subgraph 1-0\""), std::string::npos);
    EXPECT_NE(output.find("label=\"Subgraph 1-1\""), std::string::npos);
}

TEST_F(GraphDotExporterTest, WhenFindSubgraphIndexOnEmptyGraphThenReturnsNullopt) {
    const StackVec<Graph *, 16> subGraphs;
    Graph fakeSubgraph{&ctx, true};
    auto index = exporter.findSubgraphIndex(subGraphs, &fakeSubgraph);

    EXPECT_FALSE(index.has_value());
}

TEST_F(GraphDotExporterTest, WhenFindSubgraphIndexWithInvalidSubgraphThenReturnsNullopt) {
    Graph testGraph{&ctx, true};
    Mock<Event> forkEvent;
    Mock<Event> joinEvent;
    Mock<CommandList> mainCmdList;
    Mock<CommandList> subCmdList;

    Graph *testGraphPtr = &testGraph;
    captureCommand<CaptureApi::zeCommandListAppendBarrier>(mainCmdList, testGraphPtr, &mainCmdList, &forkEvent, 0U, nullptr);

    Graph *subGraph = nullptr;
    testGraph.forkTo(subCmdList, subGraph, forkEvent);
    ASSERT_NE(subGraph, nullptr);

    testGraph.tryJoinOnNextCommand(subCmdList, joinEvent);
    captureCommand<CaptureApi::zeCommandListAppendBarrier>(mainCmdList, testGraphPtr, &mainCmdList, nullptr, 0U, nullptr);
    testGraph.stopCapturing();

    const auto &subGraphs = testGraph.getSubgraphs();
    Graph fakeSubgraph{&ctx, true};
    auto index = exporter.findSubgraphIndex(subGraphs, &fakeSubgraph);

    EXPECT_FALSE(index.has_value());
}

TEST_F(GraphDotExporterTest, WhenFindSubgraphIndexWithValidGraphThenReturnsCorrectIndex) {
    Graph testGraph{&ctx, true};
    Mock<Event> forkEvent;
    Mock<Event> joinEvent;
    Mock<CommandList> mainCmdList;
    Mock<CommandList> subCmdList;

    Graph *testGraphPtr = &testGraph;
    captureCommand<CaptureApi::zeCommandListAppendBarrier>(mainCmdList, testGraphPtr, &mainCmdList, &forkEvent, 0U, nullptr);

    Graph *subGraph = nullptr;
    testGraph.forkTo(subCmdList, subGraph, forkEvent);
    ASSERT_NE(subGraph, nullptr);

    testGraph.tryJoinOnNextCommand(subCmdList, joinEvent);
    captureCommand<CaptureApi::zeCommandListAppendBarrier>(mainCmdList, testGraphPtr, &mainCmdList, nullptr, 0U, nullptr);
    testGraph.stopCapturing();

    const auto &subGraphs = testGraph.getSubgraphs();
    auto index = exporter.findSubgraphIndex(subGraphs, subGraph);

    ASSERT_TRUE(index.has_value());
    EXPECT_EQ(index.value(), 0U);
}

TEST_F(GraphDotExporterTest, WhenFindSubgraphIndexByCommandListOnEmptyGraphThenReturnsNullopt) {
    const StackVec<Graph *, 16> subGraphs;
    Mock<CommandList> fakeCmdList;
    auto index = exporter.findSubgraphIndexByCommandList(subGraphs, &fakeCmdList);

    EXPECT_FALSE(index.has_value());
}

TEST_F(GraphDotExporterTest, WhenFindSubgraphIndexByCommandListWithInvalidCommandListThenReturnsNullopt) {
    Graph testGraph{&ctx, true};
    Mock<Event> forkEvent;
    Mock<Event> joinEvent;
    Mock<CommandList> mainCmdList;
    Mock<CommandList> subCmdList;

    Graph *testGraphPtr = &testGraph;
    captureCommand<CaptureApi::zeCommandListAppendBarrier>(mainCmdList, testGraphPtr, &mainCmdList, &forkEvent, 0U, nullptr);

    Graph *subGraph = nullptr;
    testGraph.forkTo(subCmdList, subGraph, forkEvent);
    ASSERT_NE(subGraph, nullptr);

    testGraph.tryJoinOnNextCommand(subCmdList, joinEvent);
    captureCommand<CaptureApi::zeCommandListAppendBarrier>(mainCmdList, testGraphPtr, &mainCmdList, nullptr, 0U, nullptr);
    testGraph.stopCapturing();

    const auto &subGraphs = testGraph.getSubgraphs();
    Mock<CommandList> fakeCmdList;
    auto index = exporter.findSubgraphIndexByCommandList(subGraphs, &fakeCmdList);

    EXPECT_FALSE(index.has_value());
}

TEST_F(GraphDotExporterTest, WhenFindSubgraphIndexByCommandListWithValidCommandListThenReturnsCorrectIndex) {
    Graph testGraph{&ctx, true};
    Mock<Event> forkEvent;
    Mock<Event> joinEvent;
    Mock<CommandList> mainCmdList;
    Mock<CommandList> subCmdList;

    Graph *testGraphPtr = &testGraph;
    captureCommand<CaptureApi::zeCommandListAppendBarrier>(mainCmdList, testGraphPtr, &mainCmdList, &forkEvent, 0U, nullptr);

    Graph *subGraph = nullptr;
    testGraph.forkTo(subCmdList, subGraph, forkEvent);
    ASSERT_NE(subGraph, nullptr);

    testGraph.tryJoinOnNextCommand(subCmdList, joinEvent);
    captureCommand<CaptureApi::zeCommandListAppendBarrier>(mainCmdList, testGraphPtr, &mainCmdList, nullptr, 0U, nullptr);
    testGraph.stopCapturing();

    const auto &subGraphs = testGraph.getSubgraphs();
    auto index = exporter.findSubgraphIndexByCommandList(subGraphs, &subCmdList);

    ASSERT_TRUE(index.has_value());
    EXPECT_EQ(index.value(), 0U);
}

TEST_F(GraphDotExporterTest, GivenGraphWithEmptySubgraphWhenWriteForkJoinEdgesThenNoEdges) {
    Graph testGraph{&ctx, true};
    Mock<Event> forkEvent;
    Mock<Event> joinEvent;
    Mock<CommandList> mainCmdList;
    Mock<CommandList> subCmdList;

    Graph *testGraphPtr = &testGraph;
    captureCommand<CaptureApi::zeCommandListAppendBarrier>(mainCmdList, testGraphPtr, &mainCmdList, &forkEvent, 0U, nullptr);

    Graph *subGraph = nullptr;
    testGraph.forkTo(subCmdList, subGraph, forkEvent);
    ASSERT_NE(subGraph, nullptr);

    testGraph.tryJoinOnNextCommand(subCmdList, joinEvent);
    captureCommand<CaptureApi::zeCommandListAppendBarrier>(mainCmdList, testGraphPtr, &mainCmdList, nullptr, 0U, nullptr);
    testGraph.stopCapturing();

    std::ostringstream dot;
    exporter.writeForkJoinEdges(dot, testGraph, 0, 0);
    std::string output = dot.str();

    EXPECT_EQ(output.find("->"), std::string::npos);
}

TEST_F(GraphDotExporterTest, GivenGraphWithUnjoinedForksWhenWriteUnjoinedForkEdgesThenGeneratesUnjoinedEdges) {
    Graph testGraph{&ctx, true};
    Mock<Event> forkEvent;
    Mock<CommandList> mainCmdList;
    Mock<CommandList> subCmdList;

    Graph *testGraphPtr = &testGraph;
    captureCommand<CaptureApi::zeCommandListAppendBarrier>(mainCmdList, testGraphPtr, &mainCmdList, &forkEvent, 0U, nullptr);

    Graph *subGraph = nullptr;
    testGraph.forkTo(subCmdList, subGraph, forkEvent);
    ASSERT_NE(subGraph, nullptr);

    captureCommand<CaptureApi::zeCommandListAppendMemoryCopy>(subCmdList, subGraph, &subCmdList, nullptr, nullptr, 0U, nullptr, 0U, nullptr);

    std::ostringstream dot;
    exporter.writeUnjoinedForkEdges(dot, testGraph, 0, 0);
    std::string output = dot.str();

    EXPECT_NE(output.find("// Unjoined forks:"), std::string::npos);
    EXPECT_NE(output.find("L0_S0_C0 -> L1_S0_C0 [color=red, label=\"unjoined fork\"];"), std::string::npos);

    // Prevent double free with unjoined forks
    Mock<Event> joinEvent;
    testGraph.tryJoinOnNextCommand(subCmdList, joinEvent);
    captureCommand<CaptureApi::zeCommandListAppendBarrier>(mainCmdList, testGraphPtr, &mainCmdList, &forkEvent, 0U, nullptr);
    testGraph.stopCapturing();
}

TEST_F(GraphDotExporterTest, GivenGraphWithEmptyUnjoinedSubgraphWhenWriteUnjoinedForkEdgesThenOutputIsEmpty) {
    Graph testGraph{&ctx, true};
    Mock<Event> forkEvent;
    Mock<CommandList> mainCmdList;
    Mock<CommandList> subCmdList;

    Graph *testGraphPtr = &testGraph;
    captureCommand<CaptureApi::zeCommandListAppendBarrier>(mainCmdList, testGraphPtr, &mainCmdList, &forkEvent, 0U, nullptr);

    Graph *subGraph = nullptr;
    testGraph.forkTo(subCmdList, subGraph, forkEvent);
    ASSERT_NE(subGraph, nullptr);

    std::ostringstream dot;
    exporter.writeUnjoinedForkEdges(dot, testGraph, 0, 0);
    std::string output = dot.str();

    EXPECT_EQ(output.find("L0_S0_C0 -> L1_S0_C0 [color=red, label=\"unjoined fork\"];"), std::string::npos);

    // Prevent double free with unjoined forks
    Mock<Event> joinEvent;
    testGraph.tryJoinOnNextCommand(subCmdList, joinEvent);
    captureCommand<CaptureApi::zeCommandListAppendBarrier>(mainCmdList, testGraphPtr, &mainCmdList, &forkEvent, 0U, nullptr);
    testGraph.stopCapturing();
}

TEST_F(GraphDotExporterFileTest, GivenEmptyGraphWhenExportToFileThenWritesValidDotContent) {
    Graph testGraph{&ctx, true};
    setupSuccessfulWrite(testGraph);

    auto result = exporter.exportToFile(testGraph, testFilePath.c_str());
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    std::string writtenContent = getWrittenContent();
    EXPECT_NE(writtenContent.find("digraph \"graph\" {"), std::string::npos);
    EXPECT_NE(writtenContent.find('}'), std::string::npos);

    EXPECT_EQ(mockFopenCalledBefore + 1, NEO::IoFunctions::mockFopenCalled);
    EXPECT_EQ(mockFwriteCalledBefore + 1, NEO::IoFunctions::mockFwriteCalled);
    EXPECT_EQ(mockFcloseCalledBefore + 1, NEO::IoFunctions::mockFcloseCalled);
}

TEST_F(GraphDotExporterFileTest, GivenFailedFileOpenWhenExportToFileThenReturnsUnknownError) {
    Graph testGraph{&ctx, true};
    setupFailedOpen();

    auto result = exporter.exportToFile(testGraph, testFilePath.c_str());
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, result);

    EXPECT_EQ(mockFopenCalledBefore + 1, NEO::IoFunctions::mockFopenCalled);
    EXPECT_EQ(mockFwriteCalledBefore, NEO::IoFunctions::mockFwriteCalled);
}

TEST_F(GraphDotExporterFileTest, GivenFailedFileWriteWhenExportToFileThenReturnsUnknownError) {
    Graph testGraph{&ctx, true};
    setupFailedWrite();

    auto result = exporter.exportToFile(testGraph, testFilePath.c_str());
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, result);

    EXPECT_EQ(mockFopenCalledBefore + 1, NEO::IoFunctions::mockFopenCalled);
    EXPECT_EQ(mockFwriteCalledBefore + 1, NEO::IoFunctions::mockFwriteCalled);
    EXPECT_EQ(mockFcloseCalledBefore + 1, NEO::IoFunctions::mockFcloseCalled);
}

TEST(GraphDumpHelperTest, GivenNullptrAndPtrWhenFormatPointerIsCalledThenReturnsFormattedString) {
    EXPECT_EQ(GraphDumpHelper::formatPointer(nullptr), "0x0");
    const void *ptr = reinterpret_cast<void *>(0x1234);
    EXPECT_EQ(GraphDumpHelper::formatPointer(ptr), "0x1234");
}

TEST(GraphDumpHelperTest, GivenGroupCountWhenFormatGroupCountIsCalledThenReturnsFormattedString) {
    ze_group_count_t groupCount{1, 2, 3};
    EXPECT_EQ(GraphDumpHelper::formatGroupCount(groupCount), "1x2x3");
}

DEFINE_APIARGS_FIELDS(zeCommandListAppendWriteGlobalTimestamp, "hCommandList", "dstptr", "hSignalEvent", "numWaitEvents", "phWaitEvents", "phWaitEvents[0]");
DEFINE_APIARGS_FIELDS(zeCommandListAppendBarrier, "hCommandList", "hSignalEvent", "numWaitEvents", "phWaitEvents", "phWaitEvents[0]");
DEFINE_APIARGS_FIELDS(zeCommandListAppendMemoryRangesBarrier, "hCommandList", "numRanges", "pRangeSizes", "pRanges", "hSignalEvent", "numWaitEvents", "phWaitEvents", "phWaitEvents[0]");
DEFINE_APIARGS_FIELDS(zeCommandListAppendMemoryCopy, "hCommandList", "dstptr", "srcptr", "size", "hSignalEvent", "numWaitEvents", "phWaitEvents", "phWaitEvents[0]");
DEFINE_APIARGS_FIELDS(zeCommandListAppendMemoryFill, "hCommandList", "ptr", "pattern", "patternSize", "size", "hSignalEvent", "numWaitEvents", "phWaitEvents", "phWaitEvents[0]");
DEFINE_APIARGS_FIELDS(zeCommandListAppendMemoryCopyRegion, "hCommandList", "dstptr", "dstRegion", "dstPitch", "dstSlicePitch", "srcptr", "srcRegion", "srcPitch", "srcSlicePitch", "hSignalEvent", "numWaitEvents", "phWaitEvents", "phWaitEvents[0]");
DEFINE_APIARGS_FIELDS(zeCommandListAppendMemoryCopyFromContext, "hCommandList", "dstptr", "hContextSrc", "srcptr", "size", "hSignalEvent", "numWaitEvents", "phWaitEvents", "phWaitEvents[0]");
DEFINE_APIARGS_FIELDS(zeCommandListAppendImageCopy, "hCommandList", "hDstImage", "hSrcImage", "hSignalEvent", "numWaitEvents", "phWaitEvents", "phWaitEvents[0]");
DEFINE_APIARGS_FIELDS(zeCommandListAppendImageCopyRegion, "hCommandList", "hDstImage", "hSrcImage", "pDstRegion", "pSrcRegion", "hSignalEvent", "numWaitEvents", "phWaitEvents", "phWaitEvents[0]");
DEFINE_APIARGS_FIELDS(zeCommandListAppendImageCopyToMemory, "hCommandList", "dstptr", "hSrcImage", "pSrcRegion", "hSignalEvent", "numWaitEvents", "phWaitEvents", "phWaitEvents[0]");
DEFINE_APIARGS_FIELDS(zeCommandListAppendImageCopyFromMemory, "hCommandList", "hDstImage", "srcptr", "pDstRegion", "hSignalEvent", "numWaitEvents", "phWaitEvents", "phWaitEvents[0]");
DEFINE_APIARGS_FIELDS(zeCommandListAppendMemoryPrefetch, "hCommandList", "ptr", "size");
DEFINE_APIARGS_FIELDS(zeCommandListAppendMemAdvise, "hCommandList", "hDevice", "ptr", "size", "advice");
DEFINE_APIARGS_FIELDS(zeCommandListAppendSignalEvent, "hCommandList", "hEvent");
DEFINE_APIARGS_FIELDS(zeCommandListAppendWaitOnEvents, "hCommandList", "numEvents", "phEvents", "phEvents[0]");
DEFINE_APIARGS_FIELDS(zeCommandListAppendEventReset, "hCommandList", "hEvent");
DEFINE_APIARGS_FIELDS(zeCommandListAppendQueryKernelTimestamps, "hCommandList", "numEvents", "phEvents", "phEvents[0]", "dstptr", "pOffsets", "hSignalEvent", "numWaitEvents", "phWaitEvents", "phWaitEvents[0]");
DEFINE_APIARGS_FIELDS(zeCommandListAppendLaunchKernel, "hCommandList", "hKernel", "kernelName", "launchFuncArgs", "hSignalEvent", "numWaitEvents", "phWaitEvents", "phWaitEvents[0]");
DEFINE_APIARGS_FIELDS(zeCommandListAppendLaunchCooperativeKernel, "hCommandList", "hKernel", "kernelName", "launchFuncArgs", "hSignalEvent", "numWaitEvents", "phWaitEvents", "phWaitEvents[0]");
DEFINE_APIARGS_FIELDS(zeCommandListAppendLaunchKernelIndirect, "hCommandList", "hKernel", "kernelName", "pLaunchArgumentsBuffer", "hSignalEvent", "numWaitEvents", "phWaitEvents", "phWaitEvents[0]");
DEFINE_APIARGS_FIELDS(zeCommandListAppendLaunchKernelWithParameters, "hCommandList", "hKernel", "kernelName", "pGroupCounts", "pNext", "hSignalEvent", "numWaitEvents", "phWaitEvents", "phWaitEvents[0]");
DEFINE_APIARGS_FIELDS(zeCommandListAppendLaunchMultipleKernelsIndirect, "hCommandList", "numKernels", "phKernels", "pCountBuffer", "pLaunchArgumentsBuffer", "hSignalEvent", "numWaitEvents", "phWaitEvents", "phWaitEvents[0]");
DEFINE_APIARGS_FIELDS(zeCommandListAppendSignalExternalSemaphoreExt, "hCommandList", "numSemaphores", "phSemaphores", "phSemaphores[0]", "signalParams", "hSignalEvent", "numWaitEvents", "phWaitEvents", "phWaitEvents[0]");
DEFINE_APIARGS_FIELDS(zeCommandListAppendWaitExternalSemaphoreExt, "hCommandList", "numSemaphores", "phSemaphores", "phSemaphores[0]", "waitParams", "hSignalEvent", "numWaitEvents", "phWaitEvents", "phWaitEvents[0]");
DEFINE_APIARGS_FIELDS(zeCommandListAppendImageCopyToMemoryExt, "hCommandList", "dstptr", "hSrcImage", "pSrcRegion", "destRowPitch", "destSlicePitch", "hSignalEvent", "numWaitEvents", "phWaitEvents", "phWaitEvents[0]");
DEFINE_APIARGS_FIELDS(zeCommandListAppendImageCopyFromMemoryExt, "hCommandList", "hDstImage", "srcptr", "pDstRegion", "srcRowPitch", "srcSlicePitch", "hSignalEvent", "numWaitEvents", "phWaitEvents", "phWaitEvents[0]");

TEST_F(ExtractParametersTest, zeCommandListAppendWriteGlobalTimestamp) {
    Closure<CaptureApi::zeCommandListAppendWriteGlobalTimestamp>::ApiArgs args{};
    args.numWaitEvents = 1;
    args.phWaitEvents = dummyEvents;
    args.hSignalEvent = dummyEvents[0];
    expectAllApiArgsPresent<CaptureApi::zeCommandListAppendWriteGlobalTimestamp>(args);
}

TEST_F(ExtractParametersTest, zeCommandListAppendBarrier) {
    Closure<CaptureApi::zeCommandListAppendBarrier>::ApiArgs args{};
    args.numWaitEvents = 1;
    args.phWaitEvents = dummyEvents;
    args.hSignalEvent = dummyEvents[0];
    expectAllApiArgsPresent<CaptureApi::zeCommandListAppendBarrier>(args);
}

TEST_F(ExtractParametersTest, zeCommandListAppendMemoryRangesBarrier) {
    Closure<CaptureApi::zeCommandListAppendMemoryRangesBarrier>::ApiArgs args{};
    args.numWaitEvents = 1;
    args.phWaitEvents = dummyEvents;
    args.hSignalEvent = dummyEvents[0];
    args.numRanges = 1;
    args.pRangeSizes = dummyRangeSizes;
    args.pRanges = dummyRanges;
    expectAllApiArgsPresent<CaptureApi::zeCommandListAppendMemoryRangesBarrier>(args);
}

TEST_F(ExtractParametersTest, zeCommandListAppendMemoryCopy) {
    Closure<CaptureApi::zeCommandListAppendMemoryCopy>::ApiArgs args{};
    args.numWaitEvents = 1;
    args.phWaitEvents = dummyEvents;
    args.hSignalEvent = dummyEvents[0];
    expectAllApiArgsPresent<CaptureApi::zeCommandListAppendMemoryCopy>(args);
}

TEST_F(ExtractParametersTest, zeCommandListAppendMemoryFill) {
    Closure<CaptureApi::zeCommandListAppendMemoryFill>::ApiArgs args{};
    args.numWaitEvents = 1;
    args.phWaitEvents = dummyEvents;
    args.hSignalEvent = dummyEvents[0];
    expectAllApiArgsPresent<CaptureApi::zeCommandListAppendMemoryFill>(args);
}

TEST_F(ExtractParametersTest, zeCommandListAppendMemoryCopyRegion) {
    Closure<CaptureApi::zeCommandListAppendMemoryCopyRegion>::ApiArgs args{};
    args.numWaitEvents = 1;
    args.phWaitEvents = dummyEvents;
    args.hSignalEvent = dummyEvents[0];
    expectAllApiArgsPresent<CaptureApi::zeCommandListAppendMemoryCopyRegion>(args);
}

TEST_F(ExtractParametersTest, zeCommandListAppendMemoryCopyFromContext) {
    Closure<CaptureApi::zeCommandListAppendMemoryCopyFromContext>::ApiArgs args{};
    args.numWaitEvents = 1;
    args.phWaitEvents = dummyEvents;
    args.hSignalEvent = dummyEvents[0];
    expectAllApiArgsPresent<CaptureApi::zeCommandListAppendMemoryCopyFromContext>(args);
}

TEST_F(ExtractParametersTest, zeCommandListAppendImageCopy) {
    Closure<CaptureApi::zeCommandListAppendImageCopy>::ApiArgs args{};
    args.numWaitEvents = 1;
    args.phWaitEvents = dummyEvents;
    args.hSignalEvent = dummyEvents[0];
    expectAllApiArgsPresent<CaptureApi::zeCommandListAppendImageCopy>(args);
}

TEST_F(ExtractParametersTest, zeCommandListAppendImageCopyRegion) {
    Closure<CaptureApi::zeCommandListAppendImageCopyRegion>::ApiArgs args{};
    args.numWaitEvents = 1;
    args.phWaitEvents = dummyEvents;
    args.hSignalEvent = dummyEvents[0];
    args.hDstImage = reinterpret_cast<ze_image_handle_t>(0x1);
    args.hSrcImage = reinterpret_cast<ze_image_handle_t>(0x2);
    args.pDstRegion = &dummyRegion;
    args.pSrcRegion = &dummyRegion;
    expectAllApiArgsPresent<CaptureApi::zeCommandListAppendImageCopyRegion>(args);
}

TEST_F(ExtractParametersTest, zeCommandListAppendImageCopyToMemory) {
    Closure<CaptureApi::zeCommandListAppendImageCopyToMemory>::ApiArgs args{};
    args.numWaitEvents = 1;
    args.phWaitEvents = dummyEvents;
    args.hSignalEvent = dummyEvents[0];
    args.pSrcRegion = &dummyRegion;
    expectAllApiArgsPresent<CaptureApi::zeCommandListAppendImageCopyToMemory>(args);
}

TEST_F(ExtractParametersTest, zeCommandListAppendImageCopyFromMemory) {
    Closure<CaptureApi::zeCommandListAppendImageCopyFromMemory>::ApiArgs args{};
    args.numWaitEvents = 1;
    args.phWaitEvents = dummyEvents;
    args.hSignalEvent = dummyEvents[0];
    args.pDstRegion = &dummyRegion;
    expectAllApiArgsPresent<CaptureApi::zeCommandListAppendImageCopyFromMemory>(args);
}

TEST_F(ExtractParametersTest, zeCommandListAppendMemoryPrefetch) {
    Closure<CaptureApi::zeCommandListAppendMemoryPrefetch>::ApiArgs args{};
    expectAllApiArgsPresent<CaptureApi::zeCommandListAppendMemoryPrefetch>(args);
}

TEST_F(ExtractParametersTest, zeCommandListAppendMemAdvise) {
    Closure<CaptureApi::zeCommandListAppendMemAdvise>::ApiArgs args{};
    expectAllApiArgsPresent<CaptureApi::zeCommandListAppendMemAdvise>(args);
}

TEST_F(ExtractParametersTest, zeCommandListAppendSignalEvent) {
    Closure<CaptureApi::zeCommandListAppendSignalEvent>::ApiArgs args{};
    expectAllApiArgsPresent<CaptureApi::zeCommandListAppendSignalEvent>(args);
}

TEST_F(ExtractParametersTest, zeCommandListAppendWaitOnEvents) {
    Closure<CaptureApi::zeCommandListAppendWaitOnEvents>::ApiArgs args{};
    args.numEvents = 1;
    args.phEvents = dummyEvents;
    expectAllApiArgsPresent<CaptureApi::zeCommandListAppendWaitOnEvents>(args);
}

TEST_F(ExtractParametersTest, zeCommandListAppendEventReset) {
    Closure<CaptureApi::zeCommandListAppendEventReset>::ApiArgs args{};
    expectAllApiArgsPresent<CaptureApi::zeCommandListAppendEventReset>(args);
}

TEST_F(ExtractParametersTest, zeCommandListAppendQueryKernelTimestamps) {
    Closure<CaptureApi::zeCommandListAppendQueryKernelTimestamps>::ApiArgs args{};
    args.numEvents = 1;
    args.phEvents = dummyEvents;
    args.numWaitEvents = 1;
    args.phWaitEvents = dummyEvents;
    args.hSignalEvent = dummyEvents[0];
    args.pOffsets = dummyOffsets;
    expectAllApiArgsPresent<CaptureApi::zeCommandListAppendQueryKernelTimestamps>(args);
}

TEST_F(ExtractParametersTest, zeCommandListAppendLaunchKernel) {
    Closure<CaptureApi::zeCommandListAppendLaunchKernel>::ApiArgs args{};
    args.numWaitEvents = 1;
    args.phWaitEvents = dummyEvents;
    args.hSignalEvent = dummyEvents[0];
    args.kernelHandle = &kernel;
    args.launchKernelArgs = &dummyLaunchArgs;
    expectAllApiArgsPresent<CaptureApi::zeCommandListAppendLaunchKernel>(args);
}

TEST_F(ExtractParametersTest, zeCommandListAppendLaunchCooperativeKernel) {
    Closure<CaptureApi::zeCommandListAppendLaunchCooperativeKernel>::ApiArgs args{};
    args.numWaitEvents = 1;
    args.phWaitEvents = dummyEvents;
    args.hSignalEvent = dummyEvents[0];
    args.kernelHandle = &kernel;
    args.launchKernelArgs = &dummyLaunchArgs;
    expectAllApiArgsPresent<CaptureApi::zeCommandListAppendLaunchCooperativeKernel>(args);
}

TEST_F(ExtractParametersTest, zeCommandListAppendLaunchKernelIndirect) {
    Closure<CaptureApi::zeCommandListAppendLaunchKernelIndirect>::ApiArgs args{};
    args.numWaitEvents = 1;
    args.phWaitEvents = dummyEvents;
    args.hSignalEvent = dummyEvents[0];
    args.kernelHandle = &kernel;
    args.launchArgsBuffer = &dummyLaunchArgs;
    expectAllApiArgsPresent<CaptureApi::zeCommandListAppendLaunchKernelIndirect>(args);
}

TEST_F(ExtractParametersTest, zeCommandListAppendLaunchKernelWithParameters) {
    // currently skipped as zeCommandListAppendLaunchKernelWithParameters is not supported yet
    GTEST_SKIP();
    Closure<CaptureApi::zeCommandListAppendLaunchKernelWithParameters>::ApiArgs args{nullptr};
    args.numWaitEvents = 1;
    args.phWaitEvents = dummyEvents;
    args.hSignalEvent = dummyEvents[0];
    args.kernelHandle = &kernel;
    args.pGroupCounts = &dummyLaunchArgs;
    expectAllApiArgsPresent<CaptureApi::zeCommandListAppendLaunchKernelWithParameters>(args);
}

TEST_F(ExtractParametersTest, zeCommandListAppendLaunchKernelWithArguments) {
    // currently skipped as zeCommandListAppendLaunchKernelWithArguments is not supported yet
    GTEST_SKIP();
    Closure<CaptureApi::zeCommandListAppendLaunchKernelWithArguments>::ApiArgs args{nullptr};
    args.numWaitEvents = 1;
    args.phWaitEvents = dummyEvents;
    args.hSignalEvent = dummyEvents[0];
    args.kernelHandle = &kernel;
    expectAllApiArgsPresent<CaptureApi::zeCommandListAppendLaunchKernelWithArguments>(args);
}

TEST_F(ExtractParametersTest, zeCommandListAppendLaunchMultipleKernelsIndirect) {
    Closure<CaptureApi::zeCommandListAppendLaunchMultipleKernelsIndirect>::ApiArgs args{};
    args.numWaitEvents = 1;
    args.phWaitEvents = dummyEvents;
    args.hSignalEvent = dummyEvents[0];
    args.numKernels = 1;
    args.phKernels = dummyKernels;
    args.pCountBuffer = dummyCountBuffer;
    args.launchArgsBuffer = &dummyLaunchArgs;
    expectAllApiArgsPresent<CaptureApi::zeCommandListAppendLaunchMultipleKernelsIndirect>(args);
}

TEST_F(ExtractParametersTest, zeCommandListAppendSignalExternalSemaphoreExt) {
    Closure<CaptureApi::zeCommandListAppendSignalExternalSemaphoreExt>::ApiArgs args{};
    args.numWaitEvents = 1;
    args.phWaitEvents = dummyEvents;
    args.hSignalEvent = dummyEvents[0];
    args.numSemaphores = 1;
    args.phSemaphores = dummySemaphores;
    args.signalParams = &dummySignalParams;
    expectAllApiArgsPresent<CaptureApi::zeCommandListAppendSignalExternalSemaphoreExt>(args);
}

TEST_F(ExtractParametersTest, zeCommandListAppendWaitExternalSemaphoreExt) {
    Closure<CaptureApi::zeCommandListAppendWaitExternalSemaphoreExt>::ApiArgs args{};
    args.numWaitEvents = 1;
    args.phWaitEvents = dummyEvents;
    args.hSignalEvent = dummyEvents[0];
    args.numSemaphores = 1;
    args.phSemaphores = dummySemaphores;
    args.waitParams = &dummyWaitParams;
    expectAllApiArgsPresent<CaptureApi::zeCommandListAppendWaitExternalSemaphoreExt>(args);
}

TEST_F(ExtractParametersTest, zeCommandListAppendImageCopyToMemoryExt) {
    Closure<CaptureApi::zeCommandListAppendImageCopyToMemoryExt>::ApiArgs args{};
    args.numWaitEvents = 1;
    args.phWaitEvents = dummyEvents;
    args.hSignalEvent = dummyEvents[0];
    args.pSrcRegion = &dummyRegion;
    expectAllApiArgsPresent<CaptureApi::zeCommandListAppendImageCopyToMemoryExt>(args);
}

TEST_F(ExtractParametersTest, zeCommandListAppendImageCopyFromMemoryExt) {
    Closure<CaptureApi::zeCommandListAppendImageCopyFromMemoryExt>::ApiArgs args{};
    args.numWaitEvents = 1;
    args.phWaitEvents = dummyEvents;
    args.hSignalEvent = dummyEvents[0];
    args.pDstRegion = &dummyRegion;
    expectAllApiArgsPresent<CaptureApi::zeCommandListAppendImageCopyFromMemoryExt>(args);
}

TEST_F(ExtractParametersTest, GivenMultipleWaitEventsWhenExtractParametersIsCalledThenParametersAreExtractedCorrectly) {
    ze_event_handle_t multipleWaitEvents[2] = {
        reinterpret_cast<ze_event_handle_t>(0x100),
        reinterpret_cast<ze_event_handle_t>(0x200)};

    Closure<CaptureApi::zeCommandListAppendBarrier>::ApiArgs args{};
    args.numWaitEvents = 2;
    args.phWaitEvents = multipleWaitEvents;
    args.hSignalEvent = dummyEvents[0];
    Closure<CaptureApi::zeCommandListAppendBarrier> closure(args, storage);
    auto params = GraphDumpHelper::extractParameters<CaptureApi::zeCommandListAppendBarrier>(closure, storage);

    EXPECT_TRUE(hasParam(params, "phWaitEvents"));
    EXPECT_TRUE(hasParam(params, "phWaitEvents[0]"));
    EXPECT_TRUE(hasParam(params, "phWaitEvents[1]"));

    std::string waitEventStr1 = GraphDumpHelper::formatPointer(multipleWaitEvents[0]);
    std::string waitEventStr2 = GraphDumpHelper::formatPointer(multipleWaitEvents[1]);
    EXPECT_EQ(getParamValue(params, "phWaitEvents[0]"), waitEventStr1);
    EXPECT_EQ(getParamValue(params, "phWaitEvents[1]"), waitEventStr2);
}

TEST_F(ExtractParametersTest, GivenMultipleEventsWhenExtractParametersIsCalledThenParametersAreExtractedCorrectly) {
    constexpr uintptr_t firstEventAddress = 0x400;
    constexpr uintptr_t secondEventAddress = 0x500;
    ze_event_handle_t multipleEvents[2] = {
        reinterpret_cast<ze_event_handle_t>(firstEventAddress),
        reinterpret_cast<ze_event_handle_t>(secondEventAddress)};

    Closure<CaptureApi::zeCommandListAppendWaitOnEvents>::ApiArgs args{};
    args.numEvents = 2;
    args.phEvents = multipleEvents;

    Closure<CaptureApi::zeCommandListAppendWaitOnEvents> closure(args, storage);
    auto params = GraphDumpHelper::extractParameters<CaptureApi::zeCommandListAppendWaitOnEvents>(closure, storage);

    EXPECT_TRUE(hasParam(params, "phEvents"));
    EXPECT_TRUE(hasParam(params, "phEvents[0]"));
    EXPECT_TRUE(hasParam(params, "phEvents[1]"));

    std::string eventStr1 = GraphDumpHelper::formatPointer(multipleEvents[0]);
    std::string eventStr2 = GraphDumpHelper::formatPointer(multipleEvents[1]);
    EXPECT_EQ(getParamValue(params, "phEvents[0]"), eventStr1);
    EXPECT_EQ(getParamValue(params, "phEvents[1]"), eventStr2);
}

TEST_F(ExtractParametersTest, GivenMultipleSemaphoresWhenExtractParametersIsCalledThenParametersAreExtractedCorrectly) {
    constexpr uintptr_t firstSemaphoreAddress = 0x600;
    constexpr uintptr_t secondSemaphoreAddress = 0x700;
    ze_external_semaphore_ext_handle_t multipleSemaphores[2] = {
        reinterpret_cast<ze_external_semaphore_ext_handle_t>(firstSemaphoreAddress),
        reinterpret_cast<ze_external_semaphore_ext_handle_t>(secondSemaphoreAddress)};

    Closure<CaptureApi::zeCommandListAppendSignalExternalSemaphoreExt>::ApiArgs args{};
    args.numSemaphores = 2;
    args.phSemaphores = multipleSemaphores;
    args.signalParams = &dummySignalParams;

    Closure<CaptureApi::zeCommandListAppendSignalExternalSemaphoreExt> closure(args, storage);
    auto params = GraphDumpHelper::extractParameters<CaptureApi::zeCommandListAppendSignalExternalSemaphoreExt>(closure, storage);

    EXPECT_TRUE(hasParam(params, "phSemaphores"));
    EXPECT_TRUE(hasParam(params, "phSemaphores[0]"));
    EXPECT_TRUE(hasParam(params, "phSemaphores[1]"));

    std::string semaphoreStr1 = GraphDumpHelper::formatPointer(multipleSemaphores[0]);
    std::string semaphoreStr2 = GraphDumpHelper::formatPointer(multipleSemaphores[1]);
    EXPECT_EQ(getParamValue(params, "phSemaphores[0]"), semaphoreStr1);
    EXPECT_EQ(getParamValue(params, "phSemaphores[1]"), semaphoreStr2);
}

TEST_F(ExtractParametersTest, GivenZeroEventsWhenExtractParametersIsCalledThenNoIndividualEntriesArePresent) {
    Closure<CaptureApi::zeCommandListAppendWaitOnEvents>::ApiArgs args{};
    args.numEvents = 0;
    args.phEvents = nullptr;

    Closure<CaptureApi::zeCommandListAppendWaitOnEvents> closure(args, storage);
    auto params = GraphDumpHelper::extractParameters<CaptureApi::zeCommandListAppendWaitOnEvents>(closure, storage);

    EXPECT_TRUE(hasParam(params, "hCommandList"));
    EXPECT_FALSE(hasParam(params, "numEvents"));
    EXPECT_FALSE(hasParam(params, "phEvents"));
    EXPECT_FALSE(hasParam(params, "phEvents[0]"));
}

TEST_F(ExtractParametersTest, GivenNoOptionalRegionParametersWhenExtractParametersIsCalledThenNoRegionEntriesArePresent) {
    Closure<CaptureApi::zeCommandListAppendImageCopyRegion>::ApiArgs args{};
    Closure<CaptureApi::zeCommandListAppendImageCopyRegion> closure(args, storage);
    auto params = GraphDumpHelper::extractParameters<CaptureApi::zeCommandListAppendImageCopyRegion>(closure, storage);

    EXPECT_TRUE(hasParam(params, "hCommandList"));
    EXPECT_FALSE(hasParam(params, "pSrcRegion"));
    EXPECT_FALSE(hasParam(params, "pDstRegion"));
}

TEST_F(ExtractParametersTest, GivenNoOptionalOffsetsParameterWhenExtractParametersIsCalledThenNoOffsetsEntryIsPresent) {
    Closure<CaptureApi::zeCommandListAppendQueryKernelTimestamps>::ApiArgs args{};
    Closure<CaptureApi::zeCommandListAppendQueryKernelTimestamps> closure(args, storage);
    auto params = GraphDumpHelper::extractParameters<CaptureApi::zeCommandListAppendQueryKernelTimestamps>(closure, storage);

    EXPECT_TRUE(hasParam(params, "hCommandList"));
    EXPECT_FALSE(hasParam(params, "pOffsets"));
}

class ExtractKernelParametersTestFixture : public ModuleImmutableDataFixture, public ExtractParametersTestFixture {
  public:
    void setUp() {
        ModuleImmutableDataFixture::setUp();
        ExtractParametersTestFixture::setUp();

        uint32_t perHwThreadPrivateMemorySizeRequested = 0u;
        mockKernelImmData = std::make_unique<MockImmutableData>(perHwThreadPrivateMemorySizeRequested);
        mockKernelImmData->mockKernelInfo->kernelDescriptor.kernelMetadata.kernelName = kernelName;
        createModuleFromMockBinary(perHwThreadPrivateMemorySizeRequested, false, mockKernelImmData.get());
        auto pModule = ModuleImmutableDataFixture::module.get();
        mockKernel = std::make_unique<ModuleImmutableDataFixture::MockKernel>(pModule);
    }

    void tearDown() {
        mockKernel.reset(nullptr);
        mockKernelImmData.reset(nullptr);
        ModuleImmutableDataFixture::tearDown();
        ExtractParametersTestFixture::tearDown();
    }

    ze_kernel_handle_t kernelHandle;
    std::unique_ptr<MockImmutableData> mockKernelImmData;
    std::unique_ptr<MockKernel> mockKernel;
};

using ExtractKernelParametersTest = Test<ExtractKernelParametersTestFixture>;

TEST_F(ExtractKernelParametersTest, GivenKernelArgWhenExtractParametersIsCalledThenKernelArgIsPresent) {
    constexpr ConstStringRef argType = "uint32_t";
    constexpr ConstStringRef argName = "exampleVar";

    mockKernelImmData->resizeExplicitArgs(1);
    ArgTypeMetadataExtended metadata;
    metadata.type = argType.data();
    metadata.argName = argName.data();
    mockKernelImmData->mockKernelDescriptor->explicitArgsExtendedMetadata.push_back(metadata);
    createKernel(mockKernel.get());

    Closure<CaptureApi::zeCommandListAppendLaunchKernel>::ApiArgs args{};
    args.numWaitEvents = 1;
    args.phWaitEvents = dummyEvents;
    args.hSignalEvent = dummyEvents[0];
    args.kernelHandle = mockKernel->toHandle();
    args.launchKernelArgs = &dummyLaunchArgs;

    Closure<CaptureApi::zeCommandListAppendLaunchKernel> closure(args, storage);
    auto params = GraphDumpHelper::extractParameters<CaptureApi::zeCommandListAppendLaunchKernel>(closure, storage);

    const std::string expectedKernelArgParam = "arg[0]";
    const std::string expectedKernelArgDetails = argType.str() + " " + argName.str();
    EXPECT_TRUE(hasParam(params, expectedKernelArgParam));
    EXPECT_EQ(getParamValue(params, expectedKernelArgParam), expectedKernelArgDetails);
}

TEST_F(ExtractKernelParametersTest, GivenKernelNameWhenExtractParametersIsCalledThenKernelNameIsPresent) {
    createKernel(mockKernel.get());

    Closure<CaptureApi::zeCommandListAppendLaunchKernel>::ApiArgs args{};
    args.numWaitEvents = 1;
    args.phWaitEvents = dummyEvents;
    args.hSignalEvent = dummyEvents[0];
    args.kernelHandle = mockKernel->toHandle();
    args.launchKernelArgs = &dummyLaunchArgs;

    Closure<CaptureApi::zeCommandListAppendLaunchKernel> closure(args, storage);
    auto params = GraphDumpHelper::extractParameters<CaptureApi::zeCommandListAppendLaunchKernel>(closure, storage);

    EXPECT_TRUE(hasParam(params, "kernelName"));
    EXPECT_EQ(getParamValue(params, "kernelName"), kernelName);
}

} // namespace ult
} // namespace L0
