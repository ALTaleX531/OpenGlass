#include "pch.h"
#include "Benchmark.hpp"
#include "BlurSettings.hpp"
#include <DirectXPackedVector.h>
#include <d3d11sdklayers.h>
#include <fstream>
#include <random>

using namespace OpenGlass::RenderTest;
using Json = nlohmann::json;
using Clock = std::chrono::steady_clock;

Options OpenGlass::RenderTest::ParseOptions(int argc, wchar_t** argv)
{
	Options options;
	for (int i = 1; i < argc; ++i)
	{
		const std::wstring_view arg{ argv[i] };
		if (arg == L"--debug") options.debug = true;
		else if (arg == L"--cpu-only") options.cpuOnly = true;
		else if (arg == L"--validate") options.validate = true;
		else if (arg == L"--copy") options.settings.forceCapture = true;
		else
		{
			if (++i == argc) throw std::invalid_argument("Missing option value");
			const std::wstring value{ argv[i] };
			if (arg == L"--benchmark") options.output = value;
			else if (arg == L"--width") options.width = std::stoul(value);
			else if (arg == L"--height") options.height = std::stoul(value);
			else if (arg == L"--adapter") options.adapter = std::stoul(value);
			else if (arg == L"--radius") options.radius = std::stoul(value);
			else if (arg == L"--occlusion") options.occlusion = std::stoul(value);
			else if (arg == L"--frames") options.frames = std::stoul(value);
			else if (arg == L"--warmup") options.warmup = std::stoul(value);
			else if (arg == L"--capture") options.captureFrames = std::stoul(value);
			else if (arg == L"--blur") options.settings.blurAmount = std::stof(value);
			else if (arg == L"--phase") options.phase = std::stof(value);
			else if (arg == L"--format" && (value == L"bgra8" || value == L"fp16")) options.fp16 = value == L"fp16";
			else if (arg == L"--mode" && value == L"d2d") options.method = Method::D2D;
			else if (arg == L"--mode" && value == L"rects") options.method = Method::Rects;
			else if (arg == L"--mode" && value == L"d3d") options.method = Method::D3D;
			else if (arg == L"--mode" && value == L"gaussian") options.method = Method::Gaussian;
			else if (arg == L"--optimization" && value == L"balanced") options.settings.optimization = D2D1_GAUSSIANBLUR_OPTIMIZATION_BALANCED;
			else if (arg == L"--optimization" && value == L"quality") options.settings.optimization = D2D1_GAUSSIANBLUR_OPTIMIZATION_QUALITY;
			else if (arg == L"--optimization" && value == L"speed") options.settings.optimization = D2D1_GAUSSIANBLUR_OPTIMIZATION_SPEED;
			else throw std::invalid_argument("Unknown option or value");
		}
	}
	if (options.width < 320 || options.height < 240 || options.width > 7680 || options.height > 4320 ||
		options.radius > 30 || options.occlusion > 2 || !options.frames || options.frames > 100000 ||
		options.warmup > 10000 || options.captureFrames > 10000 || !std::isfinite(options.phase) || options.phase < 0.f || options.phase >= 1.f ||
		!std::isfinite(options.settings.blurAmount) || options.settings.blurAmount <= 0.f || options.settings.blurAmount > 100.f)
	{
		throw std::invalid_argument("Invalid dimensions, radius, occlusion, sample count, phase or blur");
	}
	return options;
}

double OpenGlass::RenderTest::Percentile(const std::vector<double>& values, double fraction)
{
	if (values.empty()) return 0.0;
	auto sorted = values;
	std::sort(sorted.begin(), sorted.end());
	return sorted[static_cast<size_t>((sorted.size() - 1) * fraction)];
}

