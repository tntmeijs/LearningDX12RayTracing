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

int main()
{
    return 0;
}
