#include "pch.h"
#include "Utils.hpp"
#include "uDwmProjection.hpp"
#include "GlassFramework.hpp"
#include "GlassEffectManager.hpp"
#include "ReflectionRenderer.hpp"
#include "CustomBlurEffect.hpp"

using namespace OpenGlass;
namespace OpenGlass::GlassEffectManager
{
	std::unordered_map<dwmcore::CGeometry*, winrt::com_ptr<IGlassEffect>> g_glassEffectMap{};

	class CGlassEffect : public winrt::implements<CGlassEffect, IGlassEffect>
	{
		bool m_initialized{ false };
		Type m_type{ Type::Blur };
		float m_glassOpacity{ 0.63f };
		float m_blurAmount{ 9.f };
		D2D1_COLOR_F m_color{};

		D2D1_SIZE_F m_glassSize{};
		D2D1_SIZE_F m_desktopSize{};
		D2D1_POINT_2F m_glassDesktopPosition{};
		D2D1_PIXEL_FORMAT m_backdropPixelFormat{};

		winrt::com_ptr<ID2D1DeviceContext> m_deviceContext{ nullptr };
		winrt::com_ptr<ID2D1Bitmap1> m_blurBuffer{ nullptr };
		winrt::com_ptr<ICustomBlurEffect> m_customBlurEffect{ nullptr };

		HRESULT Initialize();
	public:
		CGlassEffect(ID2D1DeviceContext* deviceContext) { winrt::copy_from_abi(m_deviceContext, deviceContext); }
		
		void STDMETHODCALLTYPE SetGlassRenderingParameters(
			const D2D1_COLOR_F& color,
			float glassOpacity,
			float blurAmount
		) override;
		void STDMETHODCALLTYPE SetSize(const D2D1_SIZE_F& size) override;
		HRESULT STDMETHODCALLTYPE Invalidate(
			ID2D1Bitmap1* backdropBitmap,
			const D2D1_POINT_2F& glassDesktopPosition,
			const D2D1_RECT_F& desktopRedrawRect,
			bool normalDesktopRender
		) override;
		HRESULT STDMETHODCALLTYPE Render(ID2D1Geometry* geometry, ID2D1SolidColorBrush* brush) override;
	};
}

