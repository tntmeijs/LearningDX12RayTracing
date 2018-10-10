#include <Windows.h>

// DX12
#include <d3d12.h>
#include <dxgi1_4.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
using namespace DirectX;

// Need the ComPtr<t> for this application
#include <wrl.h>
using namespace Microsoft::WRL;

// Helper functions / structures provided by Microsoft
#include <d3dx12.h>

#include "Utility/CheckHResult.hpp"

struct Vertex
{
	XMFLOAT3 position;
	XMFLOAT4 color;
};

const UINT BACK_BUFFER_COUNT = 3;
const UINT WINDOW_WIDTH = 1280;
const UINT WINDOW_HEIGHT = 720;

const BOOL USE_WARP_DEVICE = FALSE;

const FLOAT BACK_BUFFER_CLEAR_COLOR[] = { 0.392f, 0.584f, 0.929f, 0.0f };

const WCHAR* className = L"LearningRayTracing";
const WCHAR* windowTitle = L"Learning Ray Tracing | Tahar Meijs";

HWND windowHandle = nullptr;

UINT frameIndex = 0;
UINT rtvDescriptorSize = 0;

UINT64 fenceValue = 0;

HANDLE fenceEvent = nullptr;

// DX12 objects
ComPtr<IDXGISwapChain3> swapChain;

CD3DX12_VIEWPORT viewport(0.0f, 0.0f, static_cast<FLOAT>(WINDOW_WIDTH), static_cast<FLOAT>(WINDOW_HEIGHT));
CD3DX12_RECT scissorRect(0, 0, static_cast<LONG>(WINDOW_WIDTH), static_cast<LONG>(WINDOW_HEIGHT));

D3D12_VERTEX_BUFFER_VIEW vertexBufferView = {};

ComPtr<ID3D12Device> device;
ComPtr<ID3D12DescriptorHeap> rtvHeap;
ComPtr<ID3D12Resource> renderTargets[BACK_BUFFER_COUNT];
ComPtr<ID3D12CommandQueue> graphicsCommandQueue;
ComPtr<ID3D12CommandAllocator> graphicsCommandAllocator;
ComPtr<ID3D12GraphicsCommandList> graphicsCommandList;
ComPtr<ID3D12Fence> fence;
ComPtr<ID3D12PipelineState> graphicsPipelineStateObject;
ComPtr<ID3D12RootSignature> rootSignature;
ComPtr<ID3D12Resource> vertexBuffer;

void WaitForPreviousFrame()
{
	// Signal and increment fence value
	const UINT64 currentFenceValue = fenceValue;
	ThrowIfFailed(graphicsCommandQueue->Signal(fence.Get(), currentFenceValue));
	++fenceValue;

	// Wait until the previous frame finishes
	if (fence->GetCompletedValue() < currentFenceValue)
	{
		ThrowIfFailed(fence->SetEventOnCompletion(currentFenceValue, fenceEvent));
		WaitForSingleObject(fenceEvent, INFINITE);
	}

	frameIndex = swapChain->GetCurrentBackBufferIndex();
}

void PopulateCommandList()
{
	// This can only happen when the associated command lists have finished execution on the GPU
	ThrowIfFailed(graphicsCommandAllocator->Reset());

	// Command lists can be reset at any time as long as execute has been called on it
	ThrowIfFailed(graphicsCommandList->Reset(graphicsCommandAllocator.Get(), graphicsPipelineStateObject.Get()));

	// Set the correct states
	graphicsCommandList->SetGraphicsRootSignature(rootSignature.Get());
	graphicsCommandList->RSSetViewports(1, &viewport);
	graphicsCommandList->RSSetScissorRects(1, &scissorRect);

	// Indicate that the back buffer will be used as a render target
	graphicsCommandList->ResourceBarrier(
		1,
		&CD3DX12_RESOURCE_BARRIER::Transition(
			renderTargets[frameIndex].Get(),
			D3D12_RESOURCE_STATE_PRESENT,
			D3D12_RESOURCE_STATE_RENDER_TARGET
		)
	);

	// Handle to the current back buffer of the swap chain
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(
		rtvHeap->GetCPUDescriptorHandleForHeapStart(),
		frameIndex,
		rtvDescriptorSize
	);

	// Set the current back buffer as the render target
	graphicsCommandList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

	// Record commands
	graphicsCommandList->ClearRenderTargetView(rtvHandle, BACK_BUFFER_CLEAR_COLOR, 0, nullptr);
	graphicsCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	graphicsCommandList->IASetVertexBuffers(0, 1, &vertexBufferView);
	graphicsCommandList->DrawInstanced(3, 1, 0, 0);

	// Indicate that the back buffer will now be used to present
	graphicsCommandList->ResourceBarrier(
		1,
		&CD3DX12_RESOURCE_BARRIER::Transition(
			renderTargets[frameIndex].Get(),
			D3D12_RESOURCE_STATE_RENDER_TARGET,
			D3D12_RESOURCE_STATE_PRESENT
		)
	);

	// Done recording commands
	ThrowIfFailed(graphicsCommandList->Close());
}

