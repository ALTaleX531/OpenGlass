#include "pch.h"
#include "Benchmark.hpp"
#include <shellapi.h>
#include <cstdio>

using namespace OpenGlass::RenderTest;

namespace
{
	constexpr wchar_t Help[] =
		L"OpenGlassRenderTest: D2D / D2D rects / D3D / Gaussian (Aero)\n\n"
		L"1/2/3/4 select; Space cycle; A auto; R radius 0/6/30; O occlusion;\n"
		L"F BGRA8/FP16; C force background copy;\n"
		L"Q Speed/Balanced/Quality; +/- D2D blur; P fractional phase; S reset stats.\n\n"
		L"--benchmark PATH.json [--frames 120] [--warmup 12] [--validate]\n"
		L"--adapter INDEX --width 1600 --height 1000 --radius 6 --occlusion 0\n"
		L"--blur 3 --optimization speed|balanced|quality --format bgra8|fp16\n"
		L"--mode d2d|rects|d3d|gaussian (initial interactive mode); --phase 0.5\n"
		L"--copy --cpu-only --debug --capture FRAMES_PER_MODE\n\n"
		L"Benchmark is offscreen; capture opens a finite annotated comparison. Both run all four modes.\n"
		L"Background: OpenGlassRenderTest.png in the working directory.\n"
		L"CPU = Flush + Render + EndDraw. GPU = timestamp span. D3D sigma is fixed at 3.\n"
		L"Gaussian uses Scale 0.5, configured sigma at half size, and 2x output.\n";

	class Application
	{
		HWND m_window{};
		Benchmark m_benchmark;
		Method m_method;
		bool m_auto{};
		UINT m_captureFrame{};
		std::chrono::steady_clock::time_point m_switch{}, m_refresh{};
		winrt::com_ptr<IDXGISwapChain1> m_swapChain;
		winrt::com_ptr<ID2D1Bitmap1> m_display;
		winrt::com_ptr<ID2D1SolidColorBrush> m_brush;
		winrt::com_ptr<IDWriteTextFormat> m_textFormat;
		std::wstring m_overlay;

