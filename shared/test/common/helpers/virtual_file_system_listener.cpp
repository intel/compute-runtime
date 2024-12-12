/*
 * Copyright (C) 2022-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/virtual_file_system_listener.h"

#include "shared/test/common/helpers/mock_file_io.h"

namespace NEO {
std::map<std::string, std::stringstream> virtualFileList;

void VirtualFileSystemListener::OnTestStart(const ::testing::TestInfo &testInfo) {
}

void VirtualFileSystemListener::OnTestEnd(const ::testing::TestInfo &) {
    virtualFileList.clear();
}
void VirtualFileSystemListener::OnTestProgramStart(const testing::UnitTest &) {}
void VirtualFileSystemListener::OnTestIterationStart(const testing::UnitTest &, int) {}
void VirtualFileSystemListener::OnEnvironmentsSetUpStart(const testing::UnitTest &) {}
void VirtualFileSystemListener::OnEnvironmentsSetUpEnd(const testing::UnitTest &) {}
void VirtualFileSystemListener::OnTestPartResult(const testing::TestPartResult &) {}
void VirtualFileSystemListener::OnEnvironmentsTearDownStart(const testing::UnitTest &) {}
void VirtualFileSystemListener::OnEnvironmentsTearDownEnd(const testing::UnitTest &) {}
void VirtualFileSystemListener::OnTestIterationEnd(const testing::UnitTest &, int) {}
void VirtualFileSystemListener::OnTestProgramEnd(const testing::UnitTest &) {}
} // namespace NEO
