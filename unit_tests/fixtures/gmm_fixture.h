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

#include "gtest/gtest.h"

#include "runtime/helpers/aligned_memory.h"
#include "runtime/gmm_helper/gmm_helper.h"
#include "unit_tests/mocks/mock_gmm_resource_info.h"

using namespace OCLRT;

class GmmFixture {

  public:
    virtual void SetUp() {
    }

    virtual void TearDown() {
    }

    Gmm *getGmm(void *ptr, size_t size) {
        size_t alignedSize = alignSizeWholePage(ptr, size);
        void *alignedPtr = alignUp(ptr, 4096);
        Gmm *gmm;
        gmm = new Gmm;

        EXPECT_NE(gmm, nullptr);

        gmm->resourceParams.Type = RESOURCE_BUFFER;
        gmm->resourceParams.Format = GMM_FORMAT_GENERIC_8BIT;
        gmm->resourceParams.BaseWidth = (uint32_t)alignedSize;
        gmm->resourceParams.BaseHeight = 1;
        gmm->resourceParams.Depth = 1;
        gmm->resourceParams.Usage = GMM_RESOURCE_USAGE_OCL_BUFFER;

        gmm->resourceParams.pExistingSysMem = reinterpret_cast<GMM_VOIDPTR64>(alignedPtr);
        gmm->resourceParams.ExistingSysMemSize = alignedSize;
        gmm->resourceParams.BaseAlignment = 0;

        gmm->resourceParams.Flags.Info.ExistingSysMem = 1;
        gmm->resourceParams.Flags.Info.Linear = 1;
        gmm->resourceParams.Flags.Info.Cacheable = 1;
        gmm->resourceParams.Flags.Gpu.Texture = 1;

        gmm->create();

        EXPECT_NE(gmm->gmmResourceInfo.get(), nullptr);

        return gmm;
    }

    void releaseGmm(Gmm *gmm) {
        delete gmm;
    }
};
