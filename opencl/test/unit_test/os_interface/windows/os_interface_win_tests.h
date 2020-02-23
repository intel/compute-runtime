/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/hw_info.h"
#include "shared/source/os_interface/device_factory.h"
#include "shared/source/os_interface/windows/os_interface.h"
#include "shared/source/os_interface/windows/wddm/wddm.h"

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
