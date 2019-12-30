/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/gmm_helper/gmm_helper.h"
#include "core/gmm_helper/page_table_mngr.h"
#include "core/gmm_helper/resource_info.h"
#include "runtime/gmm_helper/gmm.h"

#include "gmm_client_context.h"

namespace NEO {
GmmPageTableMngr::~GmmPageTableMngr() {
    if (clientContext) {
        clientContext->DestroyPageTblMgrObject(pageTableManager);
    }
}

bool GmmPageTableMngr::updateAuxTable(uint64_t gpuVa, Gmm *gmm, bool map) {
    GMM_DDI_UPDATEAUXTABLE ddiUpdateAuxTable = {};
    ddiUpdateAuxTable.BaseGpuVA = gpuVa;
    ddiUpdateAuxTable.BaseResInfo = gmm->gmmResourceInfo->peekHandle();
    ddiUpdateAuxTable.DoNotWait = true;
    ddiUpdateAuxTable.Map = map ? 1u : 0u;

    return updateAuxTable(&ddiUpdateAuxTable) == GMM_STATUS::GMM_SUCCESS;
}

bool GmmPageTableMngr::initPageTableManagerRegisters(void *csrHandle) {
    auto status = initContextAuxTableRegister(csrHandle, GMM_ENGINE_TYPE::ENGINE_TYPE_RCS);
    return status == GMM_SUCCESS;
}

} // namespace NEO