void Benchmark::Initialize()
{
	winrt::com_ptr<IDXGIFactory1> dxgi;
	THROW_IF_FAILED(CreateDXGIFactory1(IID_PPV_ARGS(dxgi.put())));
	winrt::com_ptr<IDXGIAdapter1> adapter;
	THROW_IF_FAILED(dxgi->EnumAdapters1(m_options.adapter, adapter.put()));
	DXGI_ADAPTER_DESC1 desc;
	THROW_IF_FAILED(adapter->GetDesc1(&desc));
	m_adapterName = desc.Description;
	const D3D_FEATURE_LEVEL levels[]{ D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0 };
	THROW_IF_FAILED(D3D11CreateDevice(adapter.get(), D3D_DRIVER_TYPE_UNKNOWN, nullptr,
		D3D11_CREATE_DEVICE_BGRA_SUPPORT | D3D11_CREATE_DEVICE_SINGLETHREADED |
		(m_options.debug ? D3D11_CREATE_DEVICE_DEBUG : 0),
		levels, static_cast<UINT>(std::size(levels)), D3D11_SDK_VERSION, m_device.put(), nullptr, m_immediate.put()));
	m_annotation = m_immediate.as<ID3DUserDefinedAnnotation>();
	D2D1_FACTORY_OPTIONS factoryOptions{};
	if (m_options.debug) factoryOptions.debugLevel = D2D1_DEBUG_LEVEL_INFORMATION;
	THROW_IF_FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, __uuidof(ID2D1Factory1),
		&factoryOptions, reinterpret_cast<void**>(m_factory.put())));
	winrt::com_ptr<ID2D1Device> d2dDevice;
	THROW_IF_FAILED(m_factory->CreateDevice(m_device.as<IDXGIDevice>().get(), d2dDevice.put()));
	THROW_IF_FAILED(d2dDevice->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, m_context.put()));
	m_context->SetUnitMode(D2D1_UNIT_MODE_PIXELS);
	m_context->SetDpi(96.f, 96.f);
	m_context->SetPrimitiveBlend(D2D1_PRIMITIVE_BLEND_COPY);
	m_context->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);
	if (m_options.cpuOnly)
	{
		const D3D11_QUERY_DESC descriptor{ D3D11_QUERY_EVENT, 0 };
		THROW_IF_FAILED(m_device->CreateQuery(&descriptor, m_completion.put()));
	}
	else
	{
		for (auto& query : m_queries)
		{
			D3D11_QUERY_DESC descriptor{ D3D11_QUERY_TIMESTAMP_DISJOINT, 0 };
			THROW_IF_FAILED(m_device->CreateQuery(&descriptor, query.disjoint.put()));
			descriptor.Query = D3D11_QUERY_TIMESTAMP;
			THROW_IF_FAILED(m_device->CreateQuery(&descriptor, query.begin.put()));
			THROW_IF_FAILED(m_device->CreateQuery(&descriptor, query.end.put()));
		}
	}
	LoadBackground();
	CreateTarget();
	CreateGeometry();
	ResetMeasurements();
}

void Benchmark::LoadBackground()
{
	winrt::com_ptr<IWICImagingFactory> factory;
	THROW_IF_FAILED(CoCreateInstance(CLSID_WICImagingFactory2, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(factory.put())));
	winrt::com_ptr<IWICBitmapDecoder> decoder;
	THROW_IF_FAILED(factory->CreateDecoderFromFilename(L"OpenGlassRenderTest.png", nullptr, GENERIC_READ,
		WICDecodeMetadataCacheOnLoad, decoder.put()));
	winrt::com_ptr<IWICBitmapFrameDecode> frame;
	THROW_IF_FAILED(decoder->GetFrame(0, frame.put()));
	winrt::com_ptr<IWICFormatConverter> converter;
	THROW_IF_FAILED(factory->CreateFormatConverter(converter.put()));
	THROW_IF_FAILED(converter->Initialize(frame.get(), GUID_WICPixelFormat32bppPBGRA,
		WICBitmapDitherTypeNone, nullptr, 0.f, WICBitmapPaletteTypeCustom));
	const auto properties = D2D1::BitmapProperties1(D2D1_BITMAP_OPTIONS_NONE,
		D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED), 96.f, 96.f);
	THROW_IF_FAILED(m_context->CreateBitmapFromWicBitmap(converter.get(), properties, m_backgroundBitmap.put()));
}

