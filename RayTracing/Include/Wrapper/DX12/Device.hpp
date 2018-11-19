#ifndef DEVICE_HPP
#define DEVICE_HPP

#include "Windows.h"
#include <wrl.h>
#include <d3d12.h>
#include <dxgi1_4.h>

namespace tnt
{
	namespace wrapper
	{
		namespace dx12
		{
			class Device
			{
			public:
				Device();
				~Device();

				void Initialize(
					D3D_FEATURE_LEVEL t_desired_feature_level = D3D_FEATURE_LEVEL_11_0,
					BOOL t_use_warp_adapter = FALSE,
					BOOL t_enable_debug_layer = FALSE,
					UINT t_flags = 0);

				ID3D12Device* const GetDevicePointer() const;

			private:
				void SetDebugLayer(BOOL t_enable_debug_layer) const;
				void EnableDebugLayer() const;
				void SetDeviceAdapter(BOOL t_use_warp_adapter, D3D_FEATURE_LEVEL t_desired_feature_level, UINT t_flags = 0);
				void CreateDeviceUsingWarpAdapter(IDXGIFactory4* t_factory, D3D_FEATURE_LEVEL t_desired_feature_level);
				void CreateDeviceUsingHardwareAdapter(IDXGIFactory4* t_factory, D3D_FEATURE_LEVEL t_desired_feature_level);
				void CreateDevice(IDXGIAdapter* t_adapter, D3D_FEATURE_LEVEL t_desired_feature_level);
				void GetHardwareAdapter(IDXGIFactory2* t_factory, IDXGIAdapter1** t_adapter, D3D_FEATURE_LEVEL t_desired_feature_level);

			private:
				Microsoft::WRL::ComPtr<ID3D12Device> m_device;
			};
		}
	}
}

#endif