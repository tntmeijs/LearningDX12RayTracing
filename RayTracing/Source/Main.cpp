#include <Windows.h>

// Image loading
#include <stb_image.h>

// DX12
#include <d3d12.h>
#include <dxgi1_4.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
using namespace DirectX;

// DX12 wrappers
#include "Wrapper/Window.hpp"
#include "Wrapper/DX12/Device.hpp"
#include "Wrapper/DX12/SwapChain.hpp"
#include "Wrapper/DX12/DescriptorHeap.hpp"

// Need the ComPtr<t> for this application
#include <wrl.h>
using namespace Microsoft::WRL;

// Helper functions / structures provided by Microsoft
#include <d3dx12.h>

#include "Utility/CheckHResult.hpp"

struct Vertex
{
	XMFLOAT4 position;
	XMFLOAT2 texcoord;
};

struct SceneConstantBufferData
{
	XMFLOAT2 positionOffset;
};

SceneConstantBufferData constantBufferData;

HWND window_handle = nullptr;

const UINT BACK_BUFFER_COUNT = 3;
const UINT WINDOW_WIDTH = 1280;
const UINT WINDOW_HEIGHT = 720;

const FLOAT BACK_BUFFER_CLEAR_COLOR[] = { 0.392f, 0.584f, 0.929f, 0.0f };

UINT frameIndex = 0;
UINT rtvDescriptorSize = 0;
UINT cbvSrvDescriptorSize = 0;

UINT8* p_cbvDataBegin = nullptr;

UINT64 fenceValues[BACK_BUFFER_COUNT] = {};

HANDLE fenceEvent = nullptr;

// DX12 objects
IDXGISwapChain3* swap_chain_pointer = nullptr;

CD3DX12_VIEWPORT viewport(0.0f, 0.0f, static_cast<FLOAT>(WINDOW_WIDTH), static_cast<FLOAT>(WINDOW_HEIGHT));
CD3DX12_RECT scissorRect(0, 0, static_cast<LONG>(WINDOW_WIDTH), static_cast<LONG>(WINDOW_HEIGHT));

D3D12_VERTEX_BUFFER_VIEW vertexBufferView = {};

tnt::wrapper::dx12::DescriptorHeap rtvHeap;
tnt::wrapper::dx12::DescriptorHeap cbvSrvHeap;

ComPtr<ID3D12Resource> renderTargets[BACK_BUFFER_COUNT];
ComPtr<ID3D12CommandQueue> graphicsCommandQueue;
ComPtr<ID3D12CommandAllocator> graphicsCommandAllocators[BACK_BUFFER_COUNT];
ComPtr<ID3D12CommandAllocator> bundleCommandAllocator;
ComPtr<ID3D12GraphicsCommandList> graphicsCommandList;
ComPtr<ID3D12GraphicsCommandList> bundleCommandList;
ComPtr<ID3D12Fence> fence;
ComPtr<ID3D12PipelineState> graphicsPipelineStateObject;
ComPtr<ID3D12RootSignature> rootSignature;
ComPtr<ID3D12Resource> vertexBuffer;
ComPtr<ID3D12Resource> texture;
ComPtr<ID3D12Resource> constantBuffer;

void WaitForGPU()
{
	// Schedule a signal in the queue
	ThrowIfFailed(graphicsCommandQueue->Signal(fence.Get(), fenceValues[frameIndex]));

	// Wait until the fence has been processed
	ThrowIfFailed(fence->SetEventOnCompletion(fenceValues[frameIndex], fenceEvent));
	WaitForSingleObjectEx(fenceEvent, INFINITE, FALSE);

	++fenceValues[frameIndex];
}

