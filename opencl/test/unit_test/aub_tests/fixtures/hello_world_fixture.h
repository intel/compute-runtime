/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "aub_tests/command_stream/aub_command_stream_fixture.h"
#include "aub_tests/fixtures/simple_arg_fixture.h"
#include "fixtures/hello_world_fixture.h"
#include "fixtures/simple_arg_fixture.h"

namespace NEO {

////////////////////////////////////////////////////////////////////////////////
// Factory where all command stream traffic funnels to an AUB file
////////////////////////////////////////////////////////////////////////////////
struct AUBHelloWorldFixtureFactory : public HelloWorldFixtureFactory {
    typedef AUBCommandStreamFixture CommandStreamFixture;
};
} // namespace NEO
