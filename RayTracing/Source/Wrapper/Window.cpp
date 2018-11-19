#include "Wrapper/Window.hpp"

tnt::wrapper::Window::Window()
	: m_window_handle(nullptr)
{
}

tnt::wrapper::Window::~Window()
{
}

void tnt::wrapper::Window::Show() const
{
	ShowWindow(m_window_handle, TRUE);
}

void tnt::wrapper::Window::MainLoop()
{
	MSG msg = {};

	while (msg.message != WM_QUIT)
	{
		if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}
}

HWND tnt::wrapper::Window::GetWindowHandle() const
{
	return m_window_handle;
}
