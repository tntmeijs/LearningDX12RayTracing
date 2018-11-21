#include "Wrapper/DX12/SwapChain.hpp"

#include "Utility/CheckHResult.hpp"

tnt::wrapper::dx12::SwapChain::SwapChain()
	: m_back_buffer_count(0)
{
}

tnt::wrapper::dx12::SwapChain::~SwapChain()
{
}

void tnt::wrapper::dx12::SwapChain::Initialize(
	IDXGIFactory4* t_factory,
	ID3D12CommandQueue* t_command_queue,
	HWND t_window_handle,
	UINT t_width,
	UINT t_height,
	const DXGI_SAMPLE_DESC& t_sampler_description,
	BOOL t_allow_alt_enter,
	UINT t_number_of_back_buffers,
	DXGI_FORMAT t_format,
	DXGI_SWAP_EFFECT t_swap_effect)
{
	m_back_buffer_count = t_number_of_back_buffers;

	DXGI_SWAP_CHAIN_DESC1 swap_chain_desc = CreateSwapChainDescription(
		t_number_of_back_buffers,
		t_width,
		t_height,
		t_format,
		t_swap_effect,
		t_sampler_description);

	Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain_one;
	ThrowIfFailed(t_factory->CreateSwapChainForHwnd(
		t_command_queue,
		t_window_handle,
		&swap_chain_desc,
		nullptr,
		nullptr,
		&swap_chain_one));

	if (!t_allow_alt_enter)
	{
		ThrowIfFailed(t_factory->MakeWindowAssociation(t_window_handle, DXGI_MWA_NO_ALT_ENTER));
	}

	ThrowIfFailed(swap_chain_one.As(&m_swap_chain));
}

IDXGISwapChain3* const tnt::wrapper::dx12::SwapChain::GetSwapChainPointer() const
{
	return m_swap_chain.Get();
}

const UINT tnt::wrapper::dx12::SwapChain::GetBackBufferCount() const
{
	return m_back_buffer_count;
}

const UINT tnt::wrapper::dx12::SwapChain::GetCurrentBackBufferIndex() const
{
	return m_swap_chain->GetCurrentBackBufferIndex();
}

DXGI_SWAP_CHAIN_DESC1 tnt::wrapper::dx12::SwapChain::CreateSwapChainDescription(
	UINT t_number_of_buffers,
	UINT t_width,
	UINT t_height,
	DXGI_FORMAT t_format,
	DXGI_SWAP_EFFECT t_swap_effect,
	const DXGI_SAMPLE_DESC& t_sampler_description)
{
	DXGI_SWAP_CHAIN_DESC1 swap_chain_description = {};
	swap_chain_description.BufferCount = t_number_of_buffers;
	swap_chain_description.Width = t_width;
	swap_chain_description.Height = t_height;
	swap_chain_description.Format = t_format;
	swap_chain_description.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swap_chain_description.SwapEffect = t_swap_effect;
	swap_chain_description.SampleDesc = t_sampler_description;

	return swap_chain_description;
}
