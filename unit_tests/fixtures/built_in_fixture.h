/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

namespace OCLRT {
class BuiltIns;
class Device;
} // namespace OCLRT

class BuiltInFixture {
  public:
    BuiltInFixture();

    void SetUp(OCLRT::Device *pDevice);
    void TearDown();

    OCLRT::BuiltIns *pBuiltIns;
};
