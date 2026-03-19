/*
 * Copyright (C) 2018-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/client_context/gmm_client_context.h"
#include "shared/source/gmm_helper/gmm.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/gmm_helper/page_table_mngr.h"
#include "shared/source/gmm_helper/resource_info.h"

namespace NEO {
bool GmmPageTableMngr::updateAuxTable(uint64_t gpuVa, Gmm *gmm, bool map) {
    GMM_DDI_UPDATEAUXTABLE ddiUpdateAuxTable = {};
    ddiUpdateAuxTable.BaseGpuVA = gpuVa;
    ddiUpdateAuxTable.BaseResInfo = gmm->gmmResourceInfo->peekGmmResourceInfo();
    ddiUpdateAuxTable.DoNotWait = true;
    ddiUpdateAuxTable.Map = map;

    return updateAuxTable(&ddiUpdateAuxTable);
}

bool GmmPageTableMngr::initPageTableManagerRegisters(void *csrHandle) {
    return initContextAuxTableRegister(csrHandle, GMM_ENGINE_TYPE::ENGINE_TYPE_RCS);
}

bool GmmPageTableMngr::updateAuxTable(const GMM_DDI_UPDATEAUXTABLE *ddiUpdateAuxTable) {
    return pageTableManager->UpdateAuxTable(ddiUpdateAuxTable) == GMM_STATUS::GMM_SUCCESS;
}

bool GmmPageTableMngr::initContextAuxTableRegister(HANDLE initialBBHandle, GMM_ENGINE_TYPE engineType) {
    return pageTableManager->InitContextAuxTableRegister(initialBBHandle, engineType) == GMM_STATUS::GMM_SUCCESS;
}

} // namespace NEO
