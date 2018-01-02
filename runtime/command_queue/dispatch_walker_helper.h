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

#pragma once

namespace OCLRT {

constexpr int NUM_ALU_INST_FOR_READ_MODIFY_WRITE = 4;

constexpr int L3SQC_BIT_LQSC_RO_PERF_DIS = 0x08000000;
constexpr int L3SQC_REG4 = 0xB118;

constexpr int GPGPU_WALKER_COOKIE_VALUE_BEFORE_WALKER = 0xFFFFFFFF;
constexpr int GPGPU_WALKER_COOKIE_VALUE_AFTER_WALKER = 0x00000000;

constexpr int CS_GPR_R0 = 0x2600;
constexpr int CS_GPR_R1 = 0x2608;

constexpr int ALU_OPCODE_LOAD = 0x080;
constexpr int ALU_OPCODE_STORE = 0x180;
constexpr int ALU_OPCODE_OR = 0x103;
constexpr int ALU_OPCODE_AND = 0x102;

constexpr int ALU_REGISTER_R_0 = 0x0;
constexpr int ALU_REGISTER_R_1 = 0x1;
constexpr int ALU_REGISTER_R_SRCA = 0x20;
constexpr int ALU_REGISTER_R_SRCB = 0x21;
constexpr int ALU_REGISTER_R_ACCU = 0x31;

template <typename GfxFamily>
void applyWADisableLSQCROPERFforOCL(OCLRT::LinearStream *pCommandStream, const Kernel &kernel, bool disablePerfMode);

template <typename GfxFamily>
size_t getSizeForWADisableLSQCROPERFforOCL(const Kernel *pKernel);
} // namespace OCLRT
