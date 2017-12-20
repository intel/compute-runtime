/*
 * Copyright (c) 2017, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "runtime/helpers/translationtable_callbacks.h"
#include "runtime/command_stream/linear_stream.h"
#include "unit_tests/helpers/hw_parse.h"
#include "test.h"

using namespace OCLRT;

typedef ::testing::Test TTCallbacksTests;

HWTEST_F(TTCallbacksTests, writeL3Address) {
    typedef typename FamilyType::MI_LOAD_REGISTER_IMM MI_LOAD_REGISTER_IMM;
    char buff[256] = {};
    LinearStream stream(buff, 256);

    uint64_t address = 0x00234564002BCDEC;
    uint64_t value = 0xFEDCBA987654321C;

    auto res = TTCallbacks<FamilyType>::writeL3Address(&stream, value, address);
    EXPECT_EQ(1, res);
    EXPECT_EQ(2 * sizeof(MI_LOAD_REGISTER_IMM), stream.getUsed());

    HardwareParse hwParse;
    hwParse.parseCommands<FamilyType>(stream, 0);
    EXPECT_EQ(2u, hwParse.cmdList.size());

    auto cmd = genCmdCast<MI_LOAD_REGISTER_IMM *>(*hwParse.cmdList.begin());
    ASSERT_NE(nullptr, cmd);
    EXPECT_EQ(address & 0xFFFFFFFF, cmd->getRegisterOffset());
    EXPECT_EQ(value & 0xFFFFFFFF, cmd->getDataDword());

    cmd = genCmdCast<MI_LOAD_REGISTER_IMM *>(*(++hwParse.cmdList.begin()));
    ASSERT_NE(nullptr, cmd);
    EXPECT_EQ(address >> 32, cmd->getRegisterOffset());
    EXPECT_EQ(value >> 32, cmd->getDataDword());
}
