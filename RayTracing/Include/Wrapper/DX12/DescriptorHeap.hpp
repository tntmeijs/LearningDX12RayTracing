#ifndef DESCRIPTOR_HEAP_HPP
#define DESCRIPTOR_HEAP_HPP

#include <wrl.h>
#include <d3d12.h>

namespace tnt
{
	namespace wrapper
	{
		namespace dx12
		{
			class DescriptorHeap
			{
			public:
				DescriptorHeap();
				~DescriptorHeap();

				void Initialize(
					ID3D12Device* t_device,
					UINT t_descriptor_count,
					D3D12_DESCRIPTOR_HEAP_TYPE t_heap_type,
					D3D12_DESCRIPTOR_HEAP_FLAGS t_flags);

				ID3D12DescriptorHeap* const GetDescriptorHeapPointer() const;

			private:
				D3D12_DESCRIPTOR_HEAP_DESC CreateDescriptorHeapDescription(
					UINT t_descriptor_count,
					D3D12_DESCRIPTOR_HEAP_TYPE t_heap_type,
					D3D12_DESCRIPTOR_HEAP_FLAGS t_flags);

			private:
				Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_descriptor_heap;
			};
		}
	}
}

#endif