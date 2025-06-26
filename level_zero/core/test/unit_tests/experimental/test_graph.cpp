/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"
#include "level_zero/core/test/unit_tests/mocks/mock_context.h"
#include "level_zero/experimental/source/graph/graph.h"
#include "level_zero/include/level_zero/driver_experimental/zex_graph.h"

using namespace NEO;

namespace L0 {
namespace ult {

TEST(GraphTestApiCreate, GivenNonNullPNextThenGraphCreateReturnsError) {
    Mock<Context> ctx;
    ze_graph_handle_t graph = nullptr;
    ze_base_desc_t ext = {};
    ext.stype = ZE_STRUCTURE_TYPE_MUTABLE_GRAPH_ARGUMENT_EXP_DESC;
    ext.pNext = nullptr;
    auto err = zeGraphCreateExp(&ctx, &graph, &ext);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, err);
    EXPECT_EQ(nullptr, graph);
}

TEST(GraphTestApiCreate, GivenNullContextThenGraphCreateReturnsError) {
    ze_graph_handle_t graph = nullptr;
    auto err = zeGraphCreateExp(nullptr, &graph, nullptr);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, err);
    EXPECT_EQ(nullptr, graph);
}

TEST(GraphTestApiCreate, GivenValidContextThenGraphCreateReturnsSuccess) {
    Mock<Context> ctx;
    ze_graph_handle_t graph = nullptr;
    auto err = zeGraphCreateExp(&ctx, &graph, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, err);
    EXPECT_NE(nullptr, graph);

    err = zeGraphDestroyExp(graph);
    EXPECT_EQ(ZE_RESULT_SUCCESS, err);
}

TEST(GraphTestApiCreate, GivenInvalidGraphThenGraphDestroyReturnsError) {
    auto err = zeGraphDestroyExp(nullptr);
    EXPECT_NE(ZE_RESULT_SUCCESS, err);
}

TEST(GraphTestApiCaptureBeginEnd, GivenNonNullPNextThenGraphBeginCaptureReturnsError) {
    Mock<Context> ctx;
    Mock<CommandList> cmdlist;
    ze_base_desc_t ext = {};
    ext.stype = ZE_STRUCTURE_TYPE_MUTABLE_GRAPH_ARGUMENT_EXP_DESC;
    ext.pNext = nullptr;

    auto err = zeCommandListBeginGraphCaptureExp(&cmdlist, &ext);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, err);

    L0::Graph graph(&ctx, true);
    err = zeCommandListBeginCaptureIntoGraphExp(&cmdlist, &graph, &ext);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, err);
}

TEST(GraphTestApiCaptureBeginEnd, GivenNullDestinyGraphThenBeginCaptureReturnsError) {
    Mock<CommandList> cmdlist;

    auto err = zeCommandListBeginCaptureIntoGraphExp(&cmdlist, nullptr, nullptr);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, err);
}

TEST(GraphTestApiCaptureBeginEnd, GivenValidDestinyGraphThenBeginCaptureReturnsSuccessAndOutputGraphIsTheSameAsInput) {
    Mock<Context> ctx;
    Mock<CommandList> cmdlist;

    L0::Graph graph(&ctx, true);
    auto err = zeCommandListBeginCaptureIntoGraphExp(&cmdlist, &graph, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, err);

    ze_graph_handle_t retGraph = nullptr;
    err = zeCommandListEndGraphCaptureExp(&cmdlist, &retGraph, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, err);
    EXPECT_EQ(retGraph, &graph);
}

TEST(GraphTestApiCaptureBeginEnd, GivenNonNullPNextThenGraphEndCaptureReturnsError) {
    Mock<Context> ctx;
    Mock<CommandList> cmdlist;
    ze_base_desc_t ext = {};
    ext.stype = ZE_STRUCTURE_TYPE_MUTABLE_GRAPH_ARGUMENT_EXP_DESC;
    ext.pNext = nullptr;

    auto err = zeCommandListBeginGraphCaptureExp(&cmdlist, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, err);

    ze_graph_handle_t retGraph = nullptr;
    err = zeCommandListEndGraphCaptureExp(&cmdlist, &retGraph, &ext);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, err);
    EXPECT_EQ(nullptr, retGraph);
}

