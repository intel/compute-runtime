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
#include <map>
#include "runtime/helpers/aligned_memory.h"
#include "runtime/memory_manager/graphics_allocation.h"
#include "runtime/memory_manager/host_ptr_defines.h"

namespace OCLRT {

typedef std::map<const void *, FragmentStorage> HostPtrFragmentsContainer;

class HostPtrManager {
  public:
    static AllocationRequirements getAllocationRequirements(const void *inputPtr, size_t size);
    OsHandleStorage populateAlreadyAllocatedFragments(AllocationRequirements &requirements, CheckedFragments *checkedFragments);
    void storeFragment(FragmentStorage &fragment);
    void storeFragment(AllocationStorageData &storageData);

    void releaseHandleStorage(OsHandleStorage &fragments);
    bool releaseHostPtr(const void *ptr);

    FragmentStorage *getFragment(const void *inputPtr);
    size_t getFragmentCount() { return partialAllocations.size(); }
    FragmentStorage *getFragmentAndCheckForOverlaps(const void *inputPtr, size_t size, OverlapStatus &overlappingStatus);

  private:
    std::map<const void *, FragmentStorage>::iterator findElement(const void *ptr);

    HostPtrFragmentsContainer partialAllocations;
    std::mutex allocationsMutex;
};
} // namespace OCLRT
