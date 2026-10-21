#include "pch.h"
#include "TestHarness.hpp"
#include "../Common/ShellColorRefresh.hpp"
#include <future>
#include <wil/resource.h>

using namespace OpenGlass;

namespace OpenGlassTests
{
	void TestShellColorRefresh()
	{
		// Real cross-thread window delivery, with a receiver modeling the audited
		// one-shot pending flag. No Explorer, private API or registry is touched.
		struct Receiver
		{
			DWORD color{};
			bool pending{ true }, reject{};
		};
		const auto instance = GetModuleHandleW(nullptr);
		constexpr auto className = L"OpenGlassTests.ColorRefresh";
		WNDCLASSW windowClass{};
		windowClass.hInstance = instance;
		windowClass.lpszClassName = className;
		windowClass.lpfnWndProc = [](HWND window, UINT message, WPARAM wparam, LPARAM lparam) -> LRESULT
		{
			if (message == WM_NCCREATE)
				SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(reinterpret_cast<CREATESTRUCTW*>(lparam)->lpCreateParams));
			auto* receiver = reinterpret_cast<Receiver*>(GetWindowLongPtrW(window, GWLP_USERDATA));
			if (message == 0x52C && wparam == 14 && receiver)
			{
				if (receiver->reject) return E_ACCESSDENIED;
				if (lparam == static_cast<LPARAM>(0xFFFFFFFFull) || receiver->pending) receiver->color = 0x123456;
				receiver->pending = false;
				return S_OK;
			}
			if (message == WM_APP && receiver) { receiver->color = 0x74B8FC; return 0; } // Sky
			if (message == WM_APP + 1 && receiver) return receiver->color;
			if (message == WM_APP + 2 && receiver) { receiver->reject = true; return 0; }
			if (message == WM_DESTROY) { PostQuitMessage(0); return 0; }
			return DefWindowProcW(window, message, wparam, lparam);
		};
		Check(RegisterClassW(&windowClass) != 0);
		auto unregister = wil::scope_exit([&] { UnregisterClassW(className, instance); });
		std::promise<HWND> ready;
		auto future = ready.get_future();
		std::jthread thread([&]
		{
			Receiver receiver;
			const auto window = CreateWindowExW(0, className, L"", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, instance, &receiver);
			ready.set_value(window);
			if (!window) return;
			MSG message{};
			while (GetMessageW(&message, nullptr, 0, 0) > 0) DispatchMessageW(&message);
		});
		const auto window = future.get();
		auto close = wil::scope_exit([&] { if (window) PostMessageW(window, WM_CLOSE, 0, 0); });
		Check(window != nullptr);
		if (!window) return;
		Check(SUCCEEDED(ShellColorRefresh::Request(window)));
		Check(SendMessageW(window, WM_APP + 1, 0, 0) == 0x123456);
		SendMessageW(window, WM_APP, 0, 0);
		// Reproduce the bug: ordinary refresh has no pending wallpaper result.
		SendMessageW(window, 0x52C, 14, 0);
		Check(SendMessageW(window, WM_APP + 1, 0, 0) == 0x74B8FC);
		Check(SUCCEEDED(ShellColorRefresh::Request(window)));
		Check(SendMessageW(window, WM_APP + 1, 0, 0) == 0x123456);
		SendMessageW(window, WM_APP, 0, 0);
		Check(SendMessageW(window, WM_APP + 1, 0, 0) == 0x74B8FC); // No late posted refresh.
		SendMessageW(window, WM_APP + 2, 0, 0);
		Check(ShellColorRefresh::Request(window) == E_ACCESSDENIED);
		Check(FAILED(ShellColorRefresh::Request(nullptr)));
	}
}