HRESULT GlassEffectManager::CGlassEffect::Initialize()
{
	m_customBlurEffect = winrt::make<CCustomBlurEffect>(m_deviceContext.get());
	m_initialized = true;
	return S_OK;
}
void STDMETHODCALLTYPE GlassEffectManager::CGlassEffect::SetGlassRenderingParameters(
	const D2D1_COLOR_F& color,
	float glassOpacity,
	float blurAmount
)
{
	m_color = color;
	m_glassOpacity = glassOpacity;
	m_blurAmount = blurAmount;
}
void STDMETHODCALLTYPE GlassEffectManager::CGlassEffect::SetSize(const D2D1_SIZE_F& size)
{
	if (memcmp(&m_glassSize, &size, sizeof(D2D1_SIZE_F)) != 0)
	{
		if (
			auto actualGlassSize{ m_blurBuffer ? m_blurBuffer->GetSize() : m_glassSize };
			!(
				m_blurBuffer &&
				actualGlassSize.width >= size.width &&
				actualGlassSize.height >= size.height &&
				(
					actualGlassSize.width / 4.f * 3.f < size.width &&
					actualGlassSize.height / 4.f * 3.f < size.height
				)
			)
		)
		{
			m_blurBuffer = nullptr;
		}
		m_glassSize = size;
	}
}
HRESULT STDMETHODCALLTYPE GlassEffectManager::CGlassEffect::Invalidate(
	ID2D1Bitmap1* backdropBitmap,
	const D2D1_POINT_2F& glassDesktopPosition,
	const D2D1_RECT_F& desktopRedrawRect,
	bool normalDesktopRender
)
{
	m_backdropPixelFormat = backdropBitmap->GetPixelFormat();
	if (!m_initialized)
	{
		RETURN_IF_FAILED(Initialize());
	}

	if (normalDesktopRender)
	{
		if (m_blurBuffer)
		{
			auto blurBufferPixelFormat{ m_blurBuffer->GetPixelFormat() };
			m_blurBuffer = memcmp(&m_backdropPixelFormat, &blurBufferPixelFormat, sizeof(D2D1_PIXEL_FORMAT)) != 0 ? nullptr : m_blurBuffer;
		}

		bool bufferRecreated{ false };
		if (!m_blurBuffer)
		{
			RETURN_IF_FAILED(
				m_deviceContext->CreateBitmap(
					D2D1::SizeU(
						dwmcore::PixelAlign(m_glassSize.width),
						dwmcore::PixelAlign(m_glassSize.height)
					),
					nullptr,
					0,
					D2D1::BitmapProperties1(
						D2D1_BITMAP_OPTIONS_NONE,
						m_backdropPixelFormat
					),
					m_blurBuffer.put()
				)
			);
			m_customBlurEffect->Reset();
			bufferRecreated = true;

			/*OutputDebugStringW(
				std::format(
					L"blur buffer recreated: [{} x {}]\n",
					m_glassSize.width,
					m_glassSize.height
				).c_str()
			);*/
		}

		D2D1_POINT_2U dstPoint
		{
			dwmcore::PixelAlign(!bufferRecreated ? desktopRedrawRect.left - glassDesktopPosition.x : glassDesktopPosition.x > 0.f ? 0.f : -glassDesktopPosition.x),
			dwmcore::PixelAlign(!bufferRecreated ? desktopRedrawRect.top - glassDesktopPosition.y : glassDesktopPosition.y > 0.f ? 0.f : -glassDesktopPosition.y)
		};
		D2D1_RECT_U copyRect
		{
			dwmcore::PixelAlign(!bufferRecreated ? desktopRedrawRect.left : max(glassDesktopPosition.x, 0.f)),
			dwmcore::PixelAlign(!bufferRecreated ? desktopRedrawRect.top : max(glassDesktopPosition.y, 0.f)),
			dwmcore::PixelAlign(!bufferRecreated ? desktopRedrawRect.right : (glassDesktopPosition.x + m_glassSize.width)),
			dwmcore::PixelAlign(!bufferRecreated ? desktopRedrawRect.bottom : (glassDesktopPosition.y + m_glassSize.height))
		};
		/*OutputDebugStringW(
			std::format(
				L"backdrop copied: dst:[{} x {}], copyRect:[{},{},{},{}]\n",
				dstPoint.x,
				dstPoint.y,
				copyRect.left,
				copyRect.top,
				copyRect.right,
				copyRect.bottom
			).c_str()
		);*/
		RETURN_IF_FAILED(
			m_blurBuffer->CopyFromBitmap(
				&dstPoint,
				backdropBitmap,
				&copyRect
			)
		);
		m_desktopSize = backdropBitmap->GetSize();
		m_glassDesktopPosition = glassDesktopPosition;
	}

	// prepare drawing parameters
	if (m_blurBuffer && m_blurAmount)
	{
		auto backdropSize{ backdropBitmap->GetSize() };
		auto truncatedWidth{ glassDesktopPosition.x + m_glassSize.width - backdropSize.width };
		auto truncatedHeight{ glassDesktopPosition.y + m_glassSize.height - backdropSize.height };
		D2D1_RECT_F imageBounds
		{
			normalDesktopRender ? max(0.f - glassDesktopPosition.x, 0.f) : 0.f,
			normalDesktopRender ? max(0.f - glassDesktopPosition.y, 0.f) : 0.f,
			(truncatedWidth > 0.f && normalDesktopRender) ? max(m_glassSize.width - truncatedWidth, 0.f) : m_glassSize.width,
			(truncatedHeight > 0.f && normalDesktopRender) ? max(m_glassSize.height - truncatedHeight, 0.f) : m_glassSize.height
		};
		D2D1_RECT_F invalidInputRect
		{
			normalDesktopRender ? (desktopRedrawRect.left - glassDesktopPosition.x) : 0.f,
			normalDesktopRender ? (desktopRedrawRect.top - glassDesktopPosition.y) : 0.f,
			normalDesktopRender ? (desktopRedrawRect.left - glassDesktopPosition.x + (desktopRedrawRect.right - desktopRedrawRect.left)) : m_glassSize.width,
			normalDesktopRender ? (desktopRedrawRect.top - glassDesktopPosition.y + (desktopRedrawRect.bottom - desktopRedrawRect.top)) : m_glassSize.height
		};
		RETURN_IF_FAILED(
			m_customBlurEffect->Invalidate(
				m_blurBuffer.get(),
				invalidInputRect,
				imageBounds,
				m_blurAmount
			)
		);
	}

	return S_OK;
}

