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

HWND tnt::wrapper::Window::GetWindowHandle() const
{
	return m_window_handle;
}
