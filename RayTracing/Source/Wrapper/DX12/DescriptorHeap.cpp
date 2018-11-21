#include "Wrapper/DX12/DescriptorHeap.hpp"

#include "Utility/CheckHResult.hpp"

tnt::wrapper::dx12::DescriptorHeap::DescriptorHeap()
{
}

tnt::wrapper::dx12::DescriptorHeap::~DescriptorHeap()
{
}

void tnt::wrapper::dx12::DescriptorHeap::Initialize(
	ID3D12Device* t_device,
	UINT t_descriptor_count,
	D3D12_DESCRIPTOR_HEAP_TYPE t_heap_type,
	D3D12_DESCRIPTOR_HEAP_FLAGS t_flags)
{
	D3D12_DESCRIPTOR_HEAP_DESC descriptor_heap_description = CreateDescriptorHeapDescription(t_descriptor_count, t_heap_type, t_flags);

	ThrowIfFailed(t_device->CreateDescriptorHeap(&descriptor_heap_description, IID_PPV_ARGS(&m_descriptor_heap)));
}

ID3D12DescriptorHeap * const tnt::wrapper::dx12::DescriptorHeap::GetDescriptorHeapPointer() const
{
	return m_descriptor_heap.Get();
}

D3D12_DESCRIPTOR_HEAP_DESC tnt::wrapper::dx12::DescriptorHeap::CreateDescriptorHeapDescription(
	UINT t_descriptor_count,
	D3D12_DESCRIPTOR_HEAP_TYPE t_heap_type,
	D3D12_DESCRIPTOR_HEAP_FLAGS t_flags)
{
	D3D12_DESCRIPTOR_HEAP_DESC descriptor_heap_desc = {};
	descriptor_heap_desc.NumDescriptors = t_descriptor_count;
	descriptor_heap_desc.Type = t_heap_type;
	descriptor_heap_desc.Flags = t_flags;

	return descriptor_heap_desc;
}
