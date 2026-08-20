#include "pch.h"
#include "CustomBlurEffect.hpp"
#include "D2DPrivates.hpp"

using namespace OpenGlass;

namespace
{
	constexpr UINT kDefaultWidth = 1600;
	constexpr UINT kDefaultHeight = 1200;
	constexpr float kInsetLeft = 22.f * 8.f;
	constexpr float kInsetTop = 50.f * 8.f;
	constexpr float kInsetRight = 22.f * 8.f;
	constexpr float kInsetBottom = 22.f * 8.f;
	constexpr float kBlurAmount = 9.f;
	constexpr wchar_t kImageFileName[] = L"OpenGlassRenderTest.png";

	float GetBlurExpansion(float blurAmount)
	{
		return blurAmount * 3.f + 0.5f;
	}

	std::wstring BuildImagePath()
	{
		const auto length = GetCurrentDirectoryW(0, nullptr);
		std::wstring workingDir(length + 1 + wcslen(kImageFileName) + 1, L'\0');
		GetCurrentDirectoryW(length + 1, workingDir.data());
		PathCchAppend(workingDir.data(), workingDir.size(), kImageFileName);
		return workingDir;
	}

	namespace RectF
	{
		D2D1_RECT_F TransformRect(const D2D1_RECT_F& rectangle, const D2D1_MATRIX_3X2_F& matrix)
		{
			const auto matrixHelper = D2D1::Matrix3x2F::ReinterpretBaseType(&matrix);
			const auto topLeft = matrixHelper->TransformPoint(D2D1::Point2F(rectangle.left, rectangle.top));
			const auto topRight = matrixHelper->TransformPoint(D2D1::Point2F(rectangle.right, rectangle.top));
			const auto bottomLeft = matrixHelper->TransformPoint(D2D1::Point2F(rectangle.left, rectangle.bottom));
			const auto bottomRight = matrixHelper->TransformPoint(D2D1::Point2F(rectangle.right, rectangle.bottom));

			D2D1_RECT_F transformedRect{};
			transformedRect.left = std::min({ topLeft.x, topRight.x, bottomLeft.x, bottomRight.x });
			transformedRect.top = std::min({ topLeft.y, topRight.y, bottomLeft.y, bottomRight.y });
			transformedRect.right = std::max({ topLeft.x, topRight.x, bottomLeft.x, bottomRight.x });
			transformedRect.bottom = std::max({ topLeft.y, topRight.y, bottomLeft.y, bottomRight.y });

			return transformedRect;
		}

		bool IntersectUnsafe(D2D1_RECT_F& dst, const D2D1_RECT_F& src)
		{
			dst.left = std::max(dst.left, src.left);
			dst.top = std::max(dst.top, src.top);
			dst.right = std::min(dst.right, src.right);
			dst.bottom = std::min(dst.bottom, src.bottom);

			if (!wil::rect_is_empty(dst))
			{
				return true;
			}

			dst.left = dst.top = dst.right = dst.bottom = 0.f;
			return false;
		}
	}
}

enum class ClipMode
{
	PushLayer,
	DrawRects
};

class COpenGlassRenderTestApp
{
public:
	explicit COpenGlassRenderTestApp(HWND hwnd)
		: m_hwnd(hwnd)
	{
	}

	void SetHwnd(HWND hwnd)
	{
		m_hwnd = hwnd;
	}

	HRESULT Initialize()
	{
		RETURN_IF_FAILED(CreateDeviceResources());
		RETURN_IF_FAILED(LoadBackgroundBitmap());

		RECT rc{};
		GetClientRect(m_hwnd, &rc);
		RETURN_IF_FAILED(CreateWindowSizeDependentResources(rc.right - rc.left, rc.bottom - rc.top));

		return S_OK;
	}

	void OnResize(UINT width, UINT height)
	{
		if (!m_d2dContext || width == 0 || height == 0)
		{
			return;
		}

		CreateWindowSizeDependentResources(width, height);
		InvalidateRect(m_hwnd, nullptr, FALSE);
	}