void Benchmark::CreateTarget()
{
	m_context->SetTarget(nullptr);
	m_bitmap = nullptr;
	D3D11_TEXTURE2D_DESC desc{};
	desc.Width = m_options.width;
	desc.Height = m_options.height;
	desc.MipLevels = desc.ArraySize = desc.SampleDesc.Count = 1;
	desc.Format = m_options.fp16 ? DXGI_FORMAT_R16G16B16A16_FLOAT : DXGI_FORMAT_B8G8R8A8_UNORM;
	desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	THROW_IF_FAILED(m_device->CreateTexture2D(&desc, nullptr, m_target.put()));
	THROW_IF_FAILED(m_device->CreateRenderTargetView(m_target.get(), nullptr, m_targetView.put()));
	m_sourceView = nullptr;
	if (!m_options.settings.forceCapture)
	{
		THROW_IF_FAILED(m_device->CreateShaderResourceView(m_target.get(), nullptr, m_sourceView.put()));
	}
	const auto bitmapOptions = m_options.settings.forceCapture ?
		D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW : D2D1_BITMAP_OPTIONS_TARGET;
	const auto properties = D2D1::BitmapProperties1(bitmapOptions,
		D2D1::PixelFormat(desc.Format, D2D1_ALPHA_MODE_IGNORE), 96.f, 96.f);
	THROW_IF_FAILED(m_context->CreateBitmapFromDxgiSurface(m_target.as<IDXGISurface>().get(), &properties, m_bitmap.put()));
	if (m_options.settings.forceCapture)
	{
		// The realizer sees a non-sampleable target; the preview still needs an input bitmap.
		auto previewProperties = properties;
		previewProperties.bitmapOptions = D2D1_BITMAP_OPTIONS_NONE;
		THROW_IF_FAILED(m_context->CreateBitmapFromDxgiSurface(m_target.as<IDXGISurface>().get(), &previewProperties, m_previewBitmap.put()));
	}
	else
	{
		m_previewBitmap = m_bitmap;
	}
	THROW_IF_FAILED(m_device->CreateTexture2D(&desc, nullptr, m_background.put()));
	winrt::com_ptr<ID2D1Bitmap1> backgroundTarget;
	const auto backgroundProperties = D2D1::BitmapProperties1(
		D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW, properties.pixelFormat, 96.f, 96.f);
	THROW_IF_FAILED(m_context->CreateBitmapFromDxgiSurface(m_background.as<IDXGISurface>().get(),
		backgroundProperties, backgroundTarget.put()));
	m_context->SetTarget(backgroundTarget.get());
	m_context->SetTransform(D2D1::IdentityMatrix());
	m_context->SetPrimitiveBlend(D2D1_PRIMITIVE_BLEND_SOURCE_OVER);
	m_context->BeginDraw();
	m_context->Clear(D2D1::ColorF(D2D1::ColorF::Black));
	const auto imageSize = m_backgroundBitmap->GetSize();
	m_context->DrawBitmap(m_backgroundBitmap.get(), D2D1::RectF(0.f, 0.f, imageSize.width, imageSize.height),
		1.f, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);
	THROW_IF_FAILED(m_context->EndDraw());
	m_context->SetPrimitiveBlend(D2D1_PRIMITIVE_BLEND_COPY);
	m_context->SetTarget(m_bitmap.get());
}

