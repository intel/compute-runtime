/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/command_stream/command_stream_receiver.h"
#include "core/helpers/ptr_math.h"
#include "opencl/source/mem_obj/buffer.h"

#include "aub_tests/fixtures/aub_fixture.h"

using namespace NEO;

struct AubWriteCopyReadBuffer : public AUBFixture,
                                public ::testing::Test {

    void SetUp() override {
        AUBFixture::SetUp(nullptr);
    }

    void TearDown() override {
        AUBFixture::TearDown();
    }

    template <typename FamilyType>
    void runTest();
};