	void ToggleClipMode()
	{
		m_clipMode = (m_clipMode == ClipMode::PushLayer) ? ClipMode::DrawRects : ClipMode::PushLayer;
		UpdateWindowTitle();
		InvalidateRect(m_hwnd, nullptr, FALSE);
	}

	void ToggleAutoCompare()
	{
		m_autoCompare = !m_autoCompare;
		m_lastAutoSwitch = std::chrono::steady_clock::now();
		UpdateWindowTitle();
	}

	void AdjustWorkload(int delta)
	{
		m_workloadIterations = static_cast<UINT>(std::clamp<int>(static_cast<int>(m_workloadIterations) + delta, 1, 256));
		UpdateWindowTitle();
	}

	HRESULT Render()
	{
		if (!m_d2dContext || !m_targetBitmap)
		{
			return S_OK;
		}

		const auto frameStart = std::chrono::steady_clock::now();
		MaybeAutoSwitchClipMode(frameStart);

		m_d2dContext->BeginDraw();
		m_d2dContext->SetTransform(D2D1::IdentityMatrix());
		m_d2dContext->Clear(D2D1::ColorF(0.f, 0.f, 0.f, 1.f));

		if (m_backgroundBitmap)
		{
			const auto imageSize = m_backgroundBitmap->GetSize();
			const D2D1_RECT_F dst = D2D1::RectF(0.f, 0.f, imageSize.width, imageSize.height);
			m_d2dContext->DrawBitmap(m_backgroundBitmap.get(), dst, 1.f, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);
		}


		double blurMs = 0.0;
		for (UINT i = 0; i < m_workloadIterations; ++i)
		{
			const auto blurStart = std::chrono::steady_clock::now();
			RETURN_IF_FAILED(RenderBlur());
			const auto blurEnd = std::chrono::steady_clock::now();
			blurMs += std::chrono::duration<double, std::milli>(blurEnd - blurStart).count();
		}

		m_lastBlurMs = blurMs;
		AccumulateModeStats(blurMs);
		DrawPerfOverlay();

		const auto hr = m_d2dContext->EndDraw();
		if (hr == D2DERR_RECREATE_TARGET)
		{
			DiscardDeviceResources();
			RETURN_IF_FAILED(CreateDeviceResources());
			RECT rc{};
			GetClientRect(m_hwnd, &rc);
			RETURN_IF_FAILED(CreateWindowSizeDependentResources(rc.right - rc.left, rc.bottom - rc.top));
			return S_OK;
		}
		RETURN_IF_FAILED(hr);

		m_swapChain->Present(0, 0);

		const auto frameEnd = std::chrono::steady_clock::now();
		m_lastFrameMs = std::chrono::duration<double, std::milli>(frameEnd - frameStart).count();
		TickFrameStats(frameEnd);

		return S_OK;
	}

private:
	HWND m_hwnd{};
	ClipMode m_clipMode{ ClipMode::PushLayer };

	winrt::com_ptr<ID3D11Device> m_d3dDevice{};
	winrt::com_ptr<ID3D11DeviceContext> m_d3dContext{};
	winrt::com_ptr<IDXGISwapChain1> m_swapChain{};

	winrt::com_ptr<ID2D1Factory1> m_d2dFactory{};
	winrt::com_ptr<ID2D1Device> m_d2dDevice{};
	winrt::com_ptr<ID2D1DeviceContext> m_d2dContext{};
	winrt::com_ptr<ID2D1Bitmap1> m_targetBitmap{};
	winrt::com_ptr<ID2D1Layer> m_clipLayer{};
	winrt::com_ptr<ID2D1PathGeometry> m_ringGeometry{};
	winrt::com_ptr<ID2D1SolidColorBrush> m_overlayBrush{};

	winrt::com_ptr<IWICImagingFactory> m_wicFactory{};
	winrt::com_ptr<ID2D1Bitmap1> m_backgroundBitmap{};
	winrt::com_ptr<IDWriteFactory> m_dwriteFactory{};
	winrt::com_ptr<IDWriteTextFormat> m_overlayTextFormat{};

	winrt::com_ptr<CCustomBlurEffect> m_blurEffect{};

