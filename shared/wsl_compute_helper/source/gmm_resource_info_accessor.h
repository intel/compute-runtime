/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "gmm_resource_info_struct.h"

using GmmAccessorBase = GmmLib::GmmResourceInfo;

class GmmResourceInfoWinAccessor : public GmmAccessorBase {
  public:
    GmmResourceInfoWinAccessor() {
    }
    GmmAccessorBase &operator=(const GmmAccessorBase &rhs) {
        GmmAccessorBase::operator=(rhs);
        return *this;
    }

    void set(const GmmResourceInfoCommonStruct &src) {
        this->ClientType = src.ClientType;
        this->Surf = src.Surf;
        this->AuxSurf = src.AuxSurf;
        this->AuxSecSurf = src.AuxSecSurf;

        this->RotateInfo = src.RotateInfo;
        this->ExistingSysMem = src.ExistingSysMem;
        this->SvmAddress = src.SvmAddress;

        this->pPrivateData = src.pPrivateData;

        this->MultiTileArch = src.MultiTileArch;
    }

    void set(const GmmResourceInfoWinStruct &src) {
        this->set(src.GmmResourceInfoCommon);
    }

    void get(GmmResourceInfoCommonStruct &dst) const {
        dst.ClientType = this->ClientType;
        dst.Surf = this->Surf;
        dst.AuxSurf = this->AuxSurf;
        dst.AuxSecSurf = this->AuxSecSurf;

        dst.RotateInfo = this->RotateInfo;
        dst.ExistingSysMem = this->ExistingSysMem;
        dst.SvmAddress = this->SvmAddress;

        dst.pPrivateData = this->pPrivateData;

        dst.MultiTileArch = this->MultiTileArch;
    }

    void get(GmmResourceInfoWinStruct &dst) const {
        this->get(dst.GmmResourceInfoCommon);
    }
};