void Benchmark::CreateGeometry()
{
	const int left = 24, top = 24, right = int(m_options.width) - 24, bottom = int(m_options.height) - 24;
	wil::unique_hrgn region{ m_options.radius ? CreateRoundRectRgn(left, top, right + 1, bottom + 1,
		int(m_options.radius) * 2, int(m_options.radius) * 2) : CreateRectRgn(left, top, right, bottom) };
	THROW_LAST_ERROR_IF_NULL(region);
	wil::unique_hrgn client{ CreateRectRgn(left + 8, top + 48, right - 8, bottom - 8) };
	THROW_HR_IF(E_FAIL, CombineRgn(region.get(), region.get(), client.get(), RGN_DIFF) == ERROR);
	if (m_options.occlusion)
	{
		wil::unique_hrgn cover{ CreateRectRgn(left + (right - left) / 3, top - 8, right + 40, top + (bottom - top) / 2) };
		THROW_HR_IF(E_FAIL, CombineRgn(region.get(), region.get(), cover.get(), RGN_DIFF) == ERROR);
		if (m_options.occlusion == 2)
		{
			wil::unique_hrgn second{ CreateRectRgn(left - 4, top + (bottom - top) / 2, left + 28, top + (bottom - top) * 2 / 3) };
			THROW_HR_IF(E_FAIL, CombineRgn(region.get(), region.get(), second.get(), RGN_DIFF) == ERROR);
		}
	}
	const DWORD bytes = GetRegionData(region.get(), 0, nullptr);
	THROW_LAST_ERROR_IF(!bytes);
	std::vector<BYTE> storage(bytes);
	const auto data = reinterpret_cast<RGNDATA*>(storage.data());
	THROW_LAST_ERROR_IF(!GetRegionData(region.get(), bytes, data));
	m_rectangles.clear();
	std::vector<winrt::com_ptr<ID2D1RectangleGeometry>> owned;
	std::vector<ID2D1Geometry*> parts;
	for (DWORD i = 0; i < data->rdh.nCount; ++i)
	{
		const auto rect = reinterpret_cast<RECT*>(data->Buffer)[i];
		const D2D1_RECT_F box{ rect.left + m_options.phase, rect.top + m_options.phase,
			rect.right + m_options.phase, rect.bottom + m_options.phase };
		m_rectangles.push_back(box);
		winrt::com_ptr<ID2D1RectangleGeometry> geometry;
		THROW_IF_FAILED(m_factory->CreateRectangleGeometry(box, geometry.put()));
		parts.push_back(geometry.get());
		owned.push_back(std::move(geometry));
	}
	winrt::com_ptr<ID2D1GeometryGroup> group;
	THROW_IF_FAILED(m_factory->CreateGeometryGroup(D2D1_FILL_MODE_WINDING, parts.data(), static_cast<UINT>(parts.size()), group.put()));
	m_geometry = group;
	THROW_IF_FAILED(m_geometry->GetBounds(nullptr, &m_bounds));
}

void Benchmark::Reconfigure(const Options& options)
{
	DrainQueries();
	const bool formatChanged = options.fp16 != m_options.fp16;
	const bool sizeChanged = options.width != m_options.width || options.height != m_options.height;
	const bool inputChanged = options.settings.forceCapture != m_options.settings.forceCapture;
	const bool geometryChanged = sizeChanged || options.radius != m_options.radius ||
		options.occlusion != m_options.occlusion || options.phase != m_options.phase;
	m_options = options;
	if (formatChanged)
	{
		m_d2d = {};
		m_rects = RectRenderer{};
		m_d3d = {};
		m_gaussian = RectRenderer{ winrt::make<GaussianEffect>() };
	}
	if (formatChanged || sizeChanged || inputChanged) CreateTarget();
	if (geometryChanged) CreateGeometry();
	ResetMeasurements();
}

void Benchmark::ResetMeasurements()
{
	m_stats = {};
	m_warmup.fill(m_options.warmup);
	for (auto& query : m_queries) query.measured = false;
}