	D2D1_RECT_F m_outerRect{};
	D2D1_RECT_F m_innerRect{};
	std::array<D2D1_RECT_F, 4> m_ringRects{};

	struct ModePerfStats
	{
		double lastMs{ 0.0 };
		double avgMs{ 0.0 };
		double accMs{ 0.0 };
		UINT samples{ 0 };
	};

	std::chrono::steady_clock::time_point m_lastSummaryStamp{};
	std::chrono::steady_clock::time_point m_lastAutoSwitch{};
	double m_lastFrameMs{ 0.0 };
	double m_lastBlurMs{ 0.0 };
	double m_avgFrameMs{ 0.0 };
	double m_fps{ 0.0 };
	double m_accumulatedFrameMs{ 0.0 };
	UINT m_accumulatedFrames{ 0 };
	UINT m_workloadIterations{ 1 };
	bool m_autoCompare{ true };
	ModePerfStats m_pushLayerStats{};
	ModePerfStats m_drawRectsStats{};

	HRESULT CreateDeviceResources()
	{
		UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
		#ifdef _DEBUG
		flags |= D3D11_CREATE_DEVICE_DEBUG;
		#endif

		static const D3D_FEATURE_LEVEL kLevels[] =
		{
			D3D_FEATURE_LEVEL_11_1,
			D3D_FEATURE_LEVEL_11_0
		};

		RETURN_IF_FAILED(
			D3D11CreateDevice(
				nullptr,
				D3D_DRIVER_TYPE_HARDWARE,
				nullptr,
				flags,
				kLevels,
				static_cast<UINT>(std::size(kLevels)),
				D3D11_SDK_VERSION,
				m_d3dDevice.put(),
				nullptr,
				m_d3dContext.put()
			)
		);

		winrt::com_ptr<IDXGIDevice> dxgiDevice{};
		RETURN_IF_FAILED(m_d3dDevice->QueryInterface(dxgiDevice.put()));

		D2D1_FACTORY_OPTIONS options{};
		#ifdef _DEBUG
		options.debugLevel = D2D1_DEBUG_LEVEL_INFORMATION;
		#endif

		RETURN_IF_FAILED(
			D2D1CreateFactory(
				D2D1_FACTORY_TYPE_SINGLE_THREADED,
				__uuidof(ID2D1Factory1),
				&options,
				reinterpret_cast<void**>(m_d2dFactory.put())
			)
		);

		RETURN_IF_FAILED(m_d2dFactory->CreateDevice(dxgiDevice.get(), m_d2dDevice.put()));
		RETURN_IF_FAILED(m_d2dDevice->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, m_d2dContext.put()));
		RETURN_IF_FAILED(m_d2dContext->CreateLayer(m_clipLayer.put()));
		RETURN_IF_FAILED(m_d2dContext->CreateSolidColorBrush(D2D1::ColorF(1.f, 1.f, 1.f, 1.f), m_overlayBrush.put()));
		m_d2dContext->SetUnitMode(D2D1_UNIT_MODE_PIXELS);