void PrepareNextFrame()
{
	// Schedule a command in the queue
	const UINT64 currentFenceValue = fenceValues[frameIndex];
	ThrowIfFailed(graphicsCommandQueue->Signal(fence.Get(), currentFenceValue));

	frameIndex = swap_chain_pointer->GetCurrentBackBufferIndex();

	// If the next frame is not ready to b erendered yet, wait for it
	if (fence->GetCompletedValue() < fenceValues[frameIndex])
	{
		ThrowIfFailed(fence->SetEventOnCompletion(fenceValues[frameIndex], fenceEvent));
		WaitForSingleObjectEx(fenceEvent, INFINITE, FALSE);
	}

	// Set the fence value for the next frame
	fenceValues[frameIndex] = currentFenceValue + 1;
}

void PopulateCommandList()
{
	// This can only happen when the associated command lists have finished execution on the GPU
	ThrowIfFailed(graphicsCommandAllocators[frameIndex]->Reset());

	// Command lists can be reset at any time as long as execute has been called on it
	ThrowIfFailed(graphicsCommandList->Reset(graphicsCommandAllocators[frameIndex].Get(), graphicsPipelineStateObject.Get()));

	// All descriptor heaps needed for the graphics command list
	ID3D12DescriptorHeap* ppDescriptorheaps[] = { cbvSrvHeap.GetDescriptorHeapPointer() };

	CD3DX12_GPU_DESCRIPTOR_HANDLE cbvSrvHeapHandle(cbvSrvHeap.GetDescriptorHeapPointer()->GetGPUDescriptorHandleForHeapStart());

	// Set the correct states
	graphicsCommandList->SetGraphicsRootSignature(rootSignature.Get());
	graphicsCommandList->SetDescriptorHeaps(_countof(ppDescriptorheaps), ppDescriptorheaps);
	graphicsCommandList->SetGraphicsRootDescriptorTable(0, cbvSrvHeapHandle);

	cbvSrvHeapHandle.Offset(1, cbvSrvDescriptorSize);

	graphicsCommandList->SetGraphicsRootDescriptorTable(1, cbvSrvHeapHandle);
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
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(rtvHeap.GetDescriptorHeapPointer()->GetCPUDescriptorHandleForHeapStart(), frameIndex, rtvDescriptorSize);

	// Set the current back buffer as the render target
	graphicsCommandList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

	// Record commands
	graphicsCommandList->ClearRenderTargetView(rtvHandle, BACK_BUFFER_CLEAR_COLOR, 0, nullptr);

	// Execute commands stored in the bundle
	graphicsCommandList->ExecuteBundle(bundleCommandList.Get());

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
	UINT flags = 0;
	flags |= DXGI_CREATE_FACTORY_DEBUG;

	Microsoft::WRL::ComPtr<IDXGIFactory4> factory;
	ThrowIfFailed(CreateDXGIFactory2(flags, IID_PPV_ARGS(&factory)));

	tnt::wrapper::dx12::Device device;
	device.Initialize(factory.Get(), D3D_FEATURE_LEVEL_11_0, FALSE, TRUE, flags);

	ID3D12Device* device_pointer = device.GetDevicePointer();

#pragma region PIPELINE_SET_UP
	{
		// === ============= ===
		// === COMMAND QUEUE ===
		// === ============= ===
		D3D12_COMMAND_QUEUE_DESC commandQueueDesc = {};
		commandQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
		commandQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

		ThrowIfFailed(device_pointer->CreateCommandQueue(&commandQueueDesc, IID_PPV_ARGS(&graphicsCommandQueue)));

		// === ========== ===
		// === SWAP CHAIN ===
		// === ========== ===
		DXGI_SAMPLE_DESC sampler_desc = {};
		sampler_desc.Count = 1;

		tnt::wrapper::dx12::SwapChain swap_chain;
		swap_chain.Initialize(
			factory.Get(),
			graphicsCommandQueue.Get(),
			window_handle,
			WINDOW_WIDTH,
			WINDOW_HEIGHT, 
			sampler_desc,
			FALSE,
			BACK_BUFFER_COUNT);

		swap_chain_pointer = swap_chain.GetSwapChainPointer();
		frameIndex = swap_chain_pointer->GetCurrentBackBufferIndex();

		// === ================ ===
		// === DESCRIPTOR HEAPS ===
		// === ================ ===
		{
			rtvHeap.Initialize(
				device_pointer,
				BACK_BUFFER_COUNT,
				D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
				D3D12_DESCRIPTOR_HEAP_FLAG_NONE);

			cbvSrvHeap.Initialize(
				device_pointer,
				2,
				D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
				D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE);

			rtvDescriptorSize = device_pointer->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
			cbvSrvDescriptorSize = device_pointer->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		}

		// === =============== ===
		// === FRAME RESOURCES ===
		// === =============== ===
		{
			CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(rtvHeap.GetDescriptorHeapPointer()->GetCPUDescriptorHandleForHeapStart());

			// Create a new render target view for each frame
			for (UINT n = 0; n < BACK_BUFFER_COUNT; ++n)
			{
				ThrowIfFailed(swap_chain_pointer->GetBuffer(n, IID_PPV_ARGS(&renderTargets[n])));

				device_pointer->CreateRenderTargetView(renderTargets[n].Get(), nullptr, rtvHandle);
				rtvHandle.Offset(1, rtvDescriptorSize);

				// Also create a command allocator per frame
				ThrowIfFailed(device_pointer->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&graphicsCommandAllocators[n])));
			}
		}

		// === ======================== ===
		// === BUNDLE COMMAND ALLOCATOR ===
		// === ======================== ===
		ThrowIfFailed(device_pointer->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_BUNDLE, IID_PPV_ARGS(&bundleCommandAllocator)));
	}
