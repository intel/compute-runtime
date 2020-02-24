/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/helpers/ptr_math.h"

#include "opencl/source/mem_obj/buffer.h"
#include "opencl/test/unit_test/aub_tests/fixtures/aub_fixture.h"

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