void Initialize()
{
#pragma region PIPELINE_SET_UP
	{
		UINT dxgiFactoryFlags = 0;

		// === ============ ===
		// === DEBUG LAYERS ===
		// === ============ ===
#if defined(_DEBUG)
		{
			ComPtr<ID3D12Debug> debugController;

			if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
			{
				debugController->EnableDebugLayer();

				// Additional debug layers
				dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
			}}
#endif

		// === =============== ===
		// === PHYSICAL DEVICE ===
		// === =============== ===
		ComPtr<IDXGIFactory4> factory;
		ThrowIfFailed(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&factory)));

		if (USE_WARP_DEVICE)
		{
			ComPtr<IDXGIAdapter> warpAdapter;
			ThrowIfFailed(factory->EnumWarpAdapter(IID_PPV_ARGS(&warpAdapter)));

			ThrowIfFailed(D3D12CreateDevice(warpAdapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device)));
		}
		else
		{
			ComPtr<IDXGIAdapter1> hardwareAdapter;

			for (UINT adapterIndex = 0; DXGI_ERROR_NOT_FOUND != factory->EnumAdapters1(adapterIndex, &hardwareAdapter); ++adapterIndex)
			{
				DXGI_ADAPTER_DESC1 desc;
				hardwareAdapter->GetDesc1(&desc);

				if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
				{
					// Ignore the software (Wrap) adapter, looking for a hardware adapter
					continue;
				}

				// Check for DX12 support
				if (SUCCEEDED(D3D12CreateDevice(hardwareAdapter.Get(), D3D_FEATURE_LEVEL_11_0, _uuidof(ID3D12Device), nullptr)))
				{
					break;
				}
			}

			ThrowIfFailed(D3D12CreateDevice(hardwareAdapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device)));
		}

		// === ============= ===
		// === COMMAND QUEUE ===
		// === ============= ===
		D3D12_COMMAND_QUEUE_DESC commandQueueDesc = {};
		commandQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
		commandQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

		ThrowIfFailed(device->CreateCommandQueue(&commandQueueDesc, IID_PPV_ARGS(&graphicsCommandQueue)));

		// === ========== ===
		// === SWAP CHAIN ===
		// === ========== ===
		DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
		swapChainDesc.BufferCount = BACK_BUFFER_COUNT;
		swapChainDesc.Width = WINDOW_WIDTH;
		swapChainDesc.Height = WINDOW_HEIGHT;
		swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		swapChainDesc.SampleDesc.Count = 1;

		ComPtr<IDXGISwapChain1> swapChain1;
		ThrowIfFailed(factory->CreateSwapChainForHwnd(
			graphicsCommandQueue.Get(),
			windowHandle,
			&swapChainDesc,
			nullptr,
			nullptr,
			&swapChain1
		));

		// No full screen transition
		ThrowIfFailed(factory->MakeWindowAssociation(windowHandle, DXGI_MWA_NO_ALT_ENTER));

		// Convert from swap chain 1 to a swap chain 3
		ThrowIfFailed(swapChain1.As(&swapChain));
		frameIndex = swapChain->GetCurrentBackBufferIndex();

		// === ================ ===
		// === DESCRIPTOR HEAPS ===
		// === ================ ===
		{
			// Create the render target view descriptor heap
			D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
			rtvHeapDesc.NumDescriptors = BACK_BUFFER_COUNT;
			rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
			rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

			ThrowIfFailed(device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&rtvHeap)));
			rtvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
		}

		// === =============== ===
		// === FRAME RESOURCES ===
		// === =============== ===
		{
			CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(rtvHeap->GetCPUDescriptorHandleForHeapStart());

			// Create a new render target view for each frame
			for (UINT n = 0; n < BACK_BUFFER_COUNT; ++n)
			{
				ThrowIfFailed(swapChain->GetBuffer(n, IID_PPV_ARGS(&renderTargets[n])));

				device->CreateRenderTargetView(renderTargets[n].Get(), nullptr, rtvHandle);
				rtvHandle.Offset(1, rtvDescriptorSize);
			}
		}

		// === ================= ===
		// === COMMAND ALLOCATOR ===
		// === ================= ===
		ThrowIfFailed(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&graphicsCommandAllocator)));
	}
