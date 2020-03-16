/*
 * Copyright (C) 2017-2020 Intel Corporation
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
    void SetUp(NEO::Device *pDevice);
    void TearDown();

    NEO::BuiltIns *pBuiltIns = nullptr;
};
