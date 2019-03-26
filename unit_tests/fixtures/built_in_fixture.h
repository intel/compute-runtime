/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

namespace NEO {
class BuiltIns;
class Device;
} // namespace NEO

class BuiltInFixture {
  public:
    BuiltInFixture();

    void SetUp(NEO::Device *pDevice);
    void TearDown();

    NEO::BuiltIns *pBuiltIns;
};
