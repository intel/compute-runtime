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

#include "mock_gmm.h"

GMM_GLOBAL_CONTEXT *pGmmGlobalContext;
GMM_PLATFORM_INFO *pGlobalPlatformInfo = nullptr;

GMM_CLIENT_CONTEXT *GMM_STDCALL createClientContext(GMM_CLIENT ClientType) {
    return new GMM_CLIENT_CONTEXT(ClientType);
}
void GMM_STDCALL deleteClientContext(GMM_CLIENT_CONTEXT *pGmmClientContext) {
    delete pGmmClientContext;
}
void GMM_STDCALL destroySingletonContext(void) {
    delete pGlobalPlatformInfo;
}
GMM_STATUS GMM_STDCALL createSingletonContext(const PLATFORM Platform,
                                              const SKU_FEATURE_TABLE *pSkuTable,
                                              const WA_TABLE *pWaTable,
                                              const GT_SYSTEM_INFO *pGtSysInfo) {
    pGlobalPlatformInfo = new GMM_PLATFORM_INFO;
    initPlatform(pGlobalPlatformInfo);
    return GMM_SUCCESS;
}
GMM_STATUS GMM_STDCALL OpenGmm(GmmExportEntries *pm_GmmFuncs) {
    pm_GmmFuncs->pfnCreateClientContext = &createClientContext;
    pm_GmmFuncs->pfnCreateSingletonContext = &createSingletonContext;
    pm_GmmFuncs->pfnDeleteClientContext = &deleteClientContext;
    pm_GmmFuncs->pfnDestroySingletonContext = &destroySingletonContext;
    return GMM_SUCCESS;
}

