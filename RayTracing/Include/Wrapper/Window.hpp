#ifndef WINDOW_HPP
#define WINDOW_HPP

#include <Windows.h>
#include <functional>

namespace tnt
{
	namespace wrapper
	{
		class Window
			{
			public:
				Window();
				~Window();
	
				template<typename Functor>
				void Create(HINSTANCE t_hinstance, UINT t_width, UINT t_height, Functor t_window_proc);
				void Show() const;
				void MainLoop();
	
				HWND GetWindowHandle() const;
	
			private:
				HWND m_window_handle;
			};
	
			template<typename Functor>
			inline void Window::Create(HINSTANCE t_hinstance, UINT t_width, UINT t_height, Functor t_window_proc)
			{
				WNDCLASSEX window_class = { 0 };
				window_class.cbSize = sizeof(WNDCLASSEX);
				window_class.style = CS_HREDRAW | CS_VREDRAW;
				window_class.lpfnWndProc = t_window_proc;
				window_class.hInstance = t_hinstance;
				window_class.hCursor = LoadCursor(nullptr, IDC_ARROW);
				window_class.lpszClassName = className;
				RegisterClassEx(&window_class);
	
				RECT window_rect = { 0, 0, static_cast<LONG>(t_width), static_cast<LONG>(t_height) };
				AdjustWindowRect(&window_rect, WS_OVERLAPPEDWINDOW, FALSE);
	
				m_window_handle = CreateWindow(
					window_class.lpszClassName,
					windowTitle,
					WS_OVERLAPPEDWINDOW,
					CW_USEDEFAULT,
					CW_USEDEFAULT,
					window_rect.right - window_rect.left,
					window_rect.bottom - window_rect.top,
					nullptr,	// No parent window
					nullptr,	// No menus
					t_hinstance,
					nullptr);	// No additional command-line arguments
			}
	}
}

#endif