/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_configuration/aub_tests/aub_tests_configuration.inl"

#include "hw_cmds.h"

using namespace NEO;

template AubTestsConfig getAubTestsConfig<ICLFamily>();