#pragma endregion

#pragma region ASSET_LOADING
	{
		// Create the root signature
		{
			CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
			rootSignatureDesc.Init(
				0,
				nullptr,
				0,
				nullptr,
				D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
			);

			ComPtr<ID3DBlob> signature;
			ComPtr<ID3DBlob> error;

			ThrowIfFailed(D3D12SerializeRootSignature(
				&rootSignatureDesc,
				D3D_ROOT_SIGNATURE_VERSION_1,
				&signature,
				&error
			));

			ThrowIfFailed(device->CreateRootSignature(
				0,
				signature->GetBufferPointer(),
				signature->GetBufferSize(),
				IID_PPV_ARGS(&rootSignature)
			));
		}

		// Create the pipeline state (also compiles / loads the shaders)
		{
			ComPtr<ID3DBlob> vertexShader;
			ComPtr<ID3DBlob> pixelShader;

			UINT compileFlags = 0;

#if defined(_DEBUG)
			compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

			// Compile the vertex shader
			ThrowIfFailed(D3DCompileFromFile(
				L"Resources/Shaders/simple_shader.hlsl",
				nullptr,
				nullptr,
				"vs_main",
				"vs_5_0",
				compileFlags,
				0,
				&vertexShader,
				nullptr
			));

			// Compile the pixel shader
			ThrowIfFailed(D3DCompileFromFile(
				L"Resources/Shaders/simple_shader.hlsl",
				nullptr,
				nullptr,
				"ps_main",
				"ps_5_0",
				compileFlags,
				0,
				&pixelShader,
				nullptr
			));

			// Define the vertex input layout
			D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
			{
				{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
				{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
			};

			// Describe and create the pipeline state object
			D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicsPipelineStateObjectDesc = {};
			graphicsPipelineStateObjectDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
			graphicsPipelineStateObjectDesc.pRootSignature = rootSignature.Get();
			graphicsPipelineStateObjectDesc.VS = CD3DX12_SHADER_BYTECODE(vertexShader.Get());
			graphicsPipelineStateObjectDesc.PS = CD3DX12_SHADER_BYTECODE(pixelShader.Get());
			graphicsPipelineStateObjectDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
			graphicsPipelineStateObjectDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
			graphicsPipelineStateObjectDesc.DepthStencilState.DepthEnable = FALSE;
			graphicsPipelineStateObjectDesc.DepthStencilState.StencilEnable = FALSE;
			graphicsPipelineStateObjectDesc.SampleMask = UINT_MAX;
			graphicsPipelineStateObjectDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
			graphicsPipelineStateObjectDesc.NumRenderTargets = 1;
			graphicsPipelineStateObjectDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
			graphicsPipelineStateObjectDesc.SampleDesc.Count = 1;

			ThrowIfFailed(device->CreateGraphicsPipelineState(&graphicsPipelineStateObjectDesc, IID_PPV_ARGS(&graphicsPipelineStateObject)));
		}

		ThrowIfFailed(device->CreateCommandList(
			0,
			D3D12_COMMAND_LIST_TYPE_DIRECT,
			graphicsCommandAllocator.Get(),
			nullptr,
			IID_PPV_ARGS(&graphicsCommandList)
		));

		// Command lists record by default, main loop expects it to be closed by default
		ThrowIfFailed(graphicsCommandList->Close());

		// === ============= ===
		// === VERTEX BUFFER ===
		// === ============= ===
		{
			Vertex vertices[] =
			{
				{ {  0.0f,  0.5f, 0.0f }, { 1.0f, 0.0f, 0.0f, 1.0f } },
				{ {  0.5f, -0.5f, 0.0f }, { 0.0f, 1.0f, 0.0f, 1.0f } },
				{ { -0.5f, -0.5f, 0.0f }, { 0.0f, 0.0f, 1.0f, 1.0f } }
			};

			const UINT vertexBufferSize = sizeof(vertices);

			// TODO: read on default heap usage
			ThrowIfFailed(device->CreateCommittedResource(
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
				D3D12_HEAP_FLAG_NONE,
				&CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize),
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				IID_PPV_ARGS(&vertexBuffer)
			));

			// Copy the data to the vertex buffer
			UINT8* pVertexDataBegin = nullptr;
			CD3DX12_RANGE readRange(0, 0);	// Do not intend to read from this resource on the CPU
			ThrowIfFailed(vertexBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pVertexDataBegin)));
			memcpy(pVertexDataBegin, vertices, vertexBufferSize);
			vertexBuffer->Unmap(0, nullptr);

			// Initialize the vertex buffer view
			vertexBufferView.BufferLocation = vertexBuffer->GetGPUVirtualAddress();
			vertexBufferView.StrideInBytes = sizeof(Vertex);
			vertexBufferView.SizeInBytes = vertexBufferSize;
		}

		// Synchronization objects
		{
			ThrowIfFailed(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)));
			fenceValue = 1;

			// Even handle for frame synchronization
			fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);

			if (fenceEvent == nullptr)
			{
				ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
			}

			// Wait for the setup to complete...
			WaitForPreviousFrame();
		}
	}
