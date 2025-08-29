/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/mock_io_functions.h"

#include "level_zero/core/test/unit_tests/experimental/test_graph.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_module.h"
#include "level_zero/experimental/source/graph/captured_apis/graph_captured_apis.h"
#include "level_zero/experimental/source/graph/graph_export.h"

#include "gtest/gtest.h"

#include <memory>

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

#define DEFINE_APIARGS_FIELDS(api, ...)                                                                             \
    template <>                                                                                                     \
    std::set<std::string> ExtractParametersTestFixture::getApiArgsFieldNames<Closure<CaptureApi::api>::ApiArgs>() { \
        return {__VA_ARGS__};                                                                                       \
    }

class ExtractParametersTestFixture : public DeviceFixture {
  protected:
    ClosureExternalStorage storage;

    ze_event_handle_t dummyEvents[1] = {reinterpret_cast<ze_event_handle_t>(0x1000)};
    ze_image_region_t dummyRegion = {};
    size_t dummyRangeSizes[1] = {1024};
    const void *dummyRanges[1] = {reinterpret_cast<const void *>(0x1000)};
    uint64_t dummyOffsets[1] = {0};
    ze_external_semaphore_ext_handle_t dummySemaphores[1] = {reinterpret_cast<ze_external_semaphore_ext_handle_t>(0x1)};
    ze_external_semaphore_signal_params_ext_t dummySignalParams = {};
    ze_external_semaphore_wait_params_ext_t dummyWaitParams = {};
    uint32_t dummyCountBuffer[1] = {1};
    ze_group_count_t dummyLaunchArgs = {1, 1, 1};
    std::unique_ptr<Mock<Module>> module = nullptr;
    Mock<KernelImp> kernel;
    ze_kernel_handle_t dummyKernels[1];

    void setUp() {
        DeviceFixture::setUp();
        module = std::make_unique<Mock<Module>>(this->device, nullptr);
        kernel.setModule(module.get());
        dummyKernels[0] = &kernel;
    }

    void tearDown() {
        DeviceFixture::tearDown();
    }

    template <CaptureApi api, typename ApiArgsT>
    void expectAllApiArgsPresent(const ApiArgsT &args) {
        Closure<api> closure(args, storage);
        auto params = GraphDumpHelper::extractParameters<api>(closure, storage);
        if (!closure.isSupported) {
            GTEST_SKIP();
        }

        std::set<std::string> paramNames;
        for (const auto &param : params) {
            paramNames.insert(param.first);
        }
        std::set<std::string> expectedFields = getApiArgsFieldNames<ApiArgsT>();
        EXPECT_EQ(paramNames, expectedFields);
    }

    template <typename T>
    std::set<std::string> getApiArgsFieldNames() { return {}; };

    bool hasParam(const std::vector<std::pair<std::string, std::string>> &params, const std::string &name) {
        return std::find_if(params.begin(), params.end(),
                            [&name](const auto &param) { return param.first == name; }) != params.end();
    }

    std::string getParamValue(const std::vector<std::pair<std::string, std::string>> &params, const std::string &name) {
        auto iter = std::find_if(params.begin(), params.end(),
                                 [&name](const auto &param) { return param.first == name; });
        return iter != params.end() ? iter->second : std::string{};
    }
};

using ExtractParametersTest = Test<ExtractParametersTestFixture>;

} // namespace ult
} // namespace L0