		RETURN_IF_FAILED(
			DWriteCreateFactory(
				DWRITE_FACTORY_TYPE_SHARED,
				__uuidof(IDWriteFactory),
				reinterpret_cast<IUnknown**>(m_dwriteFactory.put())
			)
		);
		RETURN_IF_FAILED(
			m_dwriteFactory->CreateTextFormat(
				L"Consolas",
				nullptr,
				DWRITE_FONT_WEIGHT_REGULAR,
				DWRITE_FONT_STYLE_NORMAL,
				DWRITE_FONT_STRETCH_NORMAL,
				18.f,
				L"",
				m_overlayTextFormat.put()
			)
		);
		RETURN_IF_FAILED(m_overlayTextFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING));
		RETURN_IF_FAILED(m_overlayTextFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR));

		RETURN_IF_FAILED(CoCreateInstance(
			CLSID_WICImagingFactory2,
			nullptr,
			CLSCTX_INPROC_SERVER,
			IID_PPV_ARGS(m_wicFactory.put())
		));

		m_blurEffect = winrt::make_self<CCustomBlurEffect>();

		return S_OK;
	}

	void DiscardDeviceResources()
	{
		m_targetBitmap = nullptr;
		m_swapChain = nullptr;
	}

	HRESULT CreateWindowSizeDependentResources(UINT width, UINT height)
	{
		if (!m_swapChain)
		{
			winrt::com_ptr<IDXGIDevice> dxgiDevice{};
			RETURN_IF_FAILED(m_d3dDevice->QueryInterface(dxgiDevice.put()));
			winrt::com_ptr<IDXGIAdapter> adapter{};
			RETURN_IF_FAILED(dxgiDevice->GetAdapter(adapter.put()));
			winrt::com_ptr<IDXGIFactory2> factory{};
			RETURN_IF_FAILED(adapter->GetParent(IID_PPV_ARGS(factory.put())));

			DXGI_SWAP_CHAIN_DESC1 desc{};
			desc.Width = width;
			desc.Height = height;
			desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
			desc.SampleDesc.Count = 1;
			desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT | DXGI_USAGE_SHADER_INPUT;
			desc.BufferCount = 2;
			desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
			desc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;

			RETURN_IF_FAILED(factory->CreateSwapChainForHwnd(
				m_d3dDevice.get(),
				m_hwnd,
				&desc,
				nullptr,
				nullptr,
				m_swapChain.put()
			));
		}
		else
		{
			m_d2dContext->SetTarget(nullptr);
			m_targetBitmap = nullptr;
			RETURN_IF_FAILED(m_swapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0));
		}

		winrt::com_ptr<IDXGISurface> surface{};
		RETURN_IF_FAILED(m_swapChain->GetBuffer(0, IID_PPV_ARGS(surface.put())));

		const auto props = D2D1::BitmapProperties1(
			D2D1_BITMAP_OPTIONS_TARGET,
			D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE)
		);

		RETURN_IF_FAILED(m_d2dContext->CreateBitmapFromDxgiSurface(surface.get(), &props, m_targetBitmap.put()));
		m_d2dContext->SetTarget(m_targetBitmap.get());

		RETURN_IF_FAILED(UpdateRingGeometry(width, height));
		UpdateWindowTitle();

		return S_OK;
	}

	HRESULT LoadBackgroundBitmap()
	{
		if (!m_wicFactory)
		{
			return E_FAIL;
		}

		auto filePath = BuildImagePath();

		winrt::com_ptr<IWICBitmapDecoder> decoder{};
		const auto hr = m_wicFactory->CreateDecoderFromFilename(
			filePath.c_str(),
			nullptr,
			GENERIC_READ,
			WICDecodeMetadataCacheOnLoad,
			decoder.put()
		);
		if (FAILED(hr))
		{
			OutputDebugStringW(L"[OpenGlassRenderTest] Image not found, drawing background only.\n");
			return S_OK;
		}

		winrt::com_ptr<IWICBitmapFrameDecode> frame{};
		RETURN_IF_FAILED(decoder->GetFrame(0, frame.put()));

		winrt::com_ptr<IWICFormatConverter> converter{};
		RETURN_IF_FAILED(m_wicFactory->CreateFormatConverter(converter.put()));
		RETURN_IF_FAILED(converter->Initialize(
			frame.get(),
			GUID_WICPixelFormat32bppPBGRA,
			WICBitmapDitherTypeNone,
			nullptr,
			0.f,
			WICBitmapPaletteTypeCustom
		));

		RETURN_IF_FAILED(m_d2dContext->CreateBitmapFromWicBitmap(converter.get(), nullptr, m_backgroundBitmap.put()));

		return S_OK;
	}

	HRESULT UpdateRingGeometry(UINT width, UINT height)
	{
		m_outerRect = D2D1::RectF(30.f, 30.f, static_cast<float>(width - 30.f), static_cast<float>(height - 30.f));

		m_innerRect = D2D1::RectF(
			std::min(m_outerRect.right, m_outerRect.left + kInsetLeft),
			std::min(m_outerRect.bottom, m_outerRect.top + kInsetTop),
			std::max(m_outerRect.left, m_outerRect.right - kInsetRight),
			std::max(m_outerRect.top, m_outerRect.bottom - kInsetBottom)
		);

		m_ringRects =
		{
			D2D1::RectF(m_outerRect.left, m_outerRect.top, m_outerRect.right, m_innerRect.top),
			D2D1::RectF(m_outerRect.left, m_innerRect.top, m_innerRect.left, m_innerRect.bottom),
			D2D1::RectF(m_innerRect.right, m_innerRect.top, m_outerRect.right, m_innerRect.bottom),
			D2D1::RectF(m_outerRect.left, m_innerRect.bottom, m_outerRect.right, m_outerRect.bottom)
		};

		m_ringGeometry = nullptr;

		if (!m_d2dFactory)
		{
			return S_OK;
		}

		RETURN_IF_FAILED(m_d2dFactory->CreatePathGeometry(m_ringGeometry.put()));
		winrt::com_ptr<ID2D1GeometrySink> sink{};
		RETURN_IF_FAILED(m_ringGeometry->Open(sink.put()));

		sink->SetFillMode(D2D1_FILL_MODE_ALTERNATE);

		const D2D1_POINT_2F outerPoints[] =
		{
			{ m_outerRect.left, m_outerRect.top },
			{ m_outerRect.right, m_outerRect.top },
			{ m_outerRect.right, m_outerRect.bottom },
			{ m_outerRect.left, m_outerRect.bottom }
		};
		const D2D1_POINT_2F innerPoints[] =
		{
			{ m_innerRect.left, m_innerRect.top },
			{ m_innerRect.right, m_innerRect.top },
			{ m_innerRect.right, m_innerRect.bottom },
			{ m_innerRect.left, m_innerRect.bottom }
		};

		sink->BeginFigure(outerPoints[0], D2D1_FIGURE_BEGIN_FILLED);
		sink->AddLines(outerPoints + 1, static_cast<UINT>(std::size(outerPoints) - 1));
		sink->EndFigure(D2D1_FIGURE_END_CLOSED);

		sink->BeginFigure(innerPoints[0], D2D1_FIGURE_BEGIN_FILLED);
		sink->AddLines(innerPoints + 1, static_cast<UINT>(std::size(innerPoints) - 1));
		sink->EndFigure(D2D1_FIGURE_END_CLOSED);

		RETURN_IF_FAILED(sink->Close());

		return S_OK;
	}

	HRESULT RenderBlur()
	{
		if (!m_blurEffect || !m_targetBitmap)
		{
			return S_OK;
		}

		winrt::com_ptr<ID2D1Image> targetImage{};
		m_d2dContext->GetTarget(targetImage.put());
		if (!targetImage)
		{
			return S_OK;
		}

		RETURN_IF_FAILED(m_d2dContext->Flush());
		m_d2dContext->SetPrimitiveBlend(D2D1_PRIMITIVE_BLEND_COPY);

		winrt::com_ptr<ID2D1Bitmap1> renderTargetBitmap{};
		RETURN_IF_FAILED(targetImage->QueryInterface(renderTargetBitmap.put()));

		winrt::com_ptr<ID2D1PrivateCompositorDeviceContext> compositorDeviceContext{};
		RETURN_IF_FAILED(m_d2dContext->QueryInterface(compositorDeviceContext.put()));

		winrt::com_ptr<ID2D1Bitmap1> sharedAtlasBitmap{};
		const auto bitmapProperties = D2D1::BitmapProperties1(
			renderTargetBitmap->GetOptions(),
			renderTargetBitmap->GetPixelFormat()
		);
		RETURN_IF_FAILED(
			compositorDeviceContext->CreateSharedAtlasBitmap(
				renderTargetBitmap.get(),
				&bitmapProperties,
				sharedAtlasBitmap.put()
			)
		);

		const auto targetSize = renderTargetBitmap->GetSize();
		const auto expansion = GetBlurExpansion(kBlurAmount);

		D2D1_RECT_F alignedSamplingWorldBounds
		{
			std::max(std::floor(m_outerRect.left - expansion), 0.f),
			std::max(std::floor(m_outerRect.top - expansion), 0.f),
			std::min(std::ceil(m_outerRect.right + expansion), targetSize.width),
			std::min(std::ceil(m_outerRect.bottom + expansion), targetSize.height)
		};

		const D2D1_RECT_F alignedDrawingWorldBounds
		{
			std::floor(m_outerRect.left),
			std::floor(m_outerRect.top),
			std::ceil(m_outerRect.right),
			std::ceil(m_outerRect.bottom)
		};

		if (m_clipMode == ClipMode::PushLayer && m_ringGeometry)
		{
			const auto layerParams = D2D1::LayerParameters1(
				alignedDrawingWorldBounds,
				m_ringGeometry.get(),
				D2D1_ANTIALIAS_MODE_ALIASED,
				D2D1::IdentityMatrix(),
				1.0f,
				nullptr,
				D2D1_LAYER_OPTIONS1_INITIALIZE_FROM_BACKGROUND | D2D1_LAYER_OPTIONS1_IGNORE_ALPHA
			);
			m_d2dContext->PushLayer(layerParams, nullptr);
		}

		CCustomBlurParams params{};
		params.blurAmount = kBlurAmount;
		params.optimization = D2D1_GAUSSIANBLUR_OPTIMIZATION_BALANCED;

		RETURN_IF_FAILED(
			m_blurEffect->Build(
				m_d2dContext.get(),
				sharedAtlasBitmap.get(),
				alignedSamplingWorldBounds,
				&params
			)
		);

		winrt::com_ptr<ID2D1Image> outputImage{};
		m_blurEffect->GetOutput(outputImage.put());
		RETURN_HR_IF_NULL(D2DERR_UNSUPPORTED_PIXEL_FORMAT, outputImage);

		D2D1_MATRIX_3X2_F originalMatrix{};
		m_d2dContext->GetTransform(&originalMatrix);

		D2D1_MATRIX_3X2_F outputMatrix = m_blurEffect->GetOutputMatrix();
		m_d2dContext->SetTransform(outputMatrix);

		D2D1_RENDERING_CONTROLS renderingControls{}, originalControls{};
		m_d2dContext->GetRenderingControls(&renderingControls);
		originalControls = renderingControls;
		const auto targetPixelSize = renderTargetBitmap->GetPixelSize();
		if (renderingControls.tileSize.width < targetPixelSize.width || renderingControls.tileSize.height < targetPixelSize.height)
		{
			renderingControls.tileSize = D2D1::SizeU(4096, 4096);
			m_d2dContext->SetRenderingControls(renderingControls);
		}

		const auto restoreState = wil::scope_exit([&]
		{
			m_d2dContext->SetTransform(originalMatrix);
			m_d2dContext->SetRenderingControls(originalControls);
			if (m_clipMode == ClipMode::PushLayer && m_ringGeometry)
			{
				m_d2dContext->PopLayer();
			}
		});

		D2D1_MATRIX_3X2_F inverseOutputMatrix = outputMatrix;
		D2D1InvertMatrix(&inverseOutputMatrix);

		const auto offset = m_blurEffect->GetOutputOffset();

		auto drawBlurRect = [&](const D2D1_RECT_F& rect)
		{
			auto transformedRect = RectF::TransformRect(rect, inverseOutputMatrix);
			m_d2dContext->PushAxisAlignedClip(transformedRect, D2D1_ANTIALIAS_MODE_ALIASED);
			m_d2dContext->DrawImage(
				outputImage.get(),
				D2D1::Point2F(transformedRect.left, transformedRect.top),
				D2D1::RectF(
					transformedRect.left + offset.x,
					transformedRect.top + offset.y,
					transformedRect.right + offset.x,
					transformedRect.bottom + offset.y
				),
				D2D1_INTERPOLATION_MODE_LINEAR,
				D2D1_COMPOSITE_MODE_SOURCE_OVER
			);
			m_d2dContext->PopAxisAlignedClip();
		};

		if (m_clipMode == ClipMode::PushLayer && m_ringGeometry)
		{
			drawBlurRect(alignedDrawingWorldBounds);
		}
		else
		{
			for (const auto& rect : m_ringRects)
			{
				auto clipped = rect;
				if (RectF::IntersectUnsafe(clipped, alignedDrawingWorldBounds))
				{
					drawBlurRect(clipped);
				}
			}
		}

		m_blurEffect->Reset();

		return S_OK;
	}

	void UpdateWindowTitle()
	{
		const wchar_t* modeText = (m_clipMode == ClipMode::PushLayer) ? L"PushLayer" : L"DrawImage";
		const wchar_t* autoText = m_autoCompare ? L"Auto" : L"Manual";
		wchar_t title[256]{};
		swprintf_s(
			title,
			L"OpenGlassRenderTest - %s | Frame %.2f ms | FPS %.1f | WL x%u | %s",
			modeText,
			m_avgFrameMs,
			m_fps,
			m_workloadIterations,
			autoText
		);
		SetWindowTextW(m_hwnd, title);
	}

	void AccumulateModeStats(double blurMs)
	{
		auto& stats = (m_clipMode == ClipMode::PushLayer) ? m_pushLayerStats : m_drawRectsStats;
		stats.lastMs = blurMs;
		stats.accMs += blurMs;
		++stats.samples;
	}

	void MaybeAutoSwitchClipMode(const std::chrono::steady_clock::time_point& now)
	{
		if (!m_autoCompare)
		{
			return;
		}

		if (!m_lastAutoSwitch.time_since_epoch().count())
		{
			m_lastAutoSwitch = now;
			return;
		}

		if (now - m_lastAutoSwitch >= std::chrono::seconds(2))
		{
			m_clipMode = (m_clipMode == ClipMode::PushLayer) ? ClipMode::DrawRects : ClipMode::PushLayer;
			m_lastAutoSwitch = now;
		}
	}

	void TickFrameStats(const std::chrono::steady_clock::time_point& now)
	{
		m_accumulatedFrameMs += m_lastFrameMs;
		++m_accumulatedFrames;

		if (!m_lastSummaryStamp.time_since_epoch().count())
		{
			m_lastSummaryStamp = now;
			return;
		}

		if (now - m_lastSummaryStamp >= std::chrono::seconds(1) && m_accumulatedFrames > 0)
		{
			m_avgFrameMs = m_accumulatedFrameMs / static_cast<double>(m_accumulatedFrames);
			m_fps = (m_accumulatedFrameMs > 0.0) ? (1000.0 * static_cast<double>(m_accumulatedFrames) / m_accumulatedFrameMs) : 0.0;

			if (m_pushLayerStats.samples > 0)
			{
				m_pushLayerStats.avgMs = m_pushLayerStats.accMs / static_cast<double>(m_pushLayerStats.samples);
				m_pushLayerStats.accMs = 0.0;
				m_pushLayerStats.samples = 0;
			}
			if (m_drawRectsStats.samples > 0)
			{
				m_drawRectsStats.avgMs = m_drawRectsStats.accMs / static_cast<double>(m_drawRectsStats.samples);
				m_drawRectsStats.accMs = 0.0;
				m_drawRectsStats.samples = 0;
			}

			m_accumulatedFrameMs = 0.0;
			m_accumulatedFrames = 0;
			m_lastSummaryStamp = now;
			UpdateWindowTitle();
		}
	}

	void DrawPerfOverlay()
	{
		if (!m_overlayTextFormat || !m_overlayBrush)
		{
			return;
		}

		const wchar_t* modeText = (m_clipMode == ClipMode::PushLayer) ? L"PushLayer" : L"DrawImage";
		const wchar_t* autoText = m_autoCompare ? L"On" : L"Off";
		const auto deltaMs = m_drawRectsStats.avgMs - m_pushLayerStats.avgMs;
		wchar_t text[256]{};
		swprintf_s(
			text,
			L"Mode: %s  Auto:%s\nFrame: %.3f ms  FPS: %.1f\nBlur(last): %.3f ms  WL:x%u\nPushLayer(avg): %.3f ms\nDrawImage(avg): %.3f ms\nDelta(Draw-Push): %.3f ms",
			modeText,
			autoText,
			m_lastFrameMs,
			m_fps,
			m_lastBlurMs,
			m_workloadIterations,
			m_pushLayerStats.avgMs,
			m_drawRectsStats.avgMs,
			deltaMs
		);

		const D2D1_RECT_F panelRect = D2D1::RectF(12.f, 12.f, 460.f, 210.f);
		m_overlayBrush->SetColor(D2D1::ColorF(0.f, 0.f, 0.f, 0.55f));
		m_d2dContext->FillRectangle(panelRect, m_overlayBrush.get());

		m_overlayBrush->SetColor(D2D1::ColorF(1.f, 1.f, 1.f, 1.f));
		m_d2dContext->DrawTextW(
			text,
			static_cast<UINT32>(wcslen(text)),
			m_overlayTextFormat.get(),
			D2D1::RectF(panelRect.left + 8.f, panelRect.top + 8.f, panelRect.right - 8.f, panelRect.bottom - 8.f),
			m_overlayBrush.get(),
			D2D1_DRAW_TEXT_OPTIONS_NO_SNAP,
			DWRITE_MEASURING_MODE_NATURAL
		);
	}
};

LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	if (message == WM_NCCREATE)
	{
		auto createStruct = reinterpret_cast<CREATESTRUCTW*>(lParam);
		auto app = reinterpret_cast<COpenGlassRenderTestApp*>(createStruct->lpCreateParams);
		if (app)
		{
			app->SetHwnd(hwnd);
			SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
		}
		return TRUE;
	}

	auto app = reinterpret_cast<COpenGlassRenderTestApp*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

	switch (message)
	{
	case WM_SIZE:
		if (app)
		{
			app->OnResize(LOWORD(lParam), HIWORD(lParam));
		}
		return 0;
	case WM_KEYDOWN:
		if (app && (wParam == VK_SPACE || wParam == 'C'))
		{
			app->ToggleClipMode();
			return 0;
		}
		if (app && wParam == 'A')
		{
			app->ToggleAutoCompare();
			return 0;
		}
		if (app && (wParam == VK_OEM_PLUS || wParam == VK_ADD))
		{
			app->AdjustWorkload(4);
			return 0;
		}
		if (app && (wParam == VK_OEM_MINUS || wParam == VK_SUBTRACT))
		{
			app->AdjustWorkload(-4);
			return 0;
		}
		break;
	case WM_PAINT:
		{
			PAINTSTRUCT ps{};
			BeginPaint(hwnd, &ps);
			EndPaint(hwnd, &ps);
			return 0;
		}
		break;
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	}

	return DefWindowProcW(hwnd, message, wParam, lParam);
}