#pragma endregion
}

void Update()
{
	//
}

void Render()
{
	// Record all commands to render the scene
	PopulateCommandList();

	// Execute said commands
	ID3D12CommandList* ppCommandLists[] = { graphicsCommandList.Get() };
	graphicsCommandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

	// Present the frame (using v-sync)
	ThrowIfFailed(swapChain->Present(1, 0));
	
	WaitForPreviousFrame();
}

void Destroy()
{
	// Make sure that the GPU is no longer using any of the resources that are about to be deallocated
	WaitForPreviousFrame();

	CloseHandle(fenceEvent);
}

LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_PAINT:
		Update();
		Render();
		return 0;

	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	}

	// Fall back
	return DefWindowProc(hWnd, message, wParam, lParam);
}

int main(int argc, char* argv[])
{
	HINSTANCE hInstance = GetModuleHandle(nullptr);

	// Initialize the window class
	WNDCLASSEX windowClass = { 0 };
	windowClass.cbSize = sizeof(WNDCLASSEX);
	windowClass.style = CS_HREDRAW | CS_VREDRAW;
	windowClass.lpfnWndProc = WindowProc;
	windowClass.hInstance = hInstance;
	windowClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
	windowClass.lpszClassName = className;
	RegisterClassEx(&windowClass);

	RECT windowRect = { 0, 0, static_cast<LONG>(WINDOW_WIDTH), static_cast<LONG>(WINDOW_HEIGHT) };
	AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, FALSE);

	// Create the window
	windowHandle = CreateWindow(
		windowClass.lpszClassName,
		windowTitle,
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		windowRect.right - windowRect.left,
		windowRect.bottom - windowRect.top,
		nullptr,	// No parent window
		nullptr,	// No menus
		hInstance,
		nullptr);	// No additional command-line arguments

	// Initialize the DX12 application itself, not the window
	Initialize();

	ShowWindow(windowHandle, TRUE);

	// Main application loop
	MSG msg = {};
	while (msg.message != WM_QUIT)
	{
		if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	// Clean up all resources used by the application
	Destroy();

	return 0;
}