HRESULT STDMETHODCALLTYPE GlassEffectManager::CGlassEffect::Render(ID2D1Geometry* geometry, ID2D1SolidColorBrush* brush)
{
	D2D1_RECT_F bounds{};
	RETURN_IF_FAILED(geometry->GetBounds(nullptr, &bounds));

	m_deviceContext->PushLayer(
		D2D1::LayerParameters1(
			bounds,
			geometry,
			D2D1_ANTIALIAS_MODE_ALIASED,
			D2D1::IdentityMatrix(),
			1.f,
			nullptr,
			(m_backdropPixelFormat.alphaMode == D2D1_ALPHA_MODE_IGNORE && m_blurBuffer && m_blurAmount > 0.f ? D2D1_LAYER_OPTIONS1_IGNORE_ALPHA : D2D1_LAYER_OPTIONS1_NONE) |
			(m_blurBuffer && m_blurAmount > 0.f ? D2D1_LAYER_OPTIONS1_INITIALIZE_FROM_BACKGROUND : D2D1_LAYER_OPTIONS1_NONE)
		),
		nullptr
	);
	if (m_blurBuffer && m_blurAmount > 0.f)
	{
		m_customBlurEffect->Draw(bounds);
	}
	if (m_glassOpacity)
	{
		auto opacity{ brush->GetOpacity() };
		brush->SetOpacity(m_glassOpacity);
		m_deviceContext->FillGeometry(
			geometry,
			brush
		);
		brush->SetOpacity(opacity);
	}
	RETURN_IF_FAILED(
		ReflectionRenderer::Draw(
			m_deviceContext.get(),
			m_blurBuffer ? m_glassDesktopPosition : D2D1::Point2F(),
			m_desktopSize,
			bounds
		)
	);
	m_deviceContext->PopLayer();

	return S_OK;
}

winrt::com_ptr<GlassEffectManager::IGlassEffect> GlassEffectManager::GetOrCreate(dwmcore::CGeometry* geometry, ID2D1DeviceContext* deviceContext, bool createIfNecessary)
{
	auto it{ g_glassEffectMap.find(geometry) };

	if (createIfNecessary)
	{
		if (it == g_glassEffectMap.end())
		{
			auto result{ g_glassEffectMap.emplace(geometry, winrt::make<CGlassEffect>(deviceContext)) };
			if (result.second == true)
			{
				it = result.first;
			}
		}
	}

	return it == g_glassEffectMap.end() ? nullptr : it->second;
}
void GlassEffectManager::Remove(dwmcore::CGeometry* geometry)
{
	auto it{ g_glassEffectMap.find(geometry) };

	if (it != g_glassEffectMap.end())
	{
		g_glassEffectMap.erase(it);
	}
}
void GlassEffectManager::Shutdown()
{
	g_glassEffectMap.clear();
}