TEST(GraphTestApiCaptureBeginEnd, WhenNoDestinyGraphProvidedThenEndCaptureReturnsNewGraph) {
    Mock<Context> ctx;
    Mock<CommandList> cmdlist;

    auto err = zeCommandListBeginGraphCaptureExp(&cmdlist, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, err);

    ze_graph_handle_t retGraph = nullptr;
    err = zeCommandListEndGraphCaptureExp(&cmdlist, &retGraph, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, err);
    EXPECT_NE(nullptr, retGraph);

    zeGraphDestroyExp(retGraph);
}

TEST(GraphTestApiCaptureBeginEnd, WhenCommandListIsNotRecordingThenEndCaptureReturnsError) {
    Mock<CommandList> cmdlist;
    ze_graph_handle_t retGraph = nullptr;
    auto err = zeCommandListEndGraphCaptureExp(&cmdlist, &retGraph, nullptr);
    EXPECT_NE(ZE_RESULT_SUCCESS, err);
    EXPECT_EQ(nullptr, retGraph);
}

TEST(GraphTestApiCaptureBeginEnd, WhenNoDestinyGraphProvidedThenEndCaptureRequiresOutputGraphPlaceholder) {
    Mock<CommandList> cmdlist;
    auto err = zeCommandListBeginGraphCaptureExp(&cmdlist, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, err);

    err = zeCommandListEndGraphCaptureExp(&cmdlist, nullptr, nullptr);
    EXPECT_NE(ZE_RESULT_SUCCESS, err);
}

TEST(GraphTestApiCaptureBeginEnd, WhenDestinyGraphProvidedThenEndCaptureDoesNotRequireOutputGraphPlaceholder) {
    Mock<Context> ctx;
    Mock<CommandList> cmdlist;
    L0::Graph graph(&ctx, true);
    auto err = zeCommandListBeginCaptureIntoGraphExp(&cmdlist, &graph, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, err);

    err = zeCommandListEndGraphCaptureExp(&cmdlist, nullptr, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, err);
}

TEST(GraphTestApiCaptureBeginEnd, WhenCommandListIsAlreadyRecordingThenBeginCaptureReturnsError) {
    Mock<Context> ctx;
    Mock<CommandList> cmdlist;

    L0::Graph graph1(&ctx, true);
    L0::Graph graph2(&ctx, true);
    auto err = zeCommandListBeginCaptureIntoGraphExp(&cmdlist, &graph1, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, err);

    err = zeCommandListBeginCaptureIntoGraphExp(&cmdlist, &graph2, nullptr);
    EXPECT_NE(ZE_RESULT_SUCCESS, err);

    err = zeCommandListBeginGraphCaptureExp(&cmdlist, nullptr);
    EXPECT_NE(ZE_RESULT_SUCCESS, err);

    ze_graph_handle_t retGraph = nullptr;
    err = zeCommandListEndGraphCaptureExp(&cmdlist, &retGraph, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, err);
    EXPECT_EQ(retGraph, &graph1);
}

TEST(GraphTestApiInstantiate, GivenInvalidSourceGraphThenInstaniateGraphReturnsError) {
    ze_executable_graph_handle_t execGraph = nullptr;
    auto err = zeCommandListInstantiateGraphExp(nullptr, &execGraph, nullptr);
    EXPECT_NE(ZE_RESULT_SUCCESS, err);
    EXPECT_EQ(nullptr, execGraph);
}

TEST(GraphTestApiInstantiate, GivenInvalidOutputGraphPlaceholderThenInstaniateGraphReturnsError) {
    Mock<Context> ctx;
    L0::Graph srcGraph(&ctx, true);

    auto err = zeCommandListInstantiateGraphExp(&srcGraph, nullptr, nullptr);
    EXPECT_NE(ZE_RESULT_SUCCESS, err);
}

