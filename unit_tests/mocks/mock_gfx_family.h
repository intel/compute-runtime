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

#include "runtime/helpers/hw_helper.h"

namespace OCLRT {

extern HwHelper *hwHelperFactory[IGFX_MAX_CORE];

struct GENX {
    static bool (*isSimulationFcn)(unsigned short);
    typedef struct tagINTERFACE_DESCRIPTOR_DATA {
    } INTERFACE_DESCRIPTOR_DATA;

    typedef struct tagBINDING_TABLE_STATE {
        inline uint32_t getRawData(const uint32_t index) {
            return 0;
        }
        typedef enum tagSURFACESTATEPOINTER {
            SURFACESTATEPOINTER_BIT_SHIFT = 0x6,
            SURFACESTATEPOINTER_ALIGN_SIZE = 0x40,
        } SURFACESTATEPOINTER;
    } BINDING_TABLE_STATE;
};

} // namespace OCLRT
