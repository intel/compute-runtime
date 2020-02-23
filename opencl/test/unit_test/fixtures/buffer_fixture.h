/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "opencl/source/mem_obj/buffer.h"
#include "opencl/test/unit_test/mocks/mock_context.h"

#include "CL/cl.h"

#include <cassert>
#include <memory>

struct BufferDefaults {
    enum { flags = CL_MEM_READ_WRITE };
    static const size_t sizeInBytes;
    static void *hostPtr;
    static NEO::Context *context;
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
    using Buffer = NEO::Buffer;
    using Context = NEO::Context;
    using MockContext = NEO::MockContext;

    static Buffer *create(Context *context = Traits::context) {
        auto retVal = CL_SUCCESS;
        auto hostPtr = Traits::flags & (CL_MEM_USE_HOST_PTR | CL_MEM_COPY_HOST_PTR) ? Traits::hostPtr : nullptr;
        auto buffer = Buffer::create(
            context ? context : std::shared_ptr<Context>(new MockContext).get(),
            Traits::flags,
            Traits::sizeInBytes,
            hostPtr,
            retVal);
        assert(buffer != nullptr);
        return buffer;
    }
};