void Benchmark::CollectQueries()
{
	for (auto& query : m_queries)
	{
		if (!query.active) continue;
		D3D11_QUERY_DATA_TIMESTAMP_DISJOINT clock;
		const auto hr = m_immediate->GetData(query.disjoint.get(), &clock, sizeof(clock), D3D11_ASYNC_GETDATA_DONOTFLUSH);
		if (hr == S_FALSE) continue;
		THROW_IF_FAILED(hr);
		UINT64 begin{}, end{};
		const auto startResult = m_immediate->GetData(query.begin.get(), &begin, sizeof(begin), D3D11_ASYNC_GETDATA_DONOTFLUSH);
		const auto endResult = m_immediate->GetData(query.end.get(), &end, sizeof(end), D3D11_ASYNC_GETDATA_DONOTFLUSH);
		THROW_IF_FAILED(startResult);
		THROW_IF_FAILED(endResult);
		if (startResult == S_FALSE || endResult == S_FALSE) continue;
		if (query.measured)
		{
			auto& stats = m_stats[static_cast<size_t>(query.method)];
			if (clock.Disjoint) ++stats.disjoint;
			else stats.gpu.push_back(double(end - begin) * 1.e6 / clock.Frequency);
		}
		query.active = false;
	}
}

void Benchmark::DrainQueries()
{
	// CPU-only runs also retire each round, outside the measured CPU interval.
	if (m_completion) m_immediate->End(m_completion.get());
	m_immediate->Flush();
	const auto deadline = Clock::now() + std::chrono::seconds(15);
	for (;;)
	{
		CollectQueries();
		bool pending;
		if (m_completion)
		{
			const auto hr = m_immediate->GetData(m_completion.get(), nullptr, 0, D3D11_ASYNC_GETDATA_DONOTFLUSH);
			THROW_IF_FAILED(hr);
			pending = hr == S_FALSE;
		}
		else pending = std::any_of(m_queries.begin(), m_queries.end(), [](const auto& q) { return q.active; });
		if (!pending) break;
		THROW_HR_IF(HRESULT_FROM_WIN32(ERROR_TIMEOUT), Clock::now() > deadline);
		Sleep(1);
	}
}

void Benchmark::Frame(Method method, bool measured)
{
	CollectQueries();
	const size_t index = static_cast<size_t>(method);
	if (m_warmup[index])
	{
		--m_warmup[index];
		measured = false;
	}
	// Reset before EVERY iteration: no mode samples a previous blur's output.
	m_immediate->CopyResource(m_target.get(), m_background.get());
	m_context->SetTarget(m_bitmap.get());
	m_context->SetTransform(D2D1::IdentityMatrix());
	m_context->BeginDraw();
	m_annotation->BeginEvent(MethodLabels[index]);
	Query* query{};
	if (!m_options.cpuOnly)
	{
		for (auto& candidate : m_queries)
		{
			if (!candidate.active)
			{
				query = &candidate;
				break;
			}
		}
		if (query)
		{
			query->method = method;
			query->active = true;
			query->measured = measured;
			m_immediate->Begin(query->disjoint.get());
			m_immediate->End(query->begin.get());
		}
		else if (measured) ++m_stats[index].missed;
	}
	const auto start = Clock::now();
	// Match the caller's background-materialization boundary.
	THROW_IF_FAILED(m_context->Flush());
	const auto params = m_options.settings.Parameters();
	HRESULT hr{};
	switch (method)
	{
	case Method::D2D:
		hr = m_d2d.Render(m_context.get(), m_geometry.get(), m_bounds, m_rectangles, params, OpenGlass::Shared::GlassType::Aero);
		break;
	case Method::Rects:
		hr = m_rects.Render(m_context.get(), m_geometry.get(), m_bounds, m_rectangles, params);
		break;
	case Method::Gaussian:
		hr = m_gaussian.Render(m_context.get(), m_geometry.get(), m_bounds, m_rectangles, params, 2.f);
		break;
	case Method::D3D:
		hr = m_d3d.Render(m_device.get(), m_immediate.get(), m_target.get(), m_targetView.get(), m_sourceView.get(),
			m_geometry.get(), m_bounds, m_rectangles, params, OpenGlass::Shared::GlassType::Aero);
		break;
	}
	const auto endDraw = m_context->EndDraw();
	const auto finish = Clock::now();
	if (query)
	{
		m_immediate->End(query->end.get());
		m_immediate->End(query->disjoint.get());
	}
	m_annotation->EndEvent();
	THROW_IF_FAILED(hr);
	THROW_IF_FAILED(endDraw);
	if (measured) m_stats[index].cpu.push_back(std::chrono::duration<double, std::micro>(finish - start).count());
}

