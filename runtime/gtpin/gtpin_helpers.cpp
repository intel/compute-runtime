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

#include "config.h"
#include "gtpin_dx11_interface.h"
#include "gtpin_helpers.h"
#include "CL/cl.h"
#include "runtime/context/context.h"
#include "runtime/helpers/validators.h"
#include "runtime/mem_obj/buffer.h"

using namespace gtpin;

namespace OCLRT {

GTPIN_DI_STATUS gtpinCreateBuffer(context_handle_t context, uint32_t size, resource_handle_t *resource) {
    GTPIN_DI_STATUS retVal = GTPIN_DI_ERROR_INVALID_ARGUMENT;

    do {
        cl_int diag = CL_SUCCESS;
        Context *pContext = castToObject<Context>((cl_context)context);
        if (pContext == nullptr) {
            break;
        }
        cl_mem buffer = Buffer::create(pContext, CL_MEM_READ_WRITE, size, nullptr, diag);
        if (buffer == nullptr) {
            break;
        }
        *resource = (resource_handle_t)buffer;
        retVal = GTPIN_DI_SUCCESS;
    } while (false);

    return retVal;
}
}
