/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/offline_compiler/source/ocloc_arg_helper.h"
#include "shared/offline_compiler/source/ocloc_fatbinary.h"

#include "gtest/gtest.h"

#include <memory>

namespace NEO {
class OclocFatBinaryGetTargetConfigsForFatbinary : public ::testing::Test {
  public:
    OclocFatBinaryGetTargetConfigsForFatbinary() {
        oclocArgHelperWithoutInput = std::make_unique<OclocArgHelper>();
        oclocArgHelperWithoutInput->getPrinterRef() = MessagePrinter{true};
    }
    std::unique_ptr<OclocArgHelper> oclocArgHelperWithoutInput;
};
} // namespace NEO
