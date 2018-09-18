/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

namespace OCLRT {
enum class TestMode { NotSpecified,
                      UnitTests,
                      AubTests,
                      AubTestsWithTbx,
                      TbxTests };

extern TestMode testMode;
} // namespace OCLRT
