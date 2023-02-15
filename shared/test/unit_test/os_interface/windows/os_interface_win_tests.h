/*
 * Copyright (C) 2018-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/hw_info.h"
#include "shared/source/os_interface/os_interface.h"

#include "gtest/gtest.h"

using namespace NEO;

class OsInterfaceTest : public ::testing::Test {
  public:
    void SetUp() override {
        osInterface = std::unique_ptr<OSInterface>(new OSInterface());
    }

    void TearDown() override {
    }

    std::unique_ptr<OSInterface> osInterface;
};
