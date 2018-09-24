/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/command_stream/command_stream_receiver.h"
#include "runtime/helpers/options.h"
#include "runtime/helpers/ptr_math.h"
#include "runtime/mem_obj/buffer.h"

#include "unit_tests/aub_tests/fixtures/aub_fixture.h"

using namespace OCLRT;

struct AubWriteCopyReadBuffer : public AUBFixture,
                                public ::testing::Test {

    void SetUp() override {
        AUBFixture::SetUp();
    }

    void TearDown() override {
        AUBFixture::TearDown();
    }

    template <typename FamilyType>
    void runTest();
};
