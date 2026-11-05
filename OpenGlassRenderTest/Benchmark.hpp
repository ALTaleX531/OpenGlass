#pragma once
#include "Renderer.hpp"
#include "GlassRealizer.hpp"
#include "D3DGlassRealizer.hpp"
#include "GlassRenderTarget.hpp"
#include <nlohmann/json.hpp>
#include <array>
#include <filesystem>

namespace OpenGlass::RenderTest
{
	struct Options
	{
		UINT width{ 1600 }, height{ 1000 }, adapter{}, radius{ 6 }, occlusion{}, frames{ 120 }, warmup{ 12 }, captureFrames{};
		float phase{};
		bool fp16{}, debug{}, cpuOnly{}, validate{};
		Method method{ Method::D2D };
		Settings settings;
		std::filesystem::path output;
	};
	Options ParseOptions(int argc, wchar_t** argv);

	struct Statistics
	{
		std::vector<double> cpu;
		std::vector<double> gpu;
		UINT disjoint{}, missed{};
	};
	double Percentile(const std::vector<double>& values, double fraction);

	class Benchmark
	{
		struct Query
		{
			winrt::com_ptr<ID3D11Query> disjoint, begin, end;
			Method method{};
			bool active{}, measured{};
		};
		Options m_options;
		winrt::com_ptr<ID3D11Device> m_device;
		winrt::com_ptr<ID3D11DeviceContext> m_immediate;
		winrt::com_ptr<ID3DUserDefinedAnnotation> m_annotation;
		winrt::com_ptr<ID3D11Query> m_completion;
		winrt::com_ptr<ID2D1Factory1> m_factory;
		winrt::com_ptr<ID2D1DeviceContext> m_context;
		winrt::com_ptr<ID3D11Texture2D> m_target, m_background;
		CGlassRenderTarget m_renderTarget;
		winrt::com_ptr<ID2D1Bitmap1> m_bitmap;
		winrt::com_ptr<ID2D1Bitmap1> m_previewBitmap;
		winrt::com_ptr<ID2D1Bitmap1> m_backgroundBitmap;
		winrt::com_ptr<ID2D1Geometry> m_geometry;
		std::vector<D2D1_RECT_F> m_rectangles;
		D2D1_RECT_F m_bounds{};
		CGlassRealizer m_d2d;
		RectRenderer m_rects;
		CD3DGlassRealizer m_d3d;
		RectRenderer m_gaussian{ winrt::make<GaussianEffect>() };
		std::array<Statistics, MethodCount> m_stats;
		std::array<Query, 12> m_queries;
		std::array<UINT, MethodCount> m_warmup{};
		std::wstring m_adapterName;
		void LoadBackground();
		void CreateTarget();
		void CreateGeometry();
		void CollectQueries();
		void DrainQueries();
		std::vector<float> ReadPixels();
		nlohmann::json Report();
	public:
		explicit Benchmark(Options options) : m_options(std::move(options)) {}
		void Initialize();
		void Reconfigure(const Options& options);
		void ResetMeasurements();
		void Frame(Method method, bool measured = true);
		void Run();
		const Options& Configuration() const { return m_options; }
		const std::wstring& AdapterName() const { return m_adapterName; }
		const Statistics& Stats(Method method) const { return m_stats[static_cast<size_t>(method)]; }
		ID3D11Device* Device() const { return m_device.get(); }
		ID2D1DeviceContext* Context() const { return m_context.get(); }
		ID2D1Bitmap1* PreparePreview();
		UINT RectangleCount() const { return static_cast<UINT>(m_rectangles.size()); }
	};
}
