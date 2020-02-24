/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/hw_cmds.h"

#include "opencl/test/unit_test/aub_tests/aub_tests_configuration.inl"

using namespace NEO;

template AubTestsConfig GetAubTestsConfig<BDWFamily>();
