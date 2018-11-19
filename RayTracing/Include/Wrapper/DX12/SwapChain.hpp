#ifndef SWAP_CHAIN_HPP
#define SWAP_CHAIN_HPP

#include <Windows.h>
#include <d3d12.h>
#include <wrl.h>
#include <dxgi1_4.h>

namespace tnt
{
	namespace wrapper
	{
		namespace dx12
		{
			class SwapChain
			{
			public:
				SwapChain();
				~SwapChain();

				void Initialize(
					IDXGIFactory4* t_factory,
					ID3D12CommandQueue* t_command_queue,
					HWND t_window_handle,
					UINT t_width,
					UINT t_height,
					const DXGI_SAMPLE_DESC& t_sampler_description,
					BOOL t_allow_alt_enter = FALSE,
					UINT t_number_of_buffers = 2,
					DXGI_FORMAT t_format = DXGI_FORMAT_R8G8B8A8_UNORM,
					DXGI_SWAP_EFFECT t_swap_effect = DXGI_SWAP_EFFECT_FLIP_DISCARD);

			private:
				DXGI_SWAP_CHAIN_DESC1 CreateSwapChainDescription(
					UINT t_number_of_buffers,
					UINT t_width,
					UINT t_height,
					DXGI_FORMAT t_format,
					DXGI_SWAP_EFFECT t_swap_effect,
					const DXGI_SAMPLE_DESC& t_sampler_description);

			private:
				UINT m_frame_index;

				Microsoft::WRL::ComPtr<IDXGISwapChain3> m_swap_chain;
			};
		}
	}
}

#endif