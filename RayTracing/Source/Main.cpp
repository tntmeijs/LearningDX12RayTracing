#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

// Make sure to only use the std::min and std::max functions
#if defined(min)
#undef min
#endif

#if defined(min)
#undef min
#endif

// This function is not needed and could cause conflicts without this macro
#if defined(CreateWindow)
#undef CreateWindow
#endif

// Need the ComPtr<T> class
#include <wrl.h>
using namespace Microsoft::WRL;

// DX12
#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>

// DX12 extension library
#include <d3dx12.h>

// STL
#include <algorithm>
#include <cassert>
#include <chrono>

// === CONTROL VARIABLES ===
// The application uses triple-buffering
const uint8_t BACK_BUFFER_COUNT = 3;

// Do not use the WARP (Windows Advanced Rasterization Platform)
const bool USE_WARP = false;

// Default size of the client area of the window
uint32_t clientAreaWidth = 1280;
uint32_t clientAreaHeight = 720;

// True once DX12 initialization is done
bool initialized = false;

// V-Sync toggle (V-key)
bool vsync = true;
bool tearingSupported = false;

// Windowed mode by default
bool fullscreen = false;

// Window
HWND windowHandle;
RECT windowRect;

// DX12
ComPtr<ID3D12Device2> device;
ComPtr<ID3D12CommandQueue> commandQueue;
ComPtr<ID3D12GraphicsCommandList> commandList;
ComPtr<ID3D12CommandAllocator> commandAllocators[BACK_BUFFER_COUNT];
ComPtr<ID3D12Resource> backBuffers[BACK_BUFFER_COUNT];
ComPtr<IDXGISwapChain4> swapChain;
ComPtr<ID3D12DescriptorHeap> rtvDescriptorHeap;

UINT rtvDescriptorSize;
UINT currentBackBufferIndex;

// Synchronization
ComPtr<ID3D12Fence> fence;
uint64_t fenceValue = 0;
uint64_t frameFenceValues[BACK_BUFFER_COUNT] = {};
HANDLE fenceEvent;

int main()
{
    return 0;
}