int APIENTRY wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE, _In_ LPWSTR, _In_ int nCmdShow)
{
	SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
	CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
	const auto coUninit = wil::scope_exit([] { CoUninitialize(); });

	const wchar_t kClassName[] = L"OpenGlassRenderTestWindow";

	WNDCLASSEXW wc{};
	wc.cbSize = sizeof(wc);
	wc.lpfnWndProc = WndProc;
	wc.hInstance = hInstance;
	wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
	wc.lpszClassName = kClassName;
	wc.hbrBackground = reinterpret_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
	RegisterClassExW(&wc);

	RECT rc{ 0, 0, static_cast<LONG>(kDefaultWidth), static_cast<LONG>(kDefaultHeight) };
	AdjustWindowRectEx(&rc, WS_OVERLAPPEDWINDOW, FALSE, 0);

	COpenGlassRenderTestApp app(nullptr);

	HWND hwnd = CreateWindowExW(
		0,
		kClassName,
		L"OpenGlassRenderTest",
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		rc.right - rc.left,
		rc.bottom - rc.top,
		nullptr,
		nullptr,
		hInstance,
		&app
	);

	if (!hwnd)
	{
		return 0;
	}

	app.Initialize();

	ShowWindow(hwnd, nCmdShow);
	UpdateWindow(hwnd);

	MSG msg{};
	bool running = true;
	while (running)
	{
		while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			if (msg.message == WM_QUIT)
			{
				running = false;
				break;
			}
			TranslateMessage(&msg);
			DispatchMessageW(&msg);
		}

		if (!running)
		{
			break;
		}

		app.Render();
	}

	return static_cast<int>(msg.wParam);
}