		void CreateDisplay(UINT width, UINT height)
		{
			auto* context = m_benchmark.Context();
			if (!m_swapChain)
			{
				winrt::com_ptr<IDXGIDevice> device;
				THROW_IF_FAILED(m_benchmark.Device()->QueryInterface(device.put()));
				winrt::com_ptr<IDXGIAdapter> adapter;
				THROW_IF_FAILED(device->GetAdapter(adapter.put()));
				winrt::com_ptr<IDXGIFactory2> factory;
				THROW_IF_FAILED(adapter->GetParent(IID_PPV_ARGS(factory.put())));
				DXGI_SWAP_CHAIN_DESC1 desc{};
				desc.Width = width; desc.Height = height;
				desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
				desc.SampleDesc.Count = 1;
				desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
				desc.BufferCount = 2;
				desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
				desc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
				THROW_IF_FAILED(factory->CreateSwapChainForHwnd(m_benchmark.Device(), m_window, &desc, nullptr, nullptr, m_swapChain.put()));
			}
			else
			{
				context->SetTarget(nullptr);
				m_display = nullptr;
				THROW_IF_FAILED(m_swapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0));
			}
			winrt::com_ptr<IDXGISurface> surface;
			THROW_IF_FAILED(m_swapChain->GetBuffer(0, IID_PPV_ARGS(surface.put())));
			const auto props = D2D1::BitmapProperties1(D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
				D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE), 96.f, 96.f);
			THROW_IF_FAILED(context->CreateBitmapFromDxgiSurface(surface.get(), &props, m_display.put()));
		}

		void UpdateOverlay()
		{
			const auto& options = m_benchmark.Configuration();
			constexpr const wchar_t* optimizations[]{ L"Speed", L"Balanced", L"Quality" };
			wchar_t header[640];
			swprintf_s(header, L"%s  %ux%u %s  Aero\nMode: %s  Auto:%s  D2D blur:%.2f  Radius:%u  Occlusion:%u  Phase:%.2f\nInput: %s  D2D optimization:%s\n\nMethod      CPU median/p95 (us)  GPU span (us)    CPU/GPU samples\n",
				m_benchmark.AdapterName().c_str(), options.width, options.height, options.fp16 ? L"FP16" : L"BGRA8",
				MethodLabels[size_t(m_method)], m_auto ? L"on" : L"off", options.settings.blurAmount, options.radius,
				options.occlusion, options.phase, options.settings.forceCapture ? L"copy for all" : L"production policy",
				optimizations[options.settings.optimization]);
			m_overlay = header;
			const auto formatTime = [](const std::vector<double>& samples, double percentile)
			{
				return samples.empty() ? std::wstring{ L"--" } : std::format(L"{:.1f}", Percentile(samples, percentile));
			};
			for (size_t i = 0; i < MethodCount; ++i)
			{
				const auto& stats = m_benchmark.Stats(Method(i));
				m_overlay += std::format(L"{:<10} {:>8} / {:<8} {:>10}    {}/{}\n", MethodLabels[i],
					formatTime(stats.cpu, .5), formatTime(stats.cpu, .95), formatTime(stats.gpu, .5), stats.cpu.size(), stats.gpu.size());
			}
			wchar_t footer[512];
			swprintf_s(footer, L"\nRectangles:%u  D3D sigma:3 (fixed)\nD2D: production four-stage / native selection\nD2D rects: pre-batching copy and draw policy\nGaussian sigma: %.2f half-size / %.2f full-size (nominal)\n\n1/2/3/4 select  Space cycle  A auto  R radius  O occlusion\nF format  C copy  Q quality  +/- D2D blur  P phase  S reset\nGPU span includes CPU-feed idle. --validate exports pixel differences.",
				m_benchmark.RectangleCount(), options.settings.blurAmount, options.settings.blurAmount * 2.f);
			m_overlay += footer;
			std::wstring title = L"OpenGlassRenderTest - ";
			title += MethodLabels[size_t(m_method)];
			SetWindowTextW(m_window, title.c_str());
		}
	public:
		explicit Application(const Options& options) : m_benchmark(options), m_method(options.method) {}
		void Initialize(HWND window)
		{
			m_window = window;
			m_benchmark.Initialize();
			const auto& options = m_benchmark.Configuration();
			CreateDisplay(options.width, options.height);
			THROW_IF_FAILED(m_benchmark.Context()->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White), m_brush.put()));
			winrt::com_ptr<IDWriteFactory> writeFactory;
			THROW_IF_FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), reinterpret_cast<IUnknown**>(writeFactory.put())));
			THROW_IF_FAILED(writeFactory->CreateTextFormat(L"Consolas", nullptr, DWRITE_FONT_WEIGHT_REGULAR,
				DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 15.f, L"", m_textFormat.put()));
			m_switch = std::chrono::steady_clock::now();
			UpdateOverlay();
		}
		void Resize(UINT width, UINT height)
		{
			if (!m_swapChain || !width || !height) return;
			auto options = m_benchmark.Configuration();
			options.width = width;
			options.height = height;
			m_benchmark.Reconfigure(options);
			CreateDisplay(width, height);
			UpdateOverlay();
		}
		void Key(WPARAM key)
		{
			auto options = m_benchmark.Configuration();
			if (key >= '1' && key < '1' + MethodCount)
			{
				m_method = Method(key - '1');
				m_auto = false;
			}
			else if (key == VK_SPACE)
			{
				m_method = Method((size_t(m_method) + 1) % MethodCount);
				m_auto = false;
			}
			else if (key == 'A')
			{
				m_auto = !m_auto;
				m_switch = std::chrono::steady_clock::now();
			}
			else if (key == 'S') m_benchmark.ResetMeasurements();
			else
			{
				if (key == 'R') options.radius = options.radius == 0 ? 6 : options.radius == 6 ? 30 : 0;
				else if (key == 'O') options.occlusion = (options.occlusion + 1) % 3;
				else if (key == 'F') options.fp16 = !options.fp16;
				else if (key == 'C') options.settings.forceCapture = !options.settings.forceCapture;
				else if (key == 'P') options.phase = options.phase == 0.f ? .5f : 0.f;
				else if (key == 'Q') options.settings.optimization = D2D1_GAUSSIANBLUR_OPTIMIZATION((int(options.settings.optimization) + 1) % 3);
				else if (key == VK_ADD || key == VK_OEM_PLUS) options.settings.blurAmount = std::min(100.f, options.settings.blurAmount + 1.f);
				else if (key == VK_SUBTRACT || key == VK_OEM_MINUS) options.settings.blurAmount = std::max(1.f, options.settings.blurAmount - 1.f);
				else return;
				m_benchmark.Reconfigure(options);
			}
			UpdateOverlay();
		}
		void Render()
		{
			const auto now = std::chrono::steady_clock::now();
			const auto& options = m_benchmark.Configuration();
			if (options.captureFrames)
			{
				const auto count = options.warmup + options.captureFrames;
				if (m_captureFrame >= count * MethodCount)
				{
					PostMessageW(m_window, WM_CLOSE, 0, 0);
					return;
				}
				m_method = Method(m_captureFrame++ / count);
			}
			else if (m_auto && now - m_switch >= std::chrono::seconds(3))
			{
				m_method = Method((size_t(m_method) + 1) % MethodCount);
				m_switch = now;
			}
			m_benchmark.Frame(m_method);
			if (now - m_refresh >= std::chrono::milliseconds(500))
			{
				UpdateOverlay();
				m_refresh = now;
			}
			auto* context = m_benchmark.Context();
			const auto preview = m_benchmark.PreparePreview();
			context->SetTarget(m_display.get());
			context->SetTransform(D2D1::IdentityMatrix());
			context->BeginDraw();
			context->DrawBitmap(preview);
			const float dpiScale = GetDpiForWindow(m_window) / 96.f;
			context->SetTransform(D2D1::Matrix3x2F::Scale(dpiScale, dpiScale));
			m_brush->SetColor(D2D1::ColorF(0.f, 0.f, 0.f, .82f));
			context->FillRectangle(D2D1::RectF(12.f, 12.f, 700.f, 390.f), m_brush.get());
			m_brush->SetColor(D2D1::ColorF(D2D1::ColorF::White));
			context->DrawTextW(m_overlay.c_str(), UINT(m_overlay.size()), m_textFormat.get(),
				D2D1::RectF(22.f, 20.f, 690.f, 385.f), m_brush.get(), D2D1_DRAW_TEXT_OPTIONS_NO_SNAP);
			THROW_IF_FAILED(context->EndDraw());
			THROW_IF_FAILED(m_swapChain->Present(0, 0));
		}
	};

	LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
	{
		auto* app = reinterpret_cast<Application*>(GetWindowLongPtrW(window, GWLP_USERDATA));
		try
		{
			switch (message)
			{
			case WM_SIZE:
				if (app) app->Resize(LOWORD(lparam), HIWORD(lparam));
				return 0;
			case WM_KEYDOWN:
				if (app) app->Key(wparam);
				return 0;
			case WM_GETMINMAXINFO:
				reinterpret_cast<MINMAXINFO*>(lparam)->ptMinTrackSize = { 560, 400 };
				return 0;
			case WM_PAINT:
			{
				PAINTSTRUCT paint;
				BeginPaint(window, &paint);
				EndPaint(window, &paint);
				return 0;
			}
			case WM_DESTROY:
				PostQuitMessage(0);
				return 0;
			}
		}
		catch (...)
		{
			LOG_CAUGHT_EXCEPTION();
			PostQuitMessage(1);
			return 0;
		}
		return DefWindowProcW(window, message, wparam, lparam);
	}
}

