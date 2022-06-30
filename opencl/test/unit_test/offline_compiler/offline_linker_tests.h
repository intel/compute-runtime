/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "mock/mock_argument_helper.h"
#include "mock/mock_ocloc_igc_facade.h"
#include "mock/mock_offline_linker.h"

namespace NEO {

class OfflineLinkerTest : public ::testing::Test {
  public:
    OfflineLinkerTest() {
        mockOclocIgcFacade = std::make_unique<MockOclocIgcFacade>(&mockArgHelper);
    }

    void SetUp() override;
    void TearDown() override;

    std::string getEmptySpirvFile() const;
    std::string getEmptyLlvmBcFile() const;
    MockOfflineLinker::InputFileContent createFileContent(const std::string &content, IGC::CodeType::CodeType_t codeType) const;

  protected:
    MockOclocArgHelper::FilesMap mockArgHelperFilesMap{};
    MockOclocArgHelper mockArgHelper{mockArgHelperFilesMap};
    std::unique_ptr<MockOclocIgcFacade> mockOclocIgcFacade{};
    char binaryToReturn[8]{7, 7, 7, 7, 0, 1, 2, 3};
};

} // namespace NEO