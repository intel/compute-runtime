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
    void SetUp(NEO::Device *pDevice); // NOLINT(readability-identifier-naming)
    void TearDown();                  // NOLINT(readability-identifier-naming)

    NEO::BuiltIns *pBuiltIns = nullptr;
};
