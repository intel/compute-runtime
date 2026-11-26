/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/test/common/helpers/stream_capture.h"
#include "shared/test/common/helpers/ult_main_helpers.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/mock_io_functions.h"
#include "shared/test/common/test_macros/test.h"

#include <string>

namespace NEO {

class ReadDebugSettingsFromConfigTest : public ::testing::Test {
  public:
    template <typename DebugSettingsManagerType>
    std::string readDebugSettingsAndCaptureStdout(DebugSettingsManagerType &settingsManager) {
        capture.captureStdout();
        readDebugSettingsFromConfig(settingsManager);
        return capture.getCapturedStdout();
    }

    std::string configFileContent = "PrintDebugMessages = 1\nEnableLEO = 1\nFlushAllCaches = 1";
    VariableBackup<long int> ftellReturnBackup{&IoFunctions::mockFtellReturn, static_cast<long int>(configFileContent.size())};
    VariableBackup<size_t> freadReturnBackup{&IoFunctions::mockFreadReturn, configFileContent.size()};
    VariableBackup<char *> freadBufferBackup{&IoFunctions::mockFreadBuffer, configFileContent.data()};
    StreamCapture capture;
};

} // namespace NEO