int APIENTRY wWinMain(HINSTANCE instance, HINSTANCE, LPWSTR, int show)
try
{
	int argc{};
	auto** argv = CommandLineToArgvW(GetCommandLineW(), &argc);
	THROW_LAST_ERROR_IF_NULL(argv);
	const auto freeArgs = wil::scope_exit([&] { LocalFree(argv); });
	if (argc == 2 && std::wstring_view(argv[1]) == L"--help")
	{
		const auto text = winrt::to_string(Help);
		DWORD written{};
		if (!WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), text.data(), DWORD(text.size()), &written, nullptr))
			MessageBoxW(nullptr, Help, L"OpenGlassRenderTest", MB_OK);
		return 0;
	}
	const auto options = ParseOptions(argc, argv);
	SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
	winrt::init_apartment(winrt::apartment_type::single_threaded);
	const auto uninitialize = wil::scope_exit([] { winrt::uninit_apartment(); });
	if (!options.output.empty())
	{
		Benchmark benchmark(options);
		benchmark.Initialize();
		benchmark.Run();
		return 0;
	}
	const wchar_t* className = L"OpenGlassRenderTestWindow";
	WNDCLASSEXW wc{ sizeof(wc) };
	wc.lpfnWndProc = WindowProc;
	wc.hInstance = instance;
	wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
	wc.lpszClassName = className;
	THROW_LAST_ERROR_IF(!RegisterClassExW(&wc));
	RECT rect{ 0, 0, LONG(options.width), LONG(options.height) };
	THROW_IF_WIN32_BOOL_FALSE(AdjustWindowRectEx(&rect, WS_OVERLAPPEDWINDOW, FALSE, 0));
	const auto window = CreateWindowExW(0, className, L"OpenGlassRenderTest", WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT, rect.right - rect.left, rect.bottom - rect.top, nullptr, nullptr, instance, nullptr);
	THROW_LAST_ERROR_IF_NULL(window);
	Application app(options);
	app.Initialize(window);
	SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(&app));
	ShowWindow(window, show);
	MSG message{};
	for (;;)
	{
		while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE))
		{
			if (message.message == WM_QUIT) return int(message.wParam);
			TranslateMessage(&message);
			DispatchMessageW(&message);
		}
		if (IsIconic(window)) WaitMessage();
		else app.Render();
	}
}
catch (const wil::ResultException& error)
{
	OutputDebugStringA(error.what());
	fprintf(stderr, "%s\n", error.what());
	return 1;
}
catch (...)
{
	const auto hr = wil::ResultFromCaughtException();
	wchar_t text[100];
	swprintf_s(text, L"OpenGlassRenderTest failed: 0x%08X\n", unsigned(hr));
	OutputDebugStringW(text);
	fwprintf(stderr, L"%s", text);
	return 1;
}
