/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/skipped_aub_test_file_remover.h"

#include "shared/source/aub/aub_center.h"
#include "shared/source/execution_environment/root_device_environment.h"

#include <cstdio>
#include <iostream>

namespace NEO {

std::vector<std::string> SkippedAubTestFileRemover::aubFilesOfCurrentTest;

void SkippedAubTestFileRemover::recordAubFile(const RootDeviceEnvironment &rootDeviceEnvironment) {
    auto aubFileName = rootDeviceEnvironment.aubCenter->getAubManager()->getFileName();
    if (!aubFileName.empty()) {
        aubFilesOfCurrentTest.push_back(std::move(aubFileName));
    }
}

void SkippedAubTestFileRemover::OnTestEnd(const ::testing::TestInfo &testInfo) {
    if (testInfo.result()->Skipped()) {
        for (const auto &aubFileName : aubFilesOfCurrentTest) {
            if (std::remove(aubFileName.c_str()) == 0) {
                std::cout << "Removed AUB file of skipped test: " << aubFileName << "\n";
            } else {
                std::cout << "Failed to remove AUB file of skipped test: " << aubFileName << "\n";
            }
        }
    }
    aubFilesOfCurrentTest.clear();
    aubFilesOfCurrentTest.shrink_to_fit();
}

} // namespace NEO
