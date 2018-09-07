#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include "Utility/CheckHResult.hpp"

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

// Window callback
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

void EnableDebugLayer()
{
#if defined(_DEBUG)
	ComPtr<ID3D12Debug> debugInterface;
	ThrowIfFailed(D3D12GetDebugInterface(IID_PPV_ARGS(&debugInterface)));
	debugInterface->EnableDebugLayer();
#endif
}

void RegisterWindowClass(HINSTANCE hInstance, const wchar_t* windowClassName)
{
	WNDCLASSEXW windowClass = {};

	windowClass.cbSize = sizeof(WNDCLASSEXW);
	windowClass.style = CS_HREDRAW | CS_VREDRAW;
	windowClass.lpfnWndProc = &WndProc;
	windowClass.cbClsExtra = 0;
	windowClass.cbWndExtra = 0;
	windowClass.hInstance = hInstance;
	windowClass.hIcon = ::LoadIcon(hInstance, nullptr);
	windowClass.hCursor = ::LoadCursor(hInstance, IDC_ARROW);
	windowClass.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	windowClass.lpszMenuName = nullptr;
	windowClass.lpszClassName = windowClassName;
	windowClass.hIconSm = ::LoadIcon(hInstance, nullptr);

	static ATOM atom = ::RegisterClassExW(&windowClass);
	assert(atom > 0);
}

HWND CreateWindow(const wchar_t* windowClassName, HINSTANCE hInstance,
	const wchar_t* windowTitle, uint32_t width, uint32_t height)
{
	int screenWidth = ::GetSystemMetrics(SM_CXSCREEN);
	int screenHeight = ::GetSystemMetrics(SM_CYSCREEN);

	RECT windowRect =
	{
		0, 0, static_cast<LONG>(width), static_cast<LONG>(height)
	};

	::AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, false);

	int windowWidth = windowRect.right - windowRect.left;
	int windowHeight = windowRect.bottom - windowRect.top;

	// Center the window (clamped to 0 avoid an off screen window)
	int windowX = std::max<int>(0, (screenWidth - windowWidth) / 2);
	int windowY = std::max<int>(0, (screenHeight - windowHeight) / 2);

	HWND hWnd = ::CreateWindowExW(
		NULL,
		windowClassName,
		windowTitle,
		WS_OVERLAPPEDWINDOW,
		windowX,
		windowY,
		windowWidth,
		windowHeight,
		nullptr,
		nullptr,
		hInstance,
		nullptr
	);

	assert(hWnd && "Failed to create the window.");
	return hWnd;
}

// Query for a DX12 compatible adapter
ComPtr<IDXGIAdapter4> GetAdapter(bool useWarp)
{
	ComPtr<IDXGIFactory4> dxgiFactory;
	UINT createFactoryFlags = 0;

#if defined(_DEBUG)
	createFactoryFlags = DXGI_CREATE_FACTORY_DEBUG;
#endif

	ThrowIfFailed(
		CreateDXGIFactory2(createFactoryFlags, IID_PPV_ARGS(&dxgiFactory)));

	ComPtr<IDXGIAdapter1> dxgiAdapter1;
	ComPtr<IDXGIAdapter4> dxgiAdapter4;

	if (useWarp)
	{
		ThrowIfFailed(dxgiFactory->EnumWarpAdapter(IID_PPV_ARGS(&dxgiAdapter1)));
		ThrowIfFailed(dxgiAdapter1.As(&dxgiAdapter4));
	}
	else
	{
		SIZE_T maxDedicatedVideoMemory = 0;

		for (UINT i = 0; i < dxgiFactory->EnumAdapters1(i, &dxgiAdapter1) != DXGI_ERROR_NOT_FOUND; ++i)
		{
			DXGI_ADAPTER_DESC1 dxgiAdapterDesc1;
			dxgiAdapter1->GetDesc1(&dxgiAdapterDesc1);

			// Find a DX12 compatible hardware adapter
			if ((dxgiAdapterDesc1.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) == 0 &&
				SUCCEEDED(D3D12CreateDevice(dxgiAdapter1.Get(), D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), nullptr)) &&
				dxgiAdapterDesc1.DedicatedVideoMemory > maxDedicatedVideoMemory)
			{
				maxDedicatedVideoMemory = dxgiAdapterDesc1.DedicatedVideoMemory;
				ThrowIfFailed(dxgiAdapter1.As(&dxgiAdapter4));
			}
		}
	}

	return dxgiAdapter4;
}

ComPtr<ID3D12Device2> CreateDevice(ComPtr<IDXGIAdapter4> adapter)
{
	ComPtr<ID3D12Device2> d3d12Device2;
	ThrowIfFailed(D3D12CreateDevice(
		adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&d3d12Device2)));

#if defined(_DEBUG)
	ComPtr<ID3D12InfoQueue> infoQueue;

	// Enable these debug messages
	if (SUCCEEDED(d3d12Device2.As(&infoQueue)))
	{
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, TRUE);
		
		// Suppress messages based on their severity
		D3D12_MESSAGE_SEVERITY severities[] =
		{
			D3D12_MESSAGE_SEVERITY_INFO
		};

		D3D12_INFO_QUEUE_FILTER infoQueueFilter = {};
		infoQueueFilter.DenyList.NumSeverities = _countof(severities);
		infoQueueFilter.DenyList.pSeverityList = severities;

		ThrowIfFailed(infoQueue->PushStorageFilter(&infoQueueFilter));
	}

#endif

	return d3d12Device2;
}

int main()
{
    return 0;
}
