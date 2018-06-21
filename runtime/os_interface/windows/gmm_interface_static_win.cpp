/*
 * Copyright (c) 2018, Intel Corporation
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

#include "runtime/gmm_helper/gmm_helper.h"

namespace OCLRT {

decltype(GmmHelper::initGlobalContextFunc) GmmHelper::initGlobalContextFunc = nullptr;
decltype(GmmHelper::destroyGlobalContextFunc) GmmHelper::destroyGlobalContextFunc = nullptr;
decltype(GmmHelper::createClientContextFunc) GmmHelper::createClientContextFunc = nullptr;
decltype(GmmHelper::deleteClientContextFunc) GmmHelper::deleteClientContextFunc = nullptr;

void GmmHelper::loadLib() {
    GmmHelper::initGlobalContextFunc = GmmInitGlobalContext;
    GmmHelper::destroyGlobalContextFunc = GmmDestroyGlobalContext;
    GmmHelper::createClientContextFunc = GmmCreateClientContext;
    GmmHelper::deleteClientContextFunc = GmmDeleteClientContext;
    isLoaded = true;
}
} // namespace OCLRT
extern "C" {
void GMMDebugBreak(const char *file, const char *function, const int line) {
}

void GMMPrintMessage(uint32_t debugLevel, const char *debugMessageFmt, ...) {
}
typedef struct GfxDebugControlRec {
    uint32_t Version;
    uint32_t Size;
    uint32_t AssertEnableMask;
    uint32_t EnableDebugFileDump;
    uint32_t DebugEnableMask;
    uint32_t RingBufDbgMask;
    uint32_t ReportAssertEnable;
    uint32_t AssertBreakDisable;

} GFX_DEBUG_CONTROL, *PGFX_DEBUG_CONTROL;
PGFX_DEBUG_CONTROL pDebugControl;
}
