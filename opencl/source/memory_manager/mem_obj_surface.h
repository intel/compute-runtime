/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/memory_manager/surface.h"

namespace NEO {

class MemObj;

class MemObjSurface : public Surface {
  public:
    MemObjSurface(MemObj *memObj);
    ~MemObjSurface() override;

    void makeResident(CommandStreamReceiver &csr) override;

    Surface *duplicate() override;

  protected:
    class MemObj *memObj;
};

} // namespace NEO
