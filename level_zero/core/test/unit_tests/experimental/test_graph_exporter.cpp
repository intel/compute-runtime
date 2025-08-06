/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/mock_io_functions.h"

#include "level_zero/core/test/unit_tests/experimental/test_graph.h"

#include "gtest/gtest.h"

using namespace NEO;

namespace L0 {
namespace ult {

class MockGraphDotExporter : public GraphDotExporter {
  public:
    using GraphDotExporter::exportToString;
    using GraphDotExporter::findSubgraphIndex;
    using GraphDotExporter::findSubgraphIndexByCommandList;
    using GraphDotExporter::generateNodeId;
    using GraphDotExporter::generateSubgraphId;
    using GraphDotExporter::getCommandNodeAttributes;
    using GraphDotExporter::getCommandNodeLabel;
    using GraphDotExporter::getSubgraphFillColor;
    using GraphDotExporter::writeEdges;
    using GraphDotExporter::writeForkJoinEdges;
    using GraphDotExporter::writeHeader;
    using GraphDotExporter::writeNodes;
    using GraphDotExporter::writeSubgraphs;
    using GraphDotExporter::writeUnjoinedForkEdges;
};

class GraphDotExporterTest : public ::testing::Test {
  protected:
    GraphsCleanupGuard graphCleanup;
    Mock<Context> ctx;
    MockGraphDotExporter exporter;
    const std::string testFilePath = "test_graph_export.gv";
};

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

    std::string label = exporter.getCommandNodeLabel(testGraph, 0);
    EXPECT_EQ(label, "zeCommandListAppendBarrier");
}

TEST_F(GraphDotExporterTest, GivenDifferentCommandTypesWhenGetCommandNodeAttributesThenReturnsCorrectColors) {
    Graph testGraph{&ctx, true};
    Mock<Event> event;
    Mock<CommandList> cmdlist;

    testGraph.capture<CaptureApi::zeCommandListAppendBarrier>(&cmdlist, &event, 0U, nullptr);
    testGraph.capture<CaptureApi::zeCommandListAppendMemoryCopy>(&cmdlist, nullptr, nullptr, 0U, nullptr, 0U, nullptr);
    testGraph.capture<CaptureApi::zeCommandListAppendSignalEvent>(&cmdlist, &event);

    testGraph.stopCapturing();

    EXPECT_EQ(exporter.getCommandNodeAttributes(testGraph, 0), ", fillcolor=orange");
    EXPECT_EQ(exporter.getCommandNodeAttributes(testGraph, 1), ", fillcolor=lightblue");
    EXPECT_EQ(exporter.getCommandNodeAttributes(testGraph, 2), ", fillcolor=yellow");
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

TEST_F(GraphDotExporterTest, WhenFindSubgraphIndexWithInvalidSubgraphThenReturnsNullopt) {
    const StackVec<Graph *, 16> subGraphs;
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

TEST_F(GraphDotExporterTest, WhenFindSubgraphIndexByCommandListWithInvalidCommandListThenReturnsNullopt) {
    const StackVec<Graph *, 16> subGraphs;
    Mock<CommandList> fakeCmdList;
    auto index = exporter.findSubgraphIndexByCommandList(subGraphs, &fakeCmdList);

    EXPECT_FALSE(index.has_value());
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

class GraphDotExporterFileTest : public GraphDotExporterTest {
  protected:
    void SetUp() override {
        GraphDotExporterTest::SetUp();

        fopenBackup = std::make_unique<VariableBackup<decltype(NEO::IoFunctions::fopenPtr)>>(&NEO::IoFunctions::fopenPtr, NEO::IoFunctions::mockFopen);
        fwriteBackup = std::make_unique<VariableBackup<decltype(NEO::IoFunctions::fwritePtr)>>(&NEO::IoFunctions::fwritePtr, NEO::IoFunctions::mockFwrite);
        fcloseBackup = std::make_unique<VariableBackup<decltype(NEO::IoFunctions::fclosePtr)>>(&NEO::IoFunctions::fclosePtr, NEO::IoFunctions::mockFclose);

        mockFopenReturnedBackup = std::make_unique<VariableBackup<FILE *>>(&IoFunctions::mockFopenReturned);

        mockFopenCalledBefore = NEO::IoFunctions::mockFopenCalled;
        mockFwriteCalledBefore = NEO::IoFunctions::mockFwriteCalled;
        mockFcloseCalledBefore = NEO::IoFunctions::mockFcloseCalled;
    }

    void setupSuccessfulWrite(Graph &testGraph) {
        std::string expectedContent = exporter.exportToString(testGraph);
        ASSERT_NE(expectedContent.size(), 0U);

        mockFwriteReturnBackup = std::make_unique<VariableBackup<size_t>>(&NEO::IoFunctions::mockFwriteReturn, expectedContent.size());

        if (expectedContent.size() > 0) {
            buffer = std::make_unique<char[]>(expectedContent.size() + 1);
            memset(buffer.get(), 0, expectedContent.size() + 1);
            mockFwriteBufferBackup = std::make_unique<VariableBackup<char *>>(&NEO::IoFunctions::mockFwriteBuffer, buffer.get());
        }
    }

    void setupFailedOpen() {
        *mockFopenReturnedBackup = static_cast<FILE *>(nullptr);
    }

    void setupFailedWrite() {
        mockFwriteReturnBackup = std::make_unique<VariableBackup<size_t>>(&NEO::IoFunctions::mockFwriteReturn, static_cast<size_t>(0));
    }

    std::string getWrittenContent() const {
        return buffer ? std::string(buffer.get()) : std::string{};
    }

    std::unique_ptr<VariableBackup<decltype(NEO::IoFunctions::fopenPtr)>> fopenBackup;
    std::unique_ptr<VariableBackup<decltype(NEO::IoFunctions::fwritePtr)>> fwriteBackup;
    std::unique_ptr<VariableBackup<decltype(NEO::IoFunctions::fclosePtr)>> fcloseBackup;
    std::unique_ptr<VariableBackup<FILE *>> mockFopenReturnedBackup;
    std::unique_ptr<VariableBackup<size_t>> mockFwriteReturnBackup;
    std::unique_ptr<VariableBackup<char *>> mockFwriteBufferBackup;
    std::unique_ptr<char[]> buffer;

    uint32_t mockFopenCalledBefore;
    uint32_t mockFwriteCalledBefore;
    uint32_t mockFcloseCalledBefore;
};

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

} // namespace ult
} // namespace L0
