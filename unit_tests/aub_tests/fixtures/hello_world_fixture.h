/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "unit_tests/aub_tests/command_stream/aub_command_stream_fixture.h"
#include "unit_tests/aub_tests/fixtures/simple_arg_fixture.h"
#include "unit_tests/fixtures/hello_world_fixture.h"
#include "unit_tests/fixtures/simple_arg_fixture.h"

namespace OCLRT {

////////////////////////////////////////////////////////////////////////////////
// Factory where all command stream traffic funnels to an AUB file
////////////////////////////////////////////////////////////////////////////////
struct AUBHelloWorldFixtureFactory : public HelloWorldFixtureFactory {
    typedef AUBCommandStreamFixture CommandStreamFixture;
};
} // namespace OCLRT
