#define WIN32_LEAN_AND_MEAN
#include "d3dhooks.hpp"
#include "ass.hpp"
#include "config.hpp"
#include "mem.hpp"
#include "ui.hpp"
#include "video.hpp"
#include <Windows.h>
#include <backends/imgui_impl_dx9.h>
#include <backends/imgui_impl_win32.h>
#include <d3d9.h>
#include <spdlog/spdlog.h>

extern HWND hwnd;

static bool d3d9_need_pixelated() {
    auto& cfg = conf::get();
    return ui::is_processing() ? cfg.ui_pixel_filter : cfg.pixel_filter;
}

static HRESULT(WINAPI* DirectDrawCreateO)(void* lpGUID, void** lplpDD, void* pUnkOuter);
static HRESULT WINAPI DirectDrawCreateH(void* lpGUID, void** lplpDD, void* pUnkOuter) {
    spdlog::warn("The game is using DirectDraw, forcing custom window");
    ASS(conf::get().render_type == conf::RenderType::None);
    conf::get().render_type = conf::RenderType::DDRAW;
    return DirectDrawCreateO(lpGUID, lplpDD, pUnkOuter);
}

// IDirect3DDevice9 proxy hook
class ID3D9Proxy : public IDirect3DDevice9 {
    IDirect3DDevice9* pDev;

public:
    ID3D9Proxy(IDirect3DDevice9* pReal) : pDev(pReal) {}
    virtual ~ID3D9Proxy() {}
    STDMETHOD(QueryInterface)(REFIID riid, void** ppvObj) override {
        return pDev->QueryInterface(riid, ppvObj);
    }
    STDMETHOD_(ULONG, AddRef)() override { return pDev->AddRef(); }
    STDMETHOD_(ULONG, Release)() override {
        ULONG count = pDev->Release();
        // Don't forget to release us
        if (count == 0)
            delete this;
        return count;
    }
    STDMETHOD(TestCooperativeLevel)() override { return pDev->TestCooperativeLevel(); }
    STDMETHOD_(UINT, GetAvailableTextureMem)() override { return pDev->GetAvailableTextureMem(); }
    STDMETHOD(EvictManagedResources)() override { return pDev->EvictManagedResources(); }
    STDMETHOD(GetDirect3D)(IDirect3D9** ppD3D9) override { return pDev->GetDirect3D(ppD3D9); }
    STDMETHOD(GetDeviceCaps)(D3DCAPS9* pCaps) override { return pDev->GetDeviceCaps(pCaps); }
    STDMETHOD(GetDisplayMode)(UINT iSwapChain, D3DDISPLAYMODE* pMode) override {
        return pDev->GetDisplayMode(iSwapChain, pMode);
    }
    STDMETHOD(GetCreationParameters)(D3DDEVICE_CREATION_PARAMETERS* pParameters) override {
        return pDev->GetCreationParameters(pParameters);
    }
    STDMETHOD(SetCursorProperties)(UINT XHotSpot, UINT YHotSpot,
                                   IDirect3DSurface9* pCursorBitmap) override {
        return pDev->SetCursorProperties(XHotSpot, YHotSpot, pCursorBitmap);
    }
    STDMETHOD_(void, SetCursorPosition)(int X, int Y, DWORD Flags) override {
        pDev->SetCursorPosition(X, Y, Flags);
    }
    STDMETHOD_(BOOL, ShowCursor)(BOOL bShow) override { return pDev->ShowCursor(bShow); }
    STDMETHOD(CreateAdditionalSwapChain)(D3DPRESENT_PARAMETERS* pPresentationParameters,
                                         IDirect3DSwapChain9** pSwapChain) override {
        return pDev->CreateAdditionalSwapChain(pPresentationParameters, pSwapChain);
    }
    STDMETHOD(GetSwapChain)(UINT iSwapChain, IDirect3DSwapChain9** pSwapChain) override {
        return pDev->GetSwapChain(iSwapChain, pSwapChain);
    }
    STDMETHOD_(UINT, GetNumberOfSwapChains)() override { return pDev->GetNumberOfSwapChains(); }
    STDMETHOD(Reset)(D3DPRESENT_PARAMETERS* pPresentationParameters) {
        spdlog::debug("IDirect3DDevice9->Reset");
        // Invalidate ImGui
        ImGui_ImplDX9_InvalidateDeviceObjects();
        HRESULT hr = pDev->Reset(pPresentationParameters);
        if (SUCCEEDED(hr))
            ImGui_ImplDX9_CreateDeviceObjects();
        return hr;
    }
    STDMETHOD(Present)(const RECT* pSourceRect, const RECT* pDestRect, HWND hDestWindowOverride,
                       const RGNDATA* pDirtyRegion) override {
        return pDev->Present(pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion);
    }
    STDMETHOD(GetBackBuffer)(UINT iSwapChain, UINT iBackBuffer, D3DBACKBUFFER_TYPE Type,
                             IDirect3DSurface9** ppBackBuffer) override {
        return pDev->GetBackBuffer(iSwapChain, iBackBuffer, Type, ppBackBuffer);
    }
    STDMETHOD(GetRasterStatus)(UINT iSwapChain, D3DRASTER_STATUS* pRasterStatus) override {
        return pDev->GetRasterStatus(iSwapChain, pRasterStatus);
    }
    STDMETHOD(SetDialogBoxMode)(BOOL bEnableDialogs) override {
        return pDev->SetDialogBoxMode(bEnableDialogs);
    }
    STDMETHOD_(void, SetGammaRamp)(UINT iSwapChain, DWORD Flags,
                                   const D3DGAMMARAMP* pRamp) override {
        pDev->SetGammaRamp(iSwapChain, Flags, pRamp);
    }
    STDMETHOD_(void, GetGammaRamp)(UINT iSwapChain, D3DGAMMARAMP* pRamp) override {
        pDev->GetGammaRamp(iSwapChain, pRamp);
    }
    STDMETHOD(CreateTexture)(UINT Width, UINT Height, UINT Levels, DWORD Usage, D3DFORMAT Format,
                             D3DPOOL Pool, IDirect3DTexture9** ppTexture,
                             HANDLE* pSharedHandle) override {
        return pDev->CreateTexture(Width, Height, Levels, Usage, Format, Pool, ppTexture,
                                   pSharedHandle);
    }
    STDMETHOD(CreateVolumeTexture)(UINT Width, UINT Height, UINT Depth, UINT Levels, DWORD Usage,
                                   D3DFORMAT Format, D3DPOOL Pool,
                                   IDirect3DVolumeTexture9** ppVolumeTexture,
                                   HANDLE* pSharedHandle) override {
        return pDev->CreateVolumeTexture(Width, Height, Depth, Levels, Usage, Format, Pool,
                                         ppVolumeTexture, pSharedHandle);
    }
    STDMETHOD(CreateCubeTexture)(UINT EdgeLength, UINT Levels, DWORD Usage, D3DFORMAT Format,
                                 D3DPOOL Pool, IDirect3DCubeTexture9** ppCubeTexture,
                                 HANDLE* pSharedHandle) override {
        return pDev->CreateCubeTexture(EdgeLength, Levels, Usage, Format, Pool, ppCubeTexture,
                                       pSharedHandle);
    }
    STDMETHOD(CreateVertexBuffer)(UINT Length, DWORD Usage, DWORD FVF, D3DPOOL Pool,
                                  IDirect3DVertexBuffer9** ppVertexBuffer,
                                  HANDLE* pSharedHandle) override {
        return pDev->CreateVertexBuffer(Length, Usage, FVF, Pool, ppVertexBuffer, pSharedHandle);
    }
    STDMETHOD(CreateIndexBuffer)(UINT Length, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool,
                                 IDirect3DIndexBuffer9** ppIndexBuffer,
                                 HANDLE* pSharedHandle) override {
        return pDev->CreateIndexBuffer(Length, Usage, Format, Pool, ppIndexBuffer, pSharedHandle);
    }
    STDMETHOD(CreateRenderTarget)(UINT Width, UINT Height, D3DFORMAT Format,
                                  D3DMULTISAMPLE_TYPE MultiSample, DWORD MultisampleQuality,
                                  BOOL Lockable, IDirect3DSurface9** ppSurface,
                                  HANDLE* pSharedHandle) override {
        return pDev->CreateRenderTarget(Width, Height, Format, MultiSample, MultisampleQuality,
                                        Lockable, ppSurface, pSharedHandle);
    }
    STDMETHOD(CreateDepthStencilSurface)(UINT Width, UINT Height, D3DFORMAT Format,
                                         D3DMULTISAMPLE_TYPE MultiSample, DWORD MultisampleQuality,
                                         BOOL Discard, IDirect3DSurface9** ppSurface,
                                         HANDLE* pSharedHandle) override {
        return pDev->CreateDepthStencilSurface(Width, Height, Format, MultiSample,
                                               MultisampleQuality, Discard, ppSurface,
                                               pSharedHandle);
    }
    STDMETHOD(UpdateSurface)(IDirect3DSurface9* pSourceSurface, const RECT* pSourceRect,
                             IDirect3DSurface9* pDestinationSurface,
                             const POINT* pDestPoint) override {
        return pDev->UpdateSurface(pSourceSurface, pSourceRect, pDestinationSurface, pDestPoint);
    }
    STDMETHOD(UpdateTexture)(IDirect3DBaseTexture9* pSourceTexture,
                             IDirect3DBaseTexture9* pDestinationTexture) override {
        return pDev->UpdateTexture(pSourceTexture, pDestinationTexture);
    }
    STDMETHOD(GetRenderTargetData)(IDirect3DSurface9* pRenderTarget,
                                   IDirect3DSurface9* pDestSurface) override {
        return pDev->GetRenderTargetData(pRenderTarget, pDestSurface);
    }
    STDMETHOD(GetFrontBufferData)(UINT iSwapChain, IDirect3DSurface9* pDestSurface) override {
        return pDev->GetFrontBufferData(iSwapChain, pDestSurface);
    }
    STDMETHOD(StretchRect)(IDirect3DSurface9* pSourceSurface, const RECT* pSourceRect,
                           IDirect3DSurface9* pDestSurface, const RECT* pDestRect,
                           D3DTEXTUREFILTERTYPE Filter) override {
        return pDev->StretchRect(pSourceSurface, pSourceRect, pDestSurface, pDestRect,
                                 d3d9_need_pixelated() ? D3DTEXF_POINT : Filter);
    }
    STDMETHOD(ColorFill)(IDirect3DSurface9* pSurface, const RECT* pRect, D3DCOLOR color) override {
        return pDev->ColorFill(pSurface, pRect, color);
    }
    STDMETHOD(CreateOffscreenPlainSurface)(UINT Width, UINT Height, D3DFORMAT Format, D3DPOOL Pool,
                                           IDirect3DSurface9** ppSurface,
                                           HANDLE* pSharedHandle) override {
        return pDev->CreateOffscreenPlainSurface(Width, Height, Format, Pool, ppSurface,
                                                 pSharedHandle);
    }
    STDMETHOD(SetRenderTarget)(DWORD RenderTargetIndex, IDirect3DSurface9* pRenderTarget) override {
        return pDev->SetRenderTarget(RenderTargetIndex, pRenderTarget);
    }
    STDMETHOD(GetRenderTarget)(DWORD RenderTargetIndex,
                               IDirect3DSurface9** ppRenderTarget) override {
        return pDev->GetRenderTarget(RenderTargetIndex, ppRenderTarget);
    }
    STDMETHOD(SetDepthStencilSurface)(IDirect3DSurface9* pNewZStencil) override {
        return pDev->SetDepthStencilSurface(pNewZStencil);
    }
    STDMETHOD(GetDepthStencilSurface)(IDirect3DSurface9** ppZStencilSurface) override {
        return pDev->GetDepthStencilSurface(ppZStencilSurface);
    }
    STDMETHOD(BeginScene)() override { return pDev->BeginScene(); }

