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
#include "runtime/mem_obj/buffer.h"
#include "unit_tests/mocks/mock_context.h"
#include "CL/cl.h"
#include <cassert>
#include <memory>

struct BufferDefaults {
    enum { flags = CL_MEM_READ_WRITE };
    static const size_t sizeInBytes;
    static void *hostPtr;
    static OCLRT::Context *context;
};

template <typename BaseClass = BufferDefaults>
struct BufferUseHostPtr : public BaseClass {
    enum { flags = BaseClass::flags | CL_MEM_USE_HOST_PTR };
};

template <typename BaseClass = BufferDefaults>
struct BufferReadOnly : public BaseClass {
    enum { flags = BaseClass::flags | CL_MEM_READ_ONLY };
};

template <typename BaseClass = BufferDefaults>
struct BufferWriteOnly : public BaseClass {
    enum { flags = BaseClass::flags | CL_MEM_WRITE_ONLY };
};

template <typename Traits = BufferDefaults>
struct BufferHelper {
    using Buffer = OCLRT::Buffer;
    using Context = OCLRT::Context;
    using MockContext = OCLRT::MockContext;

    static Buffer *create(Context *context = Traits::context) {
        auto retVal = CL_SUCCESS;
        auto buffer = Buffer::create(
            context ? context : std::shared_ptr<Context>(new MockContext).get(),
            Traits::flags,
            Traits::sizeInBytes,
            Traits::hostPtr,
            retVal);
        assert(buffer != nullptr);
        return buffer;
    }
};
