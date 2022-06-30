/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/offline_compiler/source/decoder/helper.h"
#include "shared/offline_compiler/source/decoder/iga_wrapper.h"

#include "opencl/test/unit_test/offline_compiler/mock/mock_iga_dll.h"

#include "gtest/gtest.h"

namespace NEO {

class IgaWrapperTest : public ::testing::Test {
  public:
    void SetUp() override {
        igaDllMock = std::make_unique<IgaDllMock>();
        testedIgaWrapper.setMessagePrinter(messagePrinter);
    }

  protected:
    static constexpr int igaStatusSuccess{0};
    static constexpr int igaStatusFailure{1};

    std::unique_ptr<IgaDllMock> igaDllMock{};
    MessagePrinter messagePrinter{};
    IgaWrapper testedIgaWrapper{};
};

} // namespace NEO