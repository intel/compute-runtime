/*
 * Copyright (C) 2018-2022 Intel Corporation
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
    void setUp(NEO::Device *pDevice);
    void tearDown();

    NEO::BuiltIns *pBuiltIns = nullptr;
};