    STDMETHOD(EndScene)() override {
        // Main D3D9 hook here
        auto& cfg = conf::get();
        ui::set_processing(true);
        static bool inited = false;
        if (!inited && !cfg.custom_window) {
            inited = true;
            D3DDEVICE_CREATION_PARAMETERS params;
            pDev->GetCreationParameters(&params);
            ENSURE(::hwnd == params.hFocusWindow);
            IMGUI_CHECKVERSION();
            ImGui::CreateContext();
            ImGuiIO& io = ImGui::GetIO();
            io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
            io.IniFilename = nullptr;
            ImGui_ImplWin32_Init(hwnd);
            ImGui_ImplDX9_Init(pDev);
        }
        video::d3d9_draw(pDev);
        if (cfg.custom_window) {
            ui::set_processing(false);
            return pDev->EndScene();
        }
        ImGui_ImplDX9_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
        ui::draw(false);
        ImGui::EndFrame();
        ImGui::Render();
        ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
        ui::set_processing(false);
        return pDev->EndScene();
    }

    STDMETHOD(Clear)(DWORD Count, const D3DRECT* pRects, DWORD Flags, D3DCOLOR Color, float Z,
                     DWORD Stencil) override {
        return pDev->Clear(Count, pRects, Flags, Color, Z, Stencil);
    }
    STDMETHOD(SetTransform)(D3DTRANSFORMSTATETYPE State, const D3DMATRIX* pMatrix) override {
        return pDev->SetTransform(State, pMatrix);
    }
    STDMETHOD(GetTransform)(D3DTRANSFORMSTATETYPE State, D3DMATRIX* pMatrix) override {
        return pDev->GetTransform(State, pMatrix);
    }
    STDMETHOD(MultiplyTransform)(D3DTRANSFORMSTATETYPE State, const D3DMATRIX* pMatrix) override {
        return pDev->MultiplyTransform(State, pMatrix);
    }
    STDMETHOD(SetViewport)(const D3DVIEWPORT9* pViewport) override {
        return pDev->SetViewport(pViewport);
    }
    STDMETHOD(GetViewport)(D3DVIEWPORT9* pViewport) override {
        return pDev->GetViewport(pViewport);
    }
    STDMETHOD(SetMaterial)(const D3DMATERIAL9* pMaterial) override {
        return pDev->SetMaterial(pMaterial);
    }
    STDMETHOD(GetMaterial)(D3DMATERIAL9* pMaterial) override {
        return pDev->GetMaterial(pMaterial);
    }
    STDMETHOD(SetLight)(DWORD Index, const D3DLIGHT9* pLight) override {
        return pDev->SetLight(Index, pLight);
    }
    STDMETHOD(GetLight)(DWORD Index, D3DLIGHT9* pLight) override {
        return pDev->GetLight(Index, pLight);
    }
    STDMETHOD(LightEnable)(DWORD Index, BOOL Enable) override {
        return pDev->LightEnable(Index, Enable);
    }
    STDMETHOD(GetLightEnable)(DWORD Index, BOOL* pEnable) override {
        return pDev->GetLightEnable(Index, pEnable);
    }
    STDMETHOD(SetClipPlane)(DWORD Index, const float* pPlane) override {
        return pDev->SetClipPlane(Index, pPlane);
    }
    STDMETHOD(GetClipPlane)(DWORD Index, float* pPlane) override {
        return pDev->GetClipPlane(Index, pPlane);
    }
    STDMETHOD(SetRenderState)(D3DRENDERSTATETYPE State, DWORD Value) override {
        return pDev->SetRenderState(State, Value);
    }
    STDMETHOD(GetRenderState)(D3DRENDERSTATETYPE State, DWORD* pValue) override {
        return pDev->GetRenderState(State, pValue);
    }
    STDMETHOD(CreateStateBlock)(D3DSTATEBLOCKTYPE Type, IDirect3DStateBlock9** ppSB) override {
        return pDev->CreateStateBlock(Type, ppSB);
    }
    STDMETHOD(BeginStateBlock)() override { return pDev->BeginStateBlock(); }
    STDMETHOD(EndStateBlock)(IDirect3DStateBlock9** ppSB) override {
        return pDev->EndStateBlock(ppSB);
    }
    STDMETHOD(SetClipStatus)(const D3DCLIPSTATUS9* pClipStatus) override {
        return pDev->SetClipStatus(pClipStatus);
    }
    STDMETHOD(GetClipStatus)(D3DCLIPSTATUS9* pClipStatus) override {
        return pDev->GetClipStatus(pClipStatus);
    }
    STDMETHOD(GetTexture)(DWORD Stage, IDirect3DBaseTexture9** ppTexture) override {
        return pDev->GetTexture(Stage, ppTexture);
    }
    STDMETHOD(SetTexture)(DWORD Stage, IDirect3DBaseTexture9* pTexture) override {
        return pDev->SetTexture(Stage, pTexture);
    }
    STDMETHOD(GetTextureStageState)(DWORD Stage, D3DTEXTURESTAGESTATETYPE Type,
                                    DWORD* pValue) override {
        return pDev->GetTextureStageState(Stage, Type, pValue);
    }
    STDMETHOD(SetTextureStageState)(DWORD Stage, D3DTEXTURESTAGESTATETYPE Type,
                                    DWORD Value) override {
        return pDev->SetTextureStageState(Stage, Type, Value);
    }
    STDMETHOD(GetSamplerState)(DWORD Sampler, D3DSAMPLERSTATETYPE Type, DWORD* pValue) override {
        return pDev->GetSamplerState(Sampler, Type, pValue);
    }
    STDMETHOD(SetSamplerState)(DWORD Sampler, D3DSAMPLERSTATETYPE Type, DWORD Value) override {
        if (d3d9_need_pixelated() && Sampler == 0 &&
            (Type == D3DSAMP_MAGFILTER || Type == D3DSAMP_MINFILTER))
            Value = D3DTEXF_POINT;
        return pDev->SetSamplerState(Sampler, Type, Value);
    }
    STDMETHOD(ValidateDevice)(DWORD* pNumPasses) override {
        return pDev->ValidateDevice(pNumPasses);
    }
    STDMETHOD(SetPaletteEntries)(UINT PaletteNumber, const PALETTEENTRY* pEntries) override {
        return pDev->SetPaletteEntries(PaletteNumber, pEntries);
    }
    STDMETHOD(GetPaletteEntries)(UINT PaletteNumber, PALETTEENTRY* pEntries) override {
        return pDev->GetPaletteEntries(PaletteNumber, pEntries);
    }
    STDMETHOD(SetCurrentTexturePalette)(UINT PaletteNumber) override {
        return pDev->SetCurrentTexturePalette(PaletteNumber);
    }
    STDMETHOD(GetCurrentTexturePalette)(UINT* pPaletteNumber) override {
        return pDev->GetCurrentTexturePalette(pPaletteNumber);
    }
    STDMETHOD(SetScissorRect)(const RECT* pRect) override { return pDev->SetScissorRect(pRect); }
    STDMETHOD(GetScissorRect)(RECT* pRect) override { return pDev->GetScissorRect(pRect); }
    STDMETHOD(SetSoftwareVertexProcessing)(BOOL bSoftware) override {
        return pDev->SetSoftwareVertexProcessing(bSoftware);
    }
    STDMETHOD_(BOOL, GetSoftwareVertexProcessing)() override {
        return pDev->GetSoftwareVertexProcessing();
    }
    STDMETHOD(SetNPatchMode)(float nSegments) override { return pDev->SetNPatchMode(nSegments); }
    STDMETHOD_(float, GetNPatchMode)() override { return pDev->GetNPatchMode(); }
    STDMETHOD(DrawPrimitive)(D3DPRIMITIVETYPE PrimitiveType, UINT StartVertex,
                             UINT PrimitiveCount) override {
        return pDev->DrawPrimitive(PrimitiveType, StartVertex, PrimitiveCount);
    }
    STDMETHOD(DrawIndexedPrimitive)(D3DPRIMITIVETYPE PrimitiveType, INT BaseVertexIndex,
                                    UINT MinVertexIndex, UINT NumVertices, UINT startIndex,
                                    UINT primCount) override {
        return pDev->DrawIndexedPrimitive(PrimitiveType, BaseVertexIndex, MinVertexIndex,
                                          NumVertices, startIndex, primCount);
    }
    STDMETHOD(DrawPrimitiveUP)(D3DPRIMITIVETYPE PrimitiveType, UINT PrimitiveCount,
                               const void* pVertexStreamZeroData,
                               UINT VertexStreamZeroStride) override {
        return pDev->DrawPrimitiveUP(PrimitiveType, PrimitiveCount, pVertexStreamZeroData,
                                     VertexStreamZeroStride);
    }
    STDMETHOD(DrawIndexedPrimitiveUP)(D3DPRIMITIVETYPE PrimitiveType, UINT MinVertexIndex,
                                      UINT NumVertices, UINT PrimitiveCount, const void* pIndexData,
                                      D3DFORMAT IndexDataFormat, const void* pVertexStreamZeroData,
                                      UINT VertexStreamZeroStride) override {
        return pDev->DrawIndexedPrimitiveUP(PrimitiveType, MinVertexIndex, NumVertices,
                                            PrimitiveCount, pIndexData, IndexDataFormat,
                                            pVertexStreamZeroData, VertexStreamZeroStride);
    }
    STDMETHOD(ProcessVertices)(UINT SrcStartIndex, UINT DestIndex, UINT vertexCount,
                               IDirect3DVertexBuffer9* pDestBuffer,
                               IDirect3DVertexDeclaration9* pPlacement, DWORD Flags) override {
        return pDev->ProcessVertices(SrcStartIndex, DestIndex, vertexCount, pDestBuffer, pPlacement,
                                     Flags);
    }
    STDMETHOD(CreateVertexDeclaration)(const D3DVERTEXELEMENT9* pVertexElements,
                                       IDirect3DVertexDeclaration9** ppDecl) override {
        return pDev->CreateVertexDeclaration(pVertexElements, ppDecl);
    }
    STDMETHOD(SetVertexDeclaration)(IDirect3DVertexDeclaration9* pDecl) override {
        return pDev->SetVertexDeclaration(pDecl);
    }
    STDMETHOD(GetVertexDeclaration)(IDirect3DVertexDeclaration9** ppDecl) override {
        return pDev->GetVertexDeclaration(ppDecl);
    }
    STDMETHOD(SetFVF)(DWORD FVF) override { return pDev->SetFVF(FVF); }
    STDMETHOD(GetFVF)(DWORD* pFVF) override { return pDev->GetFVF(pFVF); }
    STDMETHOD(CreateVertexShader)(const DWORD* pFunction,
                                  IDirect3DVertexShader9** ppShader) override {
        return pDev->CreateVertexShader(pFunction, ppShader);
    }
    STDMETHOD(SetVertexShader)(IDirect3DVertexShader9* pShader) override {
        return pDev->SetVertexShader(pShader);
    }
    STDMETHOD(GetVertexShader)(IDirect3DVertexShader9** ppShader) override {
        return pDev->GetVertexShader(ppShader);
    }
    STDMETHOD(SetVertexShaderConstantF)(UINT StartRegister, const float* pConstantData,
                                        UINT Vector4fCount) override {
        return pDev->SetVertexShaderConstantF(StartRegister, pConstantData, Vector4fCount);
    }
    STDMETHOD(GetVertexShaderConstantF)(UINT StartRegister, float* pConstantData,
                                        UINT Vector4fCount) override {
        return pDev->GetVertexShaderConstantF(StartRegister, pConstantData, Vector4fCount);
    }
    STDMETHOD(SetVertexShaderConstantI)(UINT StartRegister, const int* pConstantData,
                                        UINT Vector4iCount) override {
        return pDev->SetVertexShaderConstantI(StartRegister, pConstantData, Vector4iCount);
    }
    STDMETHOD(GetVertexShaderConstantI)(UINT StartRegister, int* pConstantData,
                                        UINT Vector4iCount) override {
        return pDev->GetVertexShaderConstantI(StartRegister, pConstantData, Vector4iCount);
    }
    STDMETHOD(SetVertexShaderConstantB)(UINT StartRegister, const BOOL* pConstantData,
                                        UINT BoolCount) override {
        return pDev->SetVertexShaderConstantB(StartRegister, pConstantData, BoolCount);
    }
    STDMETHOD(GetVertexShaderConstantB)(UINT StartRegister, BOOL* pConstantData,
                                        UINT BoolCount) override {
        return pDev->GetVertexShaderConstantB(StartRegister, pConstantData, BoolCount);
    }
    STDMETHOD(SetStreamSource)(UINT StreamNumber, IDirect3DVertexBuffer9* pStreamData,
                               UINT OffsetInBytes, UINT Stride) override {
        return pDev->SetStreamSource(StreamNumber, pStreamData, OffsetInBytes, Stride);
    }
    STDMETHOD(GetStreamSource)(UINT StreamNumber, IDirect3DVertexBuffer9** ppStreamData,
                               UINT* pOffsetInBytes, UINT* pStride) override {
        return pDev->GetStreamSource(StreamNumber, ppStreamData, pOffsetInBytes, pStride);
    }
    STDMETHOD(SetStreamSourceFreq)(UINT StreamNumber, UINT Setting) override {
        return pDev->SetStreamSourceFreq(StreamNumber, Setting);
    }
    STDMETHOD(GetStreamSourceFreq)(UINT StreamNumber, UINT* pSetting) override {
        return pDev->GetStreamSourceFreq(StreamNumber, pSetting);
    }
    STDMETHOD(SetIndices)(IDirect3DIndexBuffer9* pIndexData) override {
        return pDev->SetIndices(pIndexData);
    }
    STDMETHOD(GetIndices)(IDirect3DIndexBuffer9** ppIndexData) override {
        return pDev->GetIndices(ppIndexData);
    }
    STDMETHOD(CreatePixelShader)(const DWORD* pFunction,
                                 IDirect3DPixelShader9** ppShader) override {
        return pDev->CreatePixelShader(pFunction, ppShader);
    }
    STDMETHOD(SetPixelShader)(IDirect3DPixelShader9* pShader) override {
        return pDev->SetPixelShader(pShader);
    }
    STDMETHOD(GetPixelShader)(IDirect3DPixelShader9** ppShader) override {
        return pDev->GetPixelShader(ppShader);
    }
    STDMETHOD(SetPixelShaderConstantF)(UINT StartRegister, const float* pConstantData,
                                       UINT Vector4fCount) override {
        return pDev->SetPixelShaderConstantF(StartRegister, pConstantData, Vector4fCount);
    }
    STDMETHOD(GetPixelShaderConstantF)(UINT StartRegister, float* pConstantData,
                                       UINT Vector4fCount) override {
        return pDev->GetPixelShaderConstantF(StartRegister, pConstantData, Vector4fCount);
    }
    STDMETHOD(SetPixelShaderConstantI)(UINT StartRegister, const int* pConstantData,
                                       UINT Vector4iCount) override {
        return pDev->SetPixelShaderConstantI(StartRegister, pConstantData, Vector4iCount);
    }
    STDMETHOD(GetPixelShaderConstantI)(UINT StartRegister, int* pConstantData,
                                       UINT Vector4iCount) override {
        return pDev->GetPixelShaderConstantI(StartRegister, pConstantData, Vector4iCount);
    }
    STDMETHOD(SetPixelShaderConstantB)(UINT StartRegister, const BOOL* pConstantData,
                                       UINT BoolCount) override {
        return pDev->SetPixelShaderConstantB(StartRegister, pConstantData, BoolCount);
    }
    STDMETHOD(GetPixelShaderConstantB)(UINT StartRegister, BOOL* pConstantData,
                                       UINT BoolCount) override {
        return pDev->GetPixelShaderConstantB(StartRegister, pConstantData, BoolCount);
    }
    STDMETHOD(DrawRectPatch)(UINT Handle, const float* pNumSegs,
                             const D3DRECTPATCH_INFO* pRectPatchInfo) override {
        return pDev->DrawRectPatch(Handle, pNumSegs, pRectPatchInfo);
    }
    STDMETHOD(DrawTriPatch)(UINT Handle, const float* pNumSegs,
                            const D3DTRIPATCH_INFO* pTriPatchInfo) override {
        return pDev->DrawTriPatch(Handle, pNumSegs, pTriPatchInfo);
    }
    STDMETHOD(DeletePatch)(UINT Handle) override { return pDev->DeletePatch(Handle); }
    STDMETHOD(CreateQuery)(D3DQUERYTYPE Type, IDirect3DQuery9** ppQuery) override {
        return pDev->CreateQuery(Type, ppQuery);
    }
};

