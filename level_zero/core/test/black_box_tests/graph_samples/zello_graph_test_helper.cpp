/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "zello_graph_test_helper.h"

#include <cstdio>
#include <iostream>
#include <vector>

namespace GraphTests {

GraphDumpSettings getGraphDumpSettings(int argc, char *argv[]) {
    GraphDumpSettings dumpSettings;
    dumpSettings.dump = LevelZeroBlackBoxTests::isParamEnabled(argc, argv, "-d", "--dump_graph");
    dumpSettings.mode = static_cast<ze_record_replay_graph_ext_dump_mode_t>(LevelZeroBlackBoxTests::getParamValue(argc, argv, "-sm", "--dump_graph_mode", 0U));
    return dumpSettings;
}

void dumpGraphToDotIfEnabled(ze_graph_handle_t virtualGraph, const std::string &testName, const GraphDumpSettings &dumpSettings) {
    if (!dumpSettings.dump) {
        return;
    }
    ze_record_replay_graph_ext_dump_desc_t dumpDesc = {};
    dumpDesc.stype = ZE_STRUCTURE_TYPE_RECORD_REPLAY_GRAPH_EXT_DUMP_DESC;
    dumpDesc.mode = dumpSettings.mode;
    dumpDesc.pNext = nullptr;

    std::string filename = testName + "_graph.gv";
    ze_result_t dumpResult = zeGraphDumpContentsExt(virtualGraph, filename.c_str(), &dumpDesc);

    if (dumpResult == ZE_RESULT_SUCCESS) {
        std::cout << "Graph dumped to " << filename << std::endl;
    } else {
        std::cerr << "Failed to dump graph for test " << testName << " (result: " << std::hex << dumpResult << std::dec << ")" << std::endl;
    }
}

bool isGraphExtensionPresent(ze_driver_handle_t driverHandle) {
    std::vector<ze_driver_extension_properties_t> extensionVector;
    ze_driver_extension_properties_t graphExtension{};
    std::snprintf(graphExtension.name, sizeof(graphExtension.name), "%s", ZE_RECORD_REPLAY_GRAPH_EXT_NAME);
    graphExtension.version = ZE_RECORD_REPLAY_GRAPH_EXT_VERSION_1_0;
    extensionVector.push_back(graphExtension);
    return LevelZeroBlackBoxTests::checkExtensionIsPresent(driverHandle, extensionVector);
}

} // namespace GraphTests
