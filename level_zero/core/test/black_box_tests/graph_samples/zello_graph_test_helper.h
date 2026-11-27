/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "zello_common.h"

#include <string>

namespace GraphTests {

struct GraphDumpSettings {
    bool dump = false;
    ze_record_replay_graph_ext_dump_mode_t mode = ZE_RECORD_REPLAY_GRAPH_EXT_DUMP_MODE_DETAILED;
};

GraphDumpSettings getGraphDumpSettings(int argc, char *argv[]);
void dumpGraphToDotIfEnabled(ze_graph_handle_t virtualGraph, const std::string &testName, const GraphDumpSettings &dumpSettings);
bool isGraphExtensionPresent(ze_driver_handle_t driverHandle);

} // namespace GraphTests