// IDirect3D9 proxy hook
class IDirect3D9Proxy : public IDirect3D9 {
    IDirect3D9* pD3D;

public:
    IDirect3D9Proxy(IDirect3D9* pReal) : pD3D(pReal) {}
    virtual ~IDirect3D9Proxy() {}

    STDMETHOD(QueryInterface)(REFIID riid, void** ppvObj) override {
        return pD3D->QueryInterface(riid, ppvObj);
    }
    STDMETHOD_(ULONG, AddRef)() override { return pD3D->AddRef(); }
    STDMETHOD_(ULONG, Release)() override {
        ULONG count = pD3D->Release();
        // Release us
        if (count == 0)
            delete this;
        return count;
    }

    STDMETHOD(RegisterSoftwareDevice)(void* pInitializeFunction) override {
        return pD3D->RegisterSoftwareDevice(pInitializeFunction);
    }
    STDMETHOD_(UINT, GetAdapterCount)() override { return pD3D->GetAdapterCount(); }
    STDMETHOD(GetAdapterIdentifier)(UINT Adapter, DWORD Flags,
                                    D3DADAPTER_IDENTIFIER9* pIdentifier) override {
        return pD3D->GetAdapterIdentifier(Adapter, Flags, pIdentifier);
    }
    STDMETHOD_(UINT, GetAdapterModeCount)(UINT Adapter, D3DFORMAT Format) override {
        return pD3D->GetAdapterModeCount(Adapter, Format);
    }
    STDMETHOD(EnumAdapterModes)(UINT Adapter, D3DFORMAT Format, UINT Mode,
                                D3DDISPLAYMODE* pMode) override {
        return pD3D->EnumAdapterModes(Adapter, Format, Mode, pMode);
    }
    STDMETHOD(GetAdapterDisplayMode)(UINT Adapter, D3DDISPLAYMODE* pMode) override {
        return pD3D->GetAdapterDisplayMode(Adapter, pMode);
    }
    STDMETHOD(CheckDeviceType)(UINT Adapter, D3DDEVTYPE DevType, D3DFORMAT DisplayFormat,
                               D3DFORMAT BackBufferFormat, BOOL bWindowed) override {
        return pD3D->CheckDeviceType(Adapter, DevType, DisplayFormat, BackBufferFormat, bWindowed);
    }
    STDMETHOD(CheckDeviceFormat)(UINT Adapter, D3DDEVTYPE DeviceType, D3DFORMAT AdapterFormat,
                                 DWORD Usage, D3DRESOURCETYPE RType,
                                 D3DFORMAT CheckFormat) override {
        return pD3D->CheckDeviceFormat(Adapter, DeviceType, AdapterFormat, Usage, RType,
                                       CheckFormat);
    }
    STDMETHOD(CheckDeviceMultiSampleType)(UINT Adapter, D3DDEVTYPE DeviceType,
                                          D3DFORMAT SurfaceFormat, BOOL Windowed,
                                          D3DMULTISAMPLE_TYPE MultiSampleType,
                                          DWORD* pQualityLevels) override {
        return pD3D->CheckDeviceMultiSampleType(Adapter, DeviceType, SurfaceFormat, Windowed,
                                                MultiSampleType, pQualityLevels);
    }
    STDMETHOD(CheckDepthStencilMatch)(UINT Adapter, D3DDEVTYPE DeviceType, D3DFORMAT AdapterFormat,
                                      D3DFORMAT RenderTargetFormat,
                                      D3DFORMAT DepthStencilFormat) override {
        return pD3D->CheckDepthStencilMatch(Adapter, DeviceType, AdapterFormat, RenderTargetFormat,
                                            DepthStencilFormat);
    }
    STDMETHOD(CheckDeviceFormatConversion)(UINT Adapter, D3DDEVTYPE DeviceType,
                                           D3DFORMAT SourceFormat,
                                           D3DFORMAT TargetFormat) override {
        return pD3D->CheckDeviceFormatConversion(Adapter, DeviceType, SourceFormat, TargetFormat);
    }
    STDMETHOD(GetDeviceCaps)(UINT Adapter, D3DDEVTYPE DeviceType, D3DCAPS9* pCaps) override {
        return pD3D->GetDeviceCaps(Adapter, DeviceType, pCaps);
    }
    STDMETHOD_(HMONITOR, GetAdapterMonitor)(UINT Adapter) override {
        return pD3D->GetAdapterMonitor(Adapter);
    }

