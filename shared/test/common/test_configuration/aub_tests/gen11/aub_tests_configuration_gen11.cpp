/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen11/hw_cmds.h"
#include "shared/test/common/test_configuration/aub_tests/aub_tests_configuration.inl"

using namespace NEO;

template AubTestsConfig getAubTestsConfig<Gen11Family>();