void Benchmark::Run()
{
	std::array order{ Method::D2D, Method::Rects, Method::D3D, Method::Gaussian };
	std::mt19937 random{ 4226 };
	for (UINT frame = 0; frame < m_options.warmup + m_options.frames; ++frame)
	{
		std::shuffle(order.begin(), order.end(), random);
		for (const auto method : order) Frame(method);
		// Submit and collect outside the timed CPU range. This bounds queue depth
		// without mixing Present, readback or busy polling into the sample.
		DrainQueries();
	}
	const auto result = Report();
	std::ofstream output(m_options.output);
	if (!output) throw std::runtime_error("Cannot open benchmark output");
	output << result.dump(2) << '\n';
}

std::vector<float> Benchmark::ReadPixels()
{
	D3D11_TEXTURE2D_DESC desc;
	m_target->GetDesc(&desc);
	desc.BindFlags = 0;
	desc.Usage = D3D11_USAGE_STAGING;
	desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	winrt::com_ptr<ID3D11Texture2D> staging;
	THROW_IF_FAILED(m_device->CreateTexture2D(&desc, nullptr, staging.put()));
	m_immediate->CopyResource(staging.get(), m_target.get());
	D3D11_MAPPED_SUBRESOURCE map;
	THROW_IF_FAILED(m_immediate->Map(staging.get(), 0, D3D11_MAP_READ, 0, &map));
	const auto unmap = wil::scope_exit([&] { m_immediate->Unmap(staging.get(), 0); });
	std::vector<float> pixels(static_cast<size_t>(desc.Width) * desc.Height * 4);
	for (UINT y = 0; y < desc.Height; ++y)
	{
		const auto row = static_cast<const BYTE*>(map.pData) + static_cast<size_t>(y) * map.RowPitch;
		for (UINT x = 0; x < desc.Width * 4; ++x)
		{
			pixels[(static_cast<size_t>(y) * desc.Width * 4) + x] = m_options.fp16 ?
				DirectX::PackedVector::XMConvertHalfToFloat(reinterpret_cast<const USHORT*>(row)[x]) : row[x] / 255.f;
		}
	}
	return pixels;
}