namespace GmmLib {
MEMORY_OBJECT_CONTROL_STATE GmmClientContext::CachePolicyGetMemoryObject(GMM_RESOURCE_INFO *pResInfo, GMM_RESOURCE_USAGE_TYPE Usage) {
    return {};
}
GmmClientContext::GmmClientContext(GMM_CLIENT ClientType) {
}
GmmClientContext::~GmmClientContext() {
}

GMM_PTE_CACHE_CONTROL_BITS GMM_STDCALL GmmClientContext::CachePolicyGetPteType(GMM_RESOURCE_USAGE_TYPE Usage) {
    return {};
}

MEMORY_OBJECT_CONTROL_STATE GMM_STDCALL GmmClientContext::CachePolicyGetOriginalMemoryObject(GMM_RESOURCE_INFO *pResInfo) {
    return {};
}

uint8_t GMM_STDCALL GmmClientContext::CachePolicyIsUsagePTECached(GMM_RESOURCE_USAGE_TYPE Usage) {
    return 0;
}

uint32_t GMM_STDCALL GmmClientContext::CachePolicyGetMaxMocsIndex() {
    return 0;
}

uint32_t GMM_STDCALL GmmClientContext::CachePolicyGetMaxL1HdcMocsIndex() {
    return 0;
}

uint32_t GMM_STDCALL GmmClientContext::CachePolicyGetMaxSpecialMocsIndex(void) {
    return 0;
}

const GMM_CACHE_POLICY_ELEMENT *GMM_STDCALL GmmClientContext::GetCachePolicyUsage() {
    return nullptr;
}

void GMM_STDCALL GmmClientContext::GetCacheSizes(GMM_CACHE_SIZES *pCacheSizes) {
    return;
}

uint8_t GMM_STDCALL GmmClientContext::GetUseGlobalGtt(GMM_HW_COMMAND_STREAMER cs,
                                                      GMM_HW_COMMAND Command,
                                                      D3DDDI_PATCHLOCATIONLIST_DRIVERID *pDriverId) {
    return 0;
}
GMM_CACHE_POLICY_ELEMENT GMM_STDCALL GmmClientContext::GetCachePolicyElement(GMM_RESOURCE_USAGE_TYPE Usage) {
    return {};
}

GMM_CACHE_POLICY_TBL_ELEMENT GMM_STDCALL GmmClientContext::GetCachePolicyTlbElement(uint32_t MocsIdx) {
    return {};
}

GMM_PLATFORM_INFO &GMM_STDCALL GmmClientContext::GetPlatformInfo() {
    return *pGlobalPlatformInfo;
}

uint8_t GMM_STDCALL GmmClientContext::IsPlanar(GMM_RESOURCE_FORMAT Format) {
    return 0;
}

uint8_t GMM_STDCALL GmmClientContext::IsP0xx(GMM_RESOURCE_FORMAT Format) {
    return 0;
}

uint8_t GMM_STDCALL GmmClientContext::IsUVPacked(GMM_RESOURCE_FORMAT Format) {
    return 0;
}

uint8_t GMM_STDCALL GmmClientContext::IsCompressed(GMM_RESOURCE_FORMAT Format) {
    return 0;
}

uint8_t GMM_STDCALL GmmClientContext::IsYUVPacked(GMM_RESOURCE_FORMAT Format) {
    return 0;
}

GMM_STATUS GMM_STDCALL GmmClientContext::GetLogicalTileShape(uint32_t TileMode,
                                                             uint32_t *pWidthInBytes,
                                                             uint32_t *pHeight,
                                                             uint32_t *pDepth) {
    return GMM_SUCCESS;
}

GMM_RESOURCE_INFO *GMM_STDCALL GmmClientContext::CreateResInfoObject(GMM_RESCREATE_PARAMS *pCreateParams) {
    return new GMM_RESOURCE_INFO;
}

GMM_RESOURCE_INFO *GMM_STDCALL GmmClientContext::CopyResInfoObject(GMM_RESOURCE_INFO *pSrcRes) {
    return new GMM_RESOURCE_INFO;
}

void GMM_STDCALL GmmClientContext::ResMemcpy(void *pDst, void *pSrc) {
}

void GMM_STDCALL GmmClientContext::DestroyResInfoObject(GMM_RESOURCE_INFO *pResInfo) {
    delete pResInfo;
}

GMM_PAGETABLE_MGR *GMM_STDCALL GmmClientContext::CreatePageTblMgrObject(
    GMM_DEVICE_CALLBACKS *pDevCb,
    GMM_TRANSLATIONTABLE_CALLBACKS *pTTCB,
    uint32_t TTFlags) {
    return nullptr;
}

GMM_PAGETABLE_MGR *GMM_STDCALL GmmClientContext::CreatePageTblMgrObject(
    GMM_DEVICE_CALLBACKS_INT *pDevCb,
    GMM_TRANSLATIONTABLE_CALLBACKS *pTTCB,
    uint32_t TTFlags) {
    return nullptr;
}

void GMM_STDCALL GmmClientContext::DestroyPageTblMgrObject(GMM_PAGETABLE_MGR *pPageTableMgr) {
}
GMM_RESOURCE_INFO *GMM_STDCALL GmmClientContext::CreateResInfoObject(GMM_RESCREATE_PARAMS *pCreateParams,
                                                                     GmmClientAllocationCallbacks *pAllocCbs) {
    return nullptr;
}

void GMM_STDCALL GmmClientContext::DestroyResInfoObject(GMM_RESOURCE_INFO *pResInfo,
                                                        GmmClientAllocationCallbacks *pAllocCbs) {
}

GMM_PAGETABLE_MGR *GMM_STDCALL GmmClientContext::CreatePageTblMgrObject(
    GMM_DEVICE_CALLBACKS_INT *pDevCb,
    GMM_TRANSLATIONTABLE_CALLBACKS *pTTCB,
    uint32_t TTFlags,
    GmmClientAllocationCallbacks *pAllocCbs) {
    return nullptr;
}

void GMM_STDCALL GmmClientContext::DestroyPageTblMgrObject(GMM_PAGETABLE_MGR *pPageTableMgr,
                                                           GmmClientAllocationCallbacks *pAllocCbs) {
}

extern "C" GMM_CLIENT_CONTEXT *GMM_STDCALL GmmCreateClientContext(GMM_CLIENT ClientType) {
    return nullptr;
}

extern "C" void GMM_STDCALL GmmDeleteClientContext(GMM_CLIENT_CONTEXT *pGmmClientContext) {
}

uint8_t GMM_STDCALL GmmClientContext::ConfigureDeviceAddressSpace(GMM_ESCAPE_HANDLE_EXT hAdapter,
                                                                  GMM_ESCAPE_HANDLE_EXT hDevice,
                                                                  GMM_ESCAPE_FUNC_TYPE_EXT pfnEscape,
                                                                  GMM_GFX_SIZE_T SvmSize,
                                                                  uint8_t FaultableSvm,
                                                                  uint8_t SparseReady,
                                                                  uint8_t BDWL3Coherency,
                                                                  GMM_GFX_SIZE_T SizeOverride,
                                                                  GMM_GFX_SIZE_T SlmGfxSpaceReserve) {
    return 0;
}

uint32_t GmmClientContext::GetSetProcessGfxPartition(GMM_ESCAPE_HANDLE_EXT hAdapter,
                                                     GMM_ESCAPE_HANDLE_EXT hDevice,
                                                     GMM_GFX_PARTITIONING *pProcessGfxPart,
                                                     uint8_t Get,
                                                     GMM_ESCAPE_FUNC_TYPE_EXT pfnEscape) {

    return 0;
}

GMM_GFX_PARTITIONING GMM_STDCALL GmmClientContext::OverrideGfxPartition(GMM_GFX_PARTITIONING *GfxPartition,
                                                                        GMM_ESCAPE_HANDLE_EXT hAdapter,
                                                                        GMM_ESCAPE_HANDLE_EXT hDevice,
                                                                        GMM_ESCAPE_FUNC_TYPE_EXT pfnEscape) {
    return {};
}

uint8_t GMM_STDCALL GmmClientContext::EnhancedBufferMap(GMM_ESCAPE_HANDLE_EXT hAdapter,
                                                        GMM_ESCAPE_HANDLE_EXT hDevice,
                                                        GMM_ESCAPE_FUNC_TYPE_EXT pfnEscape,
                                                        D3DKMT_HANDLE hOriginalAllocation,
                                                        GMM_ENHANCED_BUFFER_INFO *pEnhancedBufferInfo[GMM_MAX_DISPLAYS]) {
    return 0;
}

GMM_GFX_ADDRESS GMM_STDCALL GmmClientContext::GetHeap32Base(GMM_ESCAPE_HANDLE_EXT hAdapter,
                                                            GMM_ESCAPE_HANDLE_EXT hDevice,
                                                            uint32_t *pHeapSize,
                                                            GMM_ESCAPE_FUNC_TYPE_EXT pfnEscape) {

    return {};
}

GMM_GFX_ADDRESS GMM_STDCALL GmmClientContext::GetTrashPageGfxAddress(GMM_ESCAPE_HANDLE_EXT hAdapter,
                                                                     GMM_ESCAPE_HANDLE_EXT hDevice,
                                                                     GMM_ESCAPE_FUNC_TYPE_EXT pfnEscape) {
    return {};
}

GMM_HEAP *GMM_STDCALL GmmClientContext::UmSetupHeap(GMM_ESCAPE_HANDLE_EXT hAdapter,
                                                    GMM_ESCAPE_HANDLE_EXT hDevice,
                                                    GMM_GFX_ADDRESS GfxAddress,
                                                    GMM_GFX_SIZE_T Size,
                                                    uint32_t Flags,
                                                    GMM_ESCAPE_FUNC_TYPE_EXT pfnEscape) {

    return nullptr;
}

GMM_STATUS GMM_STDCALL GmmClientContext::UmDestroypHeap(GMM_ESCAPE_HANDLE_EXT hAdapter,
                                                        GMM_ESCAPE_HANDLE_EXT hDevice,
                                                        GMM_HEAP **pHeapObj,
                                                        GMM_ESCAPE_FUNC_TYPE_EXT pfnEscape) {
    return GMM_SUCCESS;
}

GMM_GFX_ADDRESS GMM_STDCALL GmmClientContext::AllocateHeapVA(GMM_HEAP *pHeapObj,
                                                             GMM_GFX_SIZE_T AllocSize) {

    return {};
}

GMM_STATUS GMM_STDCALL GmmClientContext::FreeHeapVA(GMM_HEAP *pHeapObj,
                                                    GMM_GFX_ADDRESS AllocVA,
                                                    GMM_GFX_SIZE_T AllocSize) {
    return GMM_SUCCESS;
}

void *GmmClientContext::__GetSharedHeapObject(GMM_ESCAPE_HANDLE_EXT hAdapter,
                                              GMM_ESCAPE_HANDLE_EXT hDevice,
                                              GMM_ESCAPE_FUNC_TYPE_EXT pfnEscape) {
    return nullptr;
}

uint32_t GmmClientContext::__SetSharedHeapObject(GMM_ESCAPE_HANDLE_EXT hAdapter,
                                                 GMM_ESCAPE_HANDLE_EXT hDevice,
                                                 void **pHeapObj,
                                                 GMM_ESCAPE_FUNC_TYPE_EXT pfnEscape) {
    return 0;
}

GMM_GFX_ADDRESS GMM_STDCALL GmmClientContext::GetSLMaddressRange(GMM_ESCAPE_HANDLE_EXT hAdapter,
                                                                 GMM_ESCAPE_HANDLE_EXT hDevice,
                                                                 GMM_GFX_SIZE_T Size,
                                                                 GMM_ESCAPE_FUNC_TYPE_EXT pfnEscape) {
    return {};
}

uint8_t GMM_STDCALL GmmClientContext::ResDestroySvmAllocation(GMM_ESCAPE_HANDLE_EXT hAdapter,
                                                              GMM_ESCAPE_HANDLE_EXT hDevice,
                                                              D3DKMT_HANDLE hAllocation,
                                                              GMM_ESCAPE_FUNC_TYPE_EXT pfnEscape) {
    return 0;
}

void GMM_STDCALL GmmClientContext::GetD3dSwizzlePattern(const SWIZZLE_DESCRIPTOR *pGmmSwizzle,
                                                        uint8_t MSAA,
                                                        D3DWDDM2_0DDI_SWIZZLE_PATTERN_DESC *pD3dSwizzle) {
}

void GMM_STDCALL GmmClientContext::GetD3dSwizzlePattern(const SWIZZLE_DESCRIPTOR *pGmmSwizzle,
                                                        uint8_t MSAA,
                                                        D3DWDDM2_2DDI_SWIZZLE_PATTERN_DESC *pD3dSwizzle) {
}

void GMM_STDCALL GmmClientContext::GetD3dSwizzlePattern(const SWIZZLE_DESCRIPTOR *pGmmSwizzle,
                                                        uint8_t MSAA,
                                                        D3D12DDI_SWIZZLE_PATTERN_DESC_0022 *pD3dSwizzle) {
}

void GMM_STDCALL GmmClientContext::GetD3dSwizzlePattern(const SWIZZLE_DESCRIPTOR *pGmmSwizzle,
                                                        uint8_t MSAA,
                                                        D3D12DDI_SWIZZLE_PATTERN_DESC_0004 *pD3dSwizzle) {
}

GMM_GFX_ADDRESS GMM_STDCALL GmmClientContext::PigmsReserveGpuVA(GMM_ESCAPE_HANDLE_EXT hAdapter,
                                                                GMM_ESCAPE_HANDLE_EXT hDevice,
                                                                GMM_GFX_SIZE_T Size,
                                                                GMM_ESCAPE_FUNC_TYPE_EXT pfnEscape) {
    return 0;
}

uint8_t GMM_STDCALL GmmClientContext::PigmsMapGpuVA(GMM_ESCAPE_HANDLE_EXT hAdapter,
                                                    GMM_ESCAPE_HANDLE_EXT hDevice,
                                                    GMM_GFX_ADDRESS GfxAddress,
                                                    D3DKMT_HANDLE hAllocation,
                                                    GMM_RESOURCE_INFO *pGmmResInfo,
                                                    GMM_ESCAPE_FUNC_TYPE_EXT pfnEscape) {
    return 0;
}

uint8_t GMM_STDCALL GmmClientContext::PigmsFreeGpuVa(GMM_ESCAPE_HANDLE_EXT hAdapter,
                                                     GMM_ESCAPE_HANDLE_EXT hDevice,
                                                     GMM_GFX_ADDRESS GfxAddress,
                                                     GMM_GFX_SIZE_T Size,
                                                     GMM_ESCAPE_FUNC_TYPE_EXT pfnEscape) {
    return 0;
}

GMM_STATUS GMM_STDCALL GmmClientContext::ResUpdateAfterSharedOpen(HANDLE hAdapter,
                                                                  HANDLE hDevice,
                                                                  D3DKMT_HANDLE hAllocation,
                                                                  GMM_RESOURCE_INFO *pGmmResInfo,
                                                                  GMM_ESCAPE_FUNC_TYPE_EXT pfnEscape) {
    return GMM_SUCCESS;
}

uint8_t GMM_STDCALL GmmClientContext::CreateTiledResource(GMM_HANDLE_EXT hAdapter,
                                                          GMM_HANDLE_EXT hDevice,
                                                          GMM_UMD_SYNCCONTEXT *pUmdContext,
                                                          GMM_TRANSLATIONTABLE_CALLBACKS *pTrTtCallbacks,
                                                          GMM_RESOURCE_INFO *pGmmResource,
                                                          GMM_ESCAPE_FUNC_TYPE_EXT pfnEscape,
                                                          uint32_t NullHw) {
    return 0;
}

uint8_t GMM_STDCALL GmmClientContext::DestroyTiledResource(GMM_HANDLE_EXT hAdapter,
                                                           GMM_HANDLE_EXT hDevice,
                                                           GMM_UMD_SYNCCONTEXT *pUmdContext,
                                                           GMM_TRANSLATIONTABLE_CALLBACKS *pTrTtCallbacks,
                                                           GMM_RESOURCE_INFO *pGmmResource,
                                                           GMM_ESCAPE_FUNC_TYPE_EXT pfnEscape,
                                                           uint32_t NullHw) {
    return 0;
}

GMM_STATUS GMM_STDCALL GmmClientContext::UpdateTiledResourceMapping(GMM_HANDLE_EXT hAdapter,
                                                                    GMM_HANDLE_EXT hDevice,
                                                                    GMM_UPDATE_TILE_MAPPINGS_INT Mappings,
                                                                    uint8_t UseWDDM20,
                                                                    GMM_UMD_SYNCCONTEXT *pUmdContext,
                                                                    GMM_TRANSLATIONTABLE_CALLBACKS *CmdBufCallbacks,
                                                                    GMM_ESCAPE_FUNC_TYPE_EXT pfnEscape,
                                                                    GMM_DEVICE_CALLBACKS_INT *DeviceCallbacks,
                                                                    uint32_t NullHw,
                                                                    uint8_t NewPool) {
    return GMM_SUCCESS;
}

GMM_STATUS GMM_STDCALL GmmClientContext::UpdateSparseResourceOpaqueMapping(GMM_HANDLE_EXT hAdapter,
                                                                           GMM_HANDLE_EXT hDevice,
                                                                           GMM_UMD_SYNCCONTEXT *pUmdContext,
                                                                           GMM_TRANSLATIONTABLE_CALLBACKS *CmdBufCallbacks,
                                                                           GMM_ESCAPE_FUNC_TYPE_EXT pfnEscape,
                                                                           uint8_t UseWDDM20,
                                                                           uint32_t NumMappings,
                                                                           GMM_UPDATE_SPARSE_RESOURCE_OPAQUE_MAPPINGS *pMappings) {
    return GMM_SUCCESS;
}

GMM_STATUS GMM_STDCALL GmmClientContext::CopyTileMappings(GMM_HANDLE_EXT hAdapter,
                                                          GMM_HANDLE_EXT hDevice,
                                                          GMM_COPY_TILE_MAPPINGS_INT Mappings,
                                                          uint8_t UseWDDM20,
                                                          GMM_UMD_SYNCCONTEXT *pUmdContext,
                                                          GMM_TRANSLATIONTABLE_CALLBACKS *CmdBufCallbacks,
                                                          GMM_ESCAPE_FUNC_TYPE_EXT pfnEscape,
                                                          GMM_DEVICE_CALLBACKS_INT *DeviceCallbacks,
                                                          uint32_t NullHw,
                                                          uint8_t CheckTilePoolAllocs) {
    return GMM_SUCCESS;
}

uint64_t GMM_STDCALL GmmClientContext::GetInternalGpuVaRangeLimit() {
    return 0;
}

bool GmmResourceInfoCommon::IsPresentableformat() {
    return false;
}
void GmmResourceInfoCommon::GetGenericRestrictions(__GMM_BUFFER_TYPE *pBuff) {
}
__GMM_BUFFER_TYPE *GmmResourceInfoCommon::GetBestRestrictions(__GMM_BUFFER_TYPE *pFirstBuffer, const __GMM_BUFFER_TYPE *pSecondBuffer) {
    return nullptr;
}
bool GmmResourceInfoCommon::CopyClientParams(GMM_RESCREATE_PARAMS &CreateParams) {
    return false;
}
bool GmmResourceInfoCommon::RedescribePlanes() {
    return false;
}
bool GmmResourceInfoCommon::ReAdjustPlaneProperties(bool IsAuxSurf) {
    return false;
}

const GMM_PLATFORM_INFO &GmmResourceInfoCommon::GetPlatformInfo() {
    return *pGlobalPlatformInfo;
}
GMM_STATUS GMM_STDCALL GmmResourceInfoCommon::Create(Context &GmmLibContext, GMM_RESCREATE_PARAMS &CreateParams) {
    return GMM_SUCCESS;
}
uint8_t GMM_STDCALL GmmResourceInfoCommon::ValidateParams() {
    return 0;
}
GMM_STATUS GMM_STDCALL GmmResourceInfoCommon::Create(GMM_RESCREATE_PARAMS &CreateParams) {

    return GMM_SUCCESS;
}
void GMM_STDCALL GmmResourceInfoCommon::GetRestrictions(__GMM_BUFFER_TYPE &Restrictions) {
}
uint32_t GMM_STDCALL GmmResourceInfoCommon::GetPaddedWidth(uint32_t MipLevel) {
    return 0;
}
uint32_t GMM_STDCALL GmmResourceInfoCommon::GetPaddedHeight(uint32_t MipLevel) {
    return 0;
}
uint32_t GMM_STDCALL GmmResourceInfoCommon::GetPaddedPitch(uint32_t MipLevel) {
    return 0;
}
uint32_t GMM_STDCALL GmmResourceInfoCommon::GetQPitch() {
    return 0;
}
GMM_STATUS GMM_STDCALL GmmResourceInfoCommon::GetOffset(GMM_REQ_OFFSET_INFO &ReqInfo) {

    return GMM_SUCCESS;
}
uint8_t GMM_STDCALL GmmResourceInfoCommon::CpuBlt(GMM_RES_COPY_BLT *pBlt) {
    return 0;
}
uint8_t GMM_STDCALL GmmResourceInfoCommon::GetMappingSpanDesc(GMM_GET_MAPPING *pMapping) {
    return 0;
}
uint8_t GMM_STDCALL GmmResourceInfoCommon::Is64KBPageSuitable() {
    return 0;
}
void GMM_STDCALL GmmResourceInfoCommon::GetTiledResourceMipPacking(uint32_t *pNumPackedMips,
                                                                   uint32_t *pNumTilesForPackedMips) {}
uint32_t GMM_STDCALL GmmResourceInfoCommon::GetPackedMipTailStartLod() {
    return 0;
}
bool GMM_STDCALL GmmResourceInfoCommon::IsMipRCCAligned(uint8_t &MisAlignedLod) {
    return false;
}
uint8_t GMM_STDCALL GmmResourceInfoCommon::GetDisplayFastClearSupport() {
    return 0;
}
uint8_t GMM_STDCALL GmmResourceInfoCommon::GetDisplayCompressionSupport() {
    return 0;
}
uint32_t GMM_STDCALL GmmResourceInfoCommon::GetCompressionBlockWidth() {
    return 0;
}
uint32_t GMM_STDCALL GmmResourceInfoCommon::GetCompressionBlockHeight() {
    return 0;
}
uint32_t GMM_STDCALL GmmResourceInfoCommon::GetCompressionBlockDepth() {
    return 0;
}
uint8_t GMM_STDCALL GmmResourceInfoCommon::IsArraySpacingSingleLod() {
    return 0;
}
uint8_t GMM_STDCALL GmmResourceInfoCommon::IsASTC() {
    return 0;
}
MEMORY_OBJECT_CONTROL_STATE GMM_STDCALL GmmResourceInfoCommon::GetMOCS() {
    return {};
}
uint32_t GMM_STDCALL GmmResourceInfoCommon::GetStdTilingModeExtSurfaceState() {
    return 0;
}
GMM_SURFACESTATE_FORMAT GMM_STDCALL GmmResourceInfoCommon::GetResourceFormatSurfaceState() {
    return {};
}
GMM_GFX_SIZE_T GMM_STDCALL GmmResourceInfoCommon::GetMipWidth(uint32_t MipLevel) {
    return {};
}
uint32_t GMM_STDCALL GmmResourceInfoCommon::GetMipHeight(uint32_t MipLevel) {
    return 0;
}
uint32_t GMM_STDCALL GmmResourceInfoCommon::GetMipDepth(uint32_t MipLevel) {
    return 0;
}
GMM_STATUS GMM_STDCALL GmmResourceInfoWin::GetOffsetFor64KBTiles(GMM_REQ_OFFSET_INFO &ReqInfo) {
    return GMM_SUCCESS;
}
uint8_t GMM_STDCALL GmmResourceInfoWin::ApplyExistingSysMem(void *pExistingSysMem, GMM_GFX_SIZE_T ExistingSysMemSize) {
    return 0;
}
uint8_t GMM_STDCALL GmmResourceInfoWin::IsPredictedGlobalAliasingParticipant() {
    return 0;
}
uint8_t GMM_STDCALL GmmResourceInfoWin::IsSurfaceFaultable() {
    return 0;
}
uint32_t GMM_STDCALL GmmResourceInfoWin::GetRenderPitchIn64KBTiles() {
    return 0;
}
uint8_t GMM_STDCALL GmmResourceInfoWin::UpdateCacheability(GMM_ESCAPE_HANDLE_EXT hAdapter,
                                                           GMM_ESCAPE_HANDLE_EXT hDevice,
                                                           D3DKMT_HANDLE hAllocation,
                                                           GMM_GPU_CACHE_SETTINGS &CacheSettings,
                                                           GMM_ESCAPE_FUNC_TYPE_EXT pfnEscape) {
    return 0;
}
uint8_t GMM_STDCALL GmmResourceInfoWin::Alias32bit(GMM_ESCAPE_HANDLE_EXT hAdapter,
                                                   GMM_ESCAPE_HANDLE_EXT hDevice,
                                                   D3DKMT_HANDLE hAllocation,
                                                   uint32_t Size,
                                                   uint32_t *pAliasAddress,
                                                   GMM_ESCAPE_FUNC_TYPE_EXT pfnEscape) {
    return 0;
}
uint8_t GMM_STDCALL GmmResourceInfoWin::KmdGetSetHardwareProtection(GMM_ESCAPE_HANDLE_EXT hAdapter,
                                                                    GMM_ESCAPE_HANDLE_EXT hDevice,
                                                                    D3DKMT_HANDLE hAllocation,
                                                                    uint8_t SetIsEncrypted,
                                                                    uint8_t *pGetIsEncrypted,
                                                                    GMM_ESCAPE_FUNC_TYPE_EXT pfnEscape) {
    return 0;
}

uint8_t GMM_STDCALL GmmResourceInfoWin::KmdSetPavpStoutOrIsolatedEncryptionForDisplay(GMM_ESCAPE_HANDLE_EXT hAdapter,
                                                                                      GMM_ESCAPE_HANDLE_EXT hDevice,
                                                                                      D3DKMT_HANDLE hAllocation,
                                                                                      uint8_t bPavpModeIsStoutOrIsolatedDecode,
                                                                                      uint32_t uiPavpSessionId,
                                                                                      GMM_ESCAPE_FUNC_TYPE_EXT pfnEscape) {
    return 0;
}

uint32_t GMM_STDCALL GmmResourceInfoWin::KmdGetSetCpSurfTag(GMM_ESCAPE_HANDLE_EXT hAdapter,
                                                            GMM_ESCAPE_HANDLE_EXT hDevice,
                                                            D3DKMT_HANDLE hAllocation,
                                                            uint8_t IsSet,
                                                            uint32_t CpTag,
                                                            GMM_ESCAPE_FUNC_TYPE_EXT pfnEscape) {
    return 0;
}
bool GmmResourceInfoWin::CopyClientParams(GMM_RESCREATE_PARAMS &CreateParams) {
    return false;
}
GMM_GFX_ADDRESS GmmPageTableMgr::GetTRL3TableAddr() {
    return {};
}
GMM_GFX_ADDRESS GmmPageTableMgr::GetAuxL3TableAddr() {
    return {};
}
GMM_STATUS GmmPageTableMgr::InitContextAuxTableRegister(HANDLE initialBBHandle, GMM_ENGINE_TYPE engType) {
    return GMM_SUCCESS;
}
GMM_STATUS GmmPageTableMgr::InitContextTRTableRegister(HANDLE initialBBHandle, GMM_ENGINE_TYPE engType) {
    return GMM_SUCCESS;
}
void GmmPageTableMgr::InitGpuBBQueueCallbacks(GMM_TRANSLATIONTABLE_CALLBACKS *TTCB) {}

HRESULT GmmPageTableMgr::ReserveTRGpuVirtualAddress(GMM_UMD_SYNCCONTEXT *, D3DDDI_RESERVEGPUVIRTUALADDRESS *, uint8_t DoNotWait) {
    return 0;
}

HRESULT GmmPageTableMgr::MapTRGpuVirtualAddress(GMM_MAPTRGPUVIRTUALADDRESS *ReserveGpuVa, uint8_t DoNotWait) {
    return 0;
}
HRESULT GmmPageTableMgr::UpdateTRGpuVirtualAddress(GMM_UMD_SYNCCONTEXT **, const GMM_UPDATETRGPUVIRTUALADDRESS_INT *, uint8_t DoNotWait) {
    return 0;
}
HRESULT GmmPageTableMgr::FreeTRGpuVirtualAddress(GMM_UMD_SYNCCONTEXT *, const GMM_DDI_FREEGPUVIRTUALADDRSS_INT *, uint8_t DoNotWait) {
    return 0;
}
GMM_STATUS GmmPageTableMgr::UpdateAuxTable(const GMM_DDI_UPDATEAUXTABLE *) {
    return GMM_SUCCESS;
}
void GmmPageTableMgr::EvictPageTablePool(D3DKMT_HANDLE *BBQHandles, int NumBBFenceObj) {}
void GmmPageTableMgr::__ReleaseUnusedPool(GMM_UMD_SYNCCONTEXT *UmdContext) {}
GMM_PAGETABLEPool *GmmPageTableMgr::__GetFreePoolNode(uint32_t *FreePoolNodeIdx, POOL_TYPE PoolType) { return nullptr; }
GmmPageTableMgr::~GmmPageTableMgr() {}
} // namespace GmmLib
uint8_t GMM_STDCALL GmmIsPlanar(GMM_RESOURCE_FORMAT Format) {
    return 0;
}
const GMM_PLATFORM_INFO *GMM_STDCALL GmmGetPlatformInfo(GMM_GLOBAL_CONTEXT *pGmmLibContext) {
    return nullptr;
}
