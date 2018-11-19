#include "Wrapper/DX12/Device.hpp"

#include "Utility/CheckHResult.hpp"

tnt::wrapper::dx12::Device::Device()
{
}

tnt::wrapper::dx12::Device::~Device()
{
}

void tnt::wrapper::dx12::Device::Initialize(
	IDXGIFactory4* t_factory,
	D3D_FEATURE_LEVEL t_desired_feature_level,
	BOOL t_use_warp_adapter,
	BOOL t_enable_debug_layer,
	UINT t_flags)
{
	SetDebugLayer(t_enable_debug_layer);
	SetDeviceAdapter(t_factory, t_use_warp_adapter, t_desired_feature_level);
}

ID3D12Device* const tnt::wrapper::dx12::Device::GetDevicePointer() const
{
	return m_device.Get();
}

void tnt::wrapper::dx12::Device::SetDebugLayer(BOOL t_enable_debug_layer) const
{
	if (t_enable_debug_layer)
	{
		EnableDebugLayer();
	}
}

void tnt::wrapper::dx12::Device::EnableDebugLayer() const
{
	Microsoft::WRL::ComPtr<ID3D12Debug> debug_controller;

	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug_controller))))
	{
		debug_controller->EnableDebugLayer();
	}
}

void tnt::wrapper::dx12::Device::SetDeviceAdapter(IDXGIFactory4* t_factory, BOOL t_use_warp_adapter, D3D_FEATURE_LEVEL t_desired_feature_level, UINT t_flags)
{
	if (t_use_warp_adapter)
	{
		CreateDeviceUsingWarpAdapter(t_factory, t_desired_feature_level);
	}
	else
	{
		CreateDeviceUsingHardwareAdapter(t_factory, t_desired_feature_level);
	}
}

void tnt::wrapper::dx12::Device::CreateDeviceUsingWarpAdapter(IDXGIFactory4* t_factory, D3D_FEATURE_LEVEL t_desired_feature_level)
{
	Microsoft::WRL::ComPtr<IDXGIAdapter> warp_adapter;
	ThrowIfFailed(t_factory->EnumWarpAdapter(IID_PPV_ARGS(&warp_adapter)));

	CreateDevice(warp_adapter.Get(), t_desired_feature_level);
}

void tnt::wrapper::dx12::Device::CreateDeviceUsingHardwareAdapter(IDXGIFactory4* t_factory, D3D_FEATURE_LEVEL t_desired_feature_level)
{
	Microsoft::WRL::ComPtr<IDXGIAdapter1> hardware_adapter;
	GetHardwareAdapter(t_factory, &hardware_adapter, t_desired_feature_level);

	CreateDevice(hardware_adapter.Get(), t_desired_feature_level);
}

void tnt::wrapper::dx12::Device::CreateDevice(IDXGIAdapter * t_adapter, D3D_FEATURE_LEVEL t_desired_feature_level)
{
	ThrowIfFailed(D3D12CreateDevice(t_adapter, t_desired_feature_level, IID_PPV_ARGS(&m_device)));
}

void tnt::wrapper::dx12::Device::GetHardwareAdapter(IDXGIFactory2* t_factory, IDXGIAdapter1** t_adapter, D3D_FEATURE_LEVEL t_desired_feature_level)
{
	Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter;
	*t_adapter = nullptr;

	for (UINT adapter_index = 0; DXGI_ERROR_NOT_FOUND != t_factory->EnumAdapters1(adapter_index, &adapter); ++adapter_index)
	{
		DXGI_ADAPTER_DESC1 adapter_desc = {};
		adapter->GetDesc1(&adapter_desc);

		if (adapter_desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
		{
			// Ignore the software (WARP) adapter
			continue;
		}

		// Only check for support, the device itself will be created later
		if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), t_desired_feature_level, __uuidof(ID3D12Device), nullptr)))
		{
			// Found a suitable adapter!
			break;
		}
	}

	*t_adapter = adapter.Detach();
}
