#pragma once

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <exception>

inline void ThrowIfFailed(HRESULT hresult)
{
	if (FAILED(hresult))
	{
		throw std::exception();
	}
}