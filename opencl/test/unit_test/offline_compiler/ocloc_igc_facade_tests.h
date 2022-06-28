/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/hw_info.h"

#include "mock/mock_argument_helper.h"

namespace NEO {

class OclocIgcFacadeTest : public ::testing::Test {
  protected:
    MockOclocArgHelper::FilesMap mockArgHelperFilesMap{};
    MockOclocArgHelper mockArgHelper{mockArgHelperFilesMap};
    HardwareInfo hwInfo{};
};

} // namespace NEO