#pragma endregion

#pragma region ASSET_LOADING
	{
		// Create the root signature
		{
			D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};
			featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;

			if (FAILED(device_pointer->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData))))
			{
				featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
			}

			CD3DX12_DESCRIPTOR_RANGE1 ranges[2];
			ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
			ranges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);

			CD3DX12_ROOT_PARAMETER1 rootParameters[2];
			rootParameters[0].InitAsDescriptorTable(1, &ranges[0], D3D12_SHADER_VISIBILITY_PIXEL);
			rootParameters[1].InitAsDescriptorTable(1, &ranges[1], D3D12_SHADER_VISIBILITY_VERTEX);

			D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
				D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
				D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
				D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
				D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

			D3D12_STATIC_SAMPLER_DESC samplerDesc = {};
			samplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
			samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
			samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
			samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
			samplerDesc.MipLODBias = 0;
			samplerDesc.MaxAnisotropy = 0;
			samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
			samplerDesc.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
			samplerDesc.MinLOD = 0.0f;
			samplerDesc.MaxLOD = D3D12_FLOAT32_MAX;
			samplerDesc.ShaderRegister = 0;
			samplerDesc.RegisterSpace = 0;
			samplerDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

			CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
			rootSignatureDesc.Init_1_1(_countof(rootParameters), rootParameters, 1, &samplerDesc, rootSignatureFlags);

			ComPtr<ID3DBlob> signature;
			ComPtr<ID3DBlob> error;

			ThrowIfFailed(D3DX12SerializeVersionedRootSignature(&rootSignatureDesc, featureData.HighestVersion, &signature, &error));
			ThrowIfFailed(device_pointer->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&rootSignature)));
		}

		// Create the pipeline state (also compiles / loads the shaders)
		{
			ComPtr<ID3DBlob> vertexShader;
			ComPtr<ID3DBlob> pixelShader;

			UINT compileFlags = 0;

#if defined(_DEBUG)
			compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

			// Compile the shaders
			ThrowIfFailed(D3DCompileFromFile(L"./Resources/Shaders/simple_shader.hlsl", nullptr, nullptr, "vs_main", "vs_5_0", compileFlags, 0, &vertexShader, nullptr));
			ThrowIfFailed(D3DCompileFromFile(L"./Resources/Shaders/simple_shader.hlsl", nullptr, nullptr, "ps_main", "ps_5_0", compileFlags, 0, &pixelShader, nullptr));

			// Define the vertex input layout
			D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
			{
				{ "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
				{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 16, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
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

			ThrowIfFailed(device_pointer->CreateGraphicsPipelineState(&graphicsPipelineStateObjectDesc, IID_PPV_ARGS(&graphicsPipelineStateObject)));
		}

		// Defaults to a recording state
		ThrowIfFailed(device_pointer->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, graphicsCommandAllocators[frameIndex].Get(), nullptr, IID_PPV_ARGS(&graphicsCommandList)));

		// === ============= ===
		// === VERTEX BUFFER ===
		// === ============= ===
		{
			Vertex vertices[] =
			{
				{ {  0.0f,  0.5f, 0.0f, 1.0f }, { 0.5f, 0.0f } },
				{ {  0.5f, -0.5f, 0.0f, 1.0f }, { 1.0f, 1.0f } },
				{ { -0.5f, -0.5f, 0.0f, 1.0f }, { 0.0f, 1.0f } }
			};

			const UINT vertexBufferSize = sizeof(vertices);

			// TODO: read on default heap usage
			ThrowIfFailed(device_pointer->CreateCommittedResource(
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

		// Pointer to the start of the CBV / SRV heap
		CD3DX12_CPU_DESCRIPTOR_HANDLE cbvSrvHandle(cbvSrvHeap.GetDescriptorHeapPointer()->GetCPUDescriptorHandleForHeapStart());

		// === ======== ===
		// === TEXTURES ===
		// === ======== ===

		ComPtr<ID3D12Resource> textureUploadHeap;

		// Load the texture
		{
			int width, height, channelCount;
			unsigned char* imageData = stbi_load("./Resources/Textures/basic_test_texture.png", &width, &height, &channelCount, STBI_rgb_alpha);

			D3D12_RESOURCE_DESC textureDesc = {};
			textureDesc.MipLevels = 1;
			textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			textureDesc.Width = width;
			textureDesc.Height = height;
			textureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
			textureDesc.DepthOrArraySize = 1;
			textureDesc.SampleDesc.Count = 1;
			textureDesc.SampleDesc.Quality = 0;
			textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

			ThrowIfFailed(device_pointer->CreateCommittedResource(
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
				D3D12_HEAP_FLAG_NONE,
				&textureDesc,
				D3D12_RESOURCE_STATE_COPY_DEST,
				nullptr,
				IID_PPV_ARGS(&texture)
			));

			const UINT64 uploadBufferSize = GetRequiredIntermediateSize(texture.Get(), 0, 1);

			// Create the GPU upload buffer
			ThrowIfFailed(device_pointer->CreateCommittedResource(
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
				D3D12_HEAP_FLAG_NONE,
				&CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize),
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				IID_PPV_ARGS(&textureUploadHeap)
			));

			// Copy the data to the intermediate upload heap and schedule a copy from the upload heap to the Texture2D
			D3D12_SUBRESOURCE_DATA textureSubresouceData = {};
			textureSubresouceData.pData = imageData;
			textureSubresouceData.RowPitch = width * STBI_rgb_alpha;	// Number of rows * number of bytes per pixel
			textureSubresouceData.SlicePitch = textureSubresouceData.RowPitch * height;

			UpdateSubresources(graphicsCommandList.Get(), texture.Get(), textureUploadHeap.Get(), 0, 0, 1, &textureSubresouceData);
			graphicsCommandList->ResourceBarrier(
				1,
				&CD3DX12_RESOURCE_BARRIER::Transition(
					texture.Get(),
					D3D12_RESOURCE_STATE_COPY_DEST,
					D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)
			);

			stbi_image_free(imageData);

			// Describe and create a SRV for the texture
			D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
			srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			srvDesc.Format = textureDesc.Format;
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			srvDesc.Texture2D.MipLevels = 1;

			device_pointer->CreateShaderResourceView(texture.Get(), &srvDesc, cbvSrvHandle);	// Resource 0
			cbvSrvHandle.Offset(1, cbvSrvDescriptorSize);
		}

		// Close the command list and execute the commands
		ThrowIfFailed(graphicsCommandList->Close());
		ID3D12CommandList* ppCommandLists[] = { graphicsCommandList.Get() };
		graphicsCommandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

		// Create the constant buffer
		{
			ThrowIfFailed(device_pointer->CreateCommittedResource(
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
				D3D12_HEAP_FLAG_NONE,
				&CD3DX12_RESOURCE_DESC::Buffer(1024 * 64),
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				IID_PPV_ARGS(&constantBuffer)
			));

			D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
			cbvDesc.BufferLocation = constantBuffer->GetGPUVirtualAddress();
			cbvDesc.SizeInBytes = (sizeof(SceneConstantBufferData) + 255) & ~255;	// Align the constant buffer to 256 bytes

			device_pointer->CreateConstantBufferView(&cbvDesc, cbvSrvHandle);	// Resource 1
			cbvSrvHandle.Offset(1, cbvSrvDescriptorSize);

			// Keep it mapped until the application closes
			// It is okay to keep things mapped for the lifetime of the resource
			CD3DX12_RANGE readRange(0, 0);
			ThrowIfFailed(constantBuffer->Map(0, &readRange, reinterpret_cast<void**>(&p_cbvDataBegin)));
			memcpy(p_cbvDataBegin, &constantBufferData, sizeof(constantBufferData));
		}

		// Create and record the bundle
		{
			ThrowIfFailed(device_pointer->CreateCommandList(
				0,
				D3D12_COMMAND_LIST_TYPE_BUNDLE,
				bundleCommandAllocator.Get(),
				graphicsPipelineStateObject.Get(),
				IID_PPV_ARGS(&bundleCommandList)
			));

			bundleCommandList->SetGraphicsRootSignature(rootSignature.Get());
			bundleCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			bundleCommandList->IASetVertexBuffers(0, 1, &vertexBufferView);
			bundleCommandList->DrawInstanced(3, 1, 0, 0);

			ThrowIfFailed(bundleCommandList->Close());
		}

		// Synchronization objects
		{
			ThrowIfFailed(device_pointer->CreateFence(fenceValues[frameIndex], D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)));
			++fenceValues[frameIndex];

			// Even handle for frame synchronization
			fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);

			if (fenceEvent == nullptr)
			{
				ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
			}

			// Wait for the setup to complete...
			WaitForGPU();
		}
	}
#pragma endregion
}

void Update()
{
	const float scrollSpeed = 0.0075f;
	const float offsetBounds = 1.5f;

	constantBufferData.positionOffset.x += scrollSpeed;

	if (constantBufferData.positionOffset.x > offsetBounds)
	{
		constantBufferData.positionOffset.x = -offsetBounds;
	}

	// Update the data in the constant buffer
	memcpy(p_cbvDataBegin, &constantBufferData, sizeof(constantBufferData));
}

void Render()
{
	// Record all commands to render the scene
	PopulateCommandList();

	// Execute said commands
	ID3D12CommandList* ppCommandLists[] = { graphicsCommandList.Get() };
	graphicsCommandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

	// Present the frame (using v-sync)
	ThrowIfFailed(swap_chain_pointer->Present(1, 0));
	
	PrepareNextFrame();
}

void Destroy()
{
	// Make sure that the GPU is no longer using any of the resources that are about to be deallocated
	WaitForGPU();

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
	HINSTANCE hinstance = GetModuleHandle(nullptr);

	tnt::wrapper::Window window;
	window.Create(L"Learning DX12 Ray Tracing | Tahar Meijs", hinstance, 1280, 720, WindowProc);

	window_handle = window.GetWindowHandle();

	// Initialize the DX12 application itself, not the window
	Initialize();

	window.Show();
	window.MainLoop();

	// Clean up all resources used by the application
	Destroy();

	return 0;
}