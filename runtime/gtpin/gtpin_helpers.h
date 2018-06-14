/*
 * Copyright (c) 2017 - 2018, Intel Corporation
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
gtpin::GTPIN_DI_STATUS GTPIN_DRIVER_CALLCONV gtpinCreateBuffer(gtpin::context_handle_t context, uint32_t size, gtpin::resource_handle_t *pResource);
gtpin::GTPIN_DI_STATUS GTPIN_DRIVER_CALLCONV gtpinFreeBuffer(gtpin::context_handle_t context, gtpin::resource_handle_t resource);
gtpin::GTPIN_DI_STATUS GTPIN_DRIVER_CALLCONV gtpinMapBuffer(gtpin::context_handle_t context, gtpin::resource_handle_t resource, uint8_t **pAddress);
gtpin::GTPIN_DI_STATUS GTPIN_DRIVER_CALLCONV gtpinUnmapBuffer(gtpin::context_handle_t context, gtpin::resource_handle_t resource);
} // namespace OCLRT