Json Benchmark::Report()
{
	const auto params = m_options.settings.Parameters();
	constexpr const char* optimizations[]{ "speed", "balanced", "quality" };
	Json result{ { "adapter", winrt::to_string(m_adapterName) }, { "adapter_index", m_options.adapter },
		{ "background_image", "OpenGlassRenderTest.png" },
		{ "width", m_options.width }, { "height", m_options.height }, { "format", m_options.fp16 ? "fp16" : "bgra8" },
		{ "radius", m_options.radius }, { "occlusion", m_options.occlusion }, { "phase", m_options.phase },
		{ "blur_amount", m_options.settings.blurAmount }, { "optimization", optimizations[m_options.settings.optimization] },
		{ "frames_per_method", m_options.frames }, { "warmup_per_method", m_options.warmup }, { "order_seed", 4226 },
		{ "force_capture", m_options.settings.forceCapture }, { "glass_type", "Aero" },
		{ "colorization", { { "color", { params.color.r, params.color.g, params.color.b, params.color.a } },
			{ "afterglow", { params.afterglow.r, params.afterglow.g, params.afterglow.b, params.afterglow.a } },
			{ "blur_balance", params.blurBalance },
			{ "fallback", { params.fallback.r, params.fallback.g, params.fallback.b, params.fallback.a } } } },
		{ "cpu_protocol", "Caller D2D Flush, Render and EndDraw; excludes source reset, query calls, readback and Present" },
		{ "gpu_protocol", "D3D11 timestamp span covering rendering, submission and CPU-feed gaps; statistics use valid non-disjoint samples." },
		{ "cpu_only", m_options.cpuOnly }, { "results", Json::array() } };
	for (size_t i = 0; i < MethodCount; ++i)
	{
		const auto& stats = m_stats[i];
		result["results"].push_back({ { "method", MethodNames[i] }, { "cpu_us_median", Percentile(stats.cpu, .5) },
			{ "cpu_us_p95", Percentile(stats.cpu, .95) }, { "cpu_samples", stats.cpu.size() },
			{ "gpu_span_us_median", stats.gpu.empty() ? Json(nullptr) : Json(Percentile(stats.gpu, .5)) },
			{ "gpu_span_us_p95", stats.gpu.empty() ? Json(nullptr) : Json(Percentile(stats.gpu, .95)) },
			{ "gpu_samples", stats.gpu.size() }, { "gpu_disjoint", stats.disjoint }, { "gpu_missed", stats.missed },
			{ "blur_amount", i == static_cast<size_t>(Method::D3D) ? OpenGlass::BlurSettings::Direct3DStandardDeviation : m_options.settings.blurAmount },
			{ "optimization", i == static_cast<size_t>(Method::D3D) ? Json(nullptr) : Json(optimizations[m_options.settings.optimization]) },
			{ "rectangles", m_rectangles.size() } });
		if (i == static_cast<size_t>(Method::Gaussian))
		{
			auto& method = result["results"].back();
			method["input_scale"] = { .5f, .5f };
			method["output_scale"] = { 2.f, 2.f };
			method["full_resolution_blur_amount"] = m_options.settings.blurAmount * 2.f;
		}
	}
	if (m_options.validate)
	{
		Frame(Method::Rects, false);
		const auto reference = ReadPixels();
		result["pixels_vs_rects"] = Json::array();
		result["pixel_region"] = "full_frame";
		for (const auto method : { Method::D2D, Method::D3D, Method::Gaussian })
		{
			Frame(method, false);
			const auto pixels = ReadPixels();
			double maximum{}, squared{};
			size_t changed{};
			for (size_t i = 0; i < pixels.size(); ++i)
			{
				const double difference = std::abs(double(pixels[i]) - reference[i]);
				maximum = std::max(maximum, difference);
				squared += difference * difference;
				changed += difference != 0;
			}
			result["pixels_vs_rects"].push_back({ { "method", MethodNames[static_cast<size_t>(method)] },
				{ "max_abs_error", maximum }, { "rmse", std::sqrt(squared / pixels.size()) }, { "changed_channels", changed } });
		}
	}
	if (m_options.debug)
	{
		const auto queue = m_device.as<ID3D11InfoQueue>();
		UINT warnings{}, errors{};
		Json messages = Json::array();
		for (UINT64 i = 0; i < queue->GetNumStoredMessages(); ++i)
		{
			SIZE_T bytes{};
			THROW_IF_FAILED(queue->GetMessage(i, nullptr, &bytes));
			std::vector<BYTE> storage(bytes);
			const auto message = reinterpret_cast<D3D11_MESSAGE*>(storage.data());
			THROW_IF_FAILED(queue->GetMessage(i, message, &bytes));
			if (message->Severity <= D3D11_MESSAGE_SEVERITY_ERROR) ++errors;
			else if (message->Severity == D3D11_MESSAGE_SEVERITY_WARNING) ++warnings;
			if (message->Severity <= D3D11_MESSAGE_SEVERITY_WARNING) messages.push_back(message->pDescription);
		}
		result["d3d_debug"] = { { "errors", errors }, { "warnings", warnings }, { "messages", messages } };
	}
	return result;
}