TEST(GraphTestApiInstantiate, GivenNonNullPNextThenInstaniateGraphReturnsError) {
    Mock<Context> ctx;
    L0::Graph srcGraph(&ctx, true);

    ze_base_desc_t ext = {};
    ext.stype = ZE_STRUCTURE_TYPE_MUTABLE_GRAPH_ARGUMENT_EXP_DESC;
    ext.pNext = nullptr;

    ze_executable_graph_handle_t execGraph = nullptr;
    auto err = zeCommandListInstantiateGraphExp(&srcGraph, &execGraph, &ext);
    EXPECT_NE(ZE_RESULT_SUCCESS, err);
    EXPECT_EQ(nullptr, execGraph);
}

TEST(GraphTestApiInstantiate, GivenValidSourceGraphThenInstaniateReturnsValidExecutableGraph) {
    Mock<Context> ctx;
    L0::Graph srcGraph(&ctx, true);

    ze_executable_graph_handle_t execGraph = nullptr;
    auto err = zeCommandListInstantiateGraphExp(&srcGraph, &execGraph, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, err);
    EXPECT_NE(nullptr, execGraph);

    err = zeExecutableGraphDestroyExp(execGraph);
    EXPECT_EQ(ZE_RESULT_SUCCESS, err);
}

TEST(GraphTestApiInstantiate, GivenInvalidExecutableGraphThenGraphDestroyReturnsError) {
    auto err = zeExecutableGraphDestroyExp(nullptr);
    EXPECT_NE(ZE_RESULT_SUCCESS, err);
}

TEST(GraphDebugApis, WhenIsGraphCaptureEnabledIsCalledThenReturnUnsupportedFeature) {
    Mock<CommandList> cmdlist;
    auto err = zeCommandListIsGraphCaptureEnabledExp(&cmdlist);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, err);
}

TEST(GraphDebugApis, WhenGraphIsEmptyIsCalledThenReturnUnsupportedFeature) {
    Mock<Context> ctx;
    L0::Graph srcGraph(&ctx, true);
    auto err = zeGraphIsEmptyExp(&srcGraph);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, err);
}

TEST(GraphDebugApis, WhenGraphDumpContentsIsCalledThenReturnUnsupportedFeature) {
    Mock<Context> ctx;
    L0::Graph srcGraph(&ctx, true);
    auto err = zeGraphDumpContentsExp(&srcGraph, "dump", nullptr);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, err);
}

TEST(GraphSubmit, GivenNonNullPNextThenGraphAppendReturnsError) {
    Mock<Context> ctx;
    Mock<CommandList> cmdlist;
    L0::Graph srcGraph(&ctx, true);
    L0::ExecutableGraph execGraph(&srcGraph);

    ze_base_desc_t ext = {};
    ext.stype = ZE_STRUCTURE_TYPE_MUTABLE_GRAPH_ARGUMENT_EXP_DESC;
    ext.pNext = nullptr;

    auto err = zeCommandListAppendGraphExp(&cmdlist, &execGraph, &ext, nullptr, 0, nullptr);
    EXPECT_NE(ZE_RESULT_SUCCESS, err);
}

TEST(GraphSubmit, GivenInvalidCmdListThenGraphAppendReturnsError) {
    Mock<Context> ctx;
    L0::Graph srcGraph(&ctx, true);
    L0::ExecutableGraph execGraph(&srcGraph);
    auto err = zeCommandListAppendGraphExp(nullptr, &execGraph, nullptr, nullptr, 0, nullptr);
    EXPECT_NE(ZE_RESULT_SUCCESS, err);
}

TEST(GraphSubmit, GivenInvalidGraphThenGraphAppendReturnsError) {
    Mock<CommandList> cmdlist;

    auto err = zeCommandListAppendGraphExp(&cmdlist, nullptr, nullptr, nullptr, 0, nullptr);
    EXPECT_NE(ZE_RESULT_SUCCESS, err);
}

TEST(GraphSubmit, GivenValidCmdListAndGraphThenGraphAppendReturnsSuccess) {
    Mock<Context> ctx;
    Mock<CommandList> cmdlist;
    L0::Graph srcGraph(&ctx, true);
    L0::ExecutableGraph execGraph(&srcGraph);

    auto err = zeCommandListAppendGraphExp(&cmdlist, &execGraph, nullptr, nullptr, 0, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, err);
}

} // namespace ult
} // namespace L0