    STDMETHOD(CreateDevice)(UINT Adapter, D3DDEVTYPE DeviceType, HWND hFocusWindow,
                            DWORD BehaviorFlags, D3DPRESENT_PARAMETERS* pPresentationParameters,
                            IDirect3DDevice9** ppReturnedDeviceInterface) override {
        spdlog::debug("IDirect3D9Proxy -> CreateDevice called");

        HRESULT hr = pD3D->CreateDevice(Adapter, DeviceType, hFocusWindow, BehaviorFlags,
                                        pPresentationParameters, ppReturnedDeviceInterface);
        auto& cfg = conf::get();
        if (cfg.render_type != conf::RenderType::None &&
            cfg.render_type != conf::RenderType::D3D9) {
            spdlog::debug("Skipping D3D9 CreateDevice hook");
            return hr;
        }
        if (SUCCEEDED(hr) && ppReturnedDeviceInterface && *ppReturnedDeviceInterface) {
            spdlog::debug("Wrapping IDirect3DDevice9 into ID3D9Proxy");
            *ppReturnedDeviceInterface = new ID3D9Proxy(*ppReturnedDeviceInterface);
            cfg.render_type = conf::RenderType::D3D9;
        }
        return hr;
    }
};

IDirect3D9*(WINAPI* Direct3DCreate9O)(UINT SDKVersion) = Direct3DCreate9;
static IDirect3D9* WINAPI Direct3DCreate9H(UINT SDKVersion) {
    spdlog::debug("Direct3DCreate9");
    auto ret = Direct3DCreate9O(SDKVersion);
    if (ret) {
        spdlog::debug("Wrapping IDirect3D9 into ID3D9Proxy");
        return new IDirect3D9Proxy(ret);
    }
    return ret;
}

void d3dhooks::pre_init() { IAT_AUTO("ddraw.dll", DirectDrawCreate); }

void d3dhooks::init() { IAT_AUTO("d3d9.dll", Direct3DCreate9); }
