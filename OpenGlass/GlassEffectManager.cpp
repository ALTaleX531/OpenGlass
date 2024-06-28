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
		bool m_recreateBlurBuffer{ true };
		D2D1_PIXEL_FORMAT m_backdropPixelFormat{};
		D2D1_SIZE_F m_desktopSize{};
		D2D1_RECT_F m_sourceRect{};
		D2D1_RECT_F m_sourceRectBackup{};
		D2D1_COLOR_F m_color{};
		float m_glassOpacity{};
		winrt::com_ptr<ID2D1DeviceContext> m_deviceContext{ nullptr };
		winrt::com_ptr<ID2D1Bitmap1> m_blurBuffer{ nullptr };
		winrt::com_ptr<ID2D1Effect> m_fallbackBlurBuffer{ nullptr };
		winrt::com_ptr<ID2D1Effect> m_colorEffect{ nullptr };
		winrt::com_ptr<ICustomBlurEffect> m_customBlurEffect{ nullptr };

		static inline constexpr D2D1_PIXEL_FORMAT c_blurBufferPixelFormat{ DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE };
	public:
		CGlassEffect(ID2D1DeviceContext* deviceContext) { winrt::copy_from_abi(m_deviceContext, deviceContext); }
		HRESULT STDMETHODCALLTYPE SetSourceRect(const D2D1_RECT_F& rect) override;
		HRESULT STDMETHODCALLTYPE Invalidate(
			ID2D1Bitmap1* backdropBitmap,
			const D2D1_RECT_F& rect,
			const D2D1_COLOR_F& color,
			float glassOpacity,
			float blurAmount
		) override;
		HRESULT STDMETHODCALLTYPE Render(
			ID2D1Geometry* geometry
		) override;
	};
}

HRESULT STDMETHODCALLTYPE GlassEffectManager::CGlassEffect::SetSourceRect(const D2D1_RECT_F& rect)
{
	if (
		(m_sourceRect.right - m_sourceRect.left) != (rect.right - rect.left) ||
		(m_sourceRect.bottom - m_sourceRect.top) != (rect.bottom - rect.top)
	)
	{
		m_recreateBlurBuffer = true;
	}
	m_sourceRect = rect;

	return S_OK;
}
HRESULT STDMETHODCALLTYPE GlassEffectManager::CGlassEffect::Invalidate(
	ID2D1Bitmap1* backdropBitmap,
	const D2D1_RECT_F& rect,
	const D2D1_COLOR_F& color,
	float glassOpacity,
	float blurAmount
)
{
	m_backdropPixelFormat = backdropBitmap->GetPixelFormat();
	auto samePixelFormat{ m_backdropPixelFormat.format == c_blurBufferPixelFormat.format && m_backdropPixelFormat.alphaMode == c_blurBufferPixelFormat.alphaMode };
	m_recreateBlurBuffer = m_recreateBlurBuffer ? samePixelFormat : m_recreateBlurBuffer;

	if (!m_customBlurEffect)
	{
		m_customBlurEffect = winrt::make<CCustomBlurEffect>(m_deviceContext.get());
	}
	if (!m_fallbackBlurBuffer)
	{
		RETURN_IF_FAILED(
			m_deviceContext->CreateEffect(
				CLSID_D2D1Flood,
				m_fallbackBlurBuffer.put()
			)
		);
		RETURN_IF_FAILED(
			m_fallbackBlurBuffer->SetValue(
				D2D1_FLOOD_PROP_COLOR,
				D2D1::ColorF(0.f, 0.f, 0.f, 0.f)
			)
		);
	}
	if (!m_colorEffect)
	{
		RETURN_IF_FAILED(
			m_deviceContext->CreateEffect(
				CLSID_D2D1Flood,
				m_colorEffect.put()
			)
		);
		RETURN_IF_FAILED(
			m_colorEffect->SetValue(
				D2D1_FLOOD_PROP_COLOR,
				D2D1::ColorF(0.f, 0.f, 0.f, 0.f)
			)
		);
	}
	if (m_recreateBlurBuffer)
	{
		m_recreateBlurBuffer = false;

		RETURN_IF_FAILED(
			m_deviceContext->CreateBitmap(
				D2D1::SizeU(
					dwmcore::PixelAlign(m_sourceRect.right - m_sourceRect.left),
					dwmcore::PixelAlign(m_sourceRect.bottom - m_sourceRect.top)
				),
				nullptr,
				0,
				D2D1::BitmapProperties1(
					D2D1_BITMAP_OPTIONS_NONE,
					c_blurBufferPixelFormat
				),
				m_blurBuffer.put()
			)
		);
	}

	m_color = color;
	m_glassOpacity = glassOpacity;
	RETURN_IF_FAILED(
		m_colorEffect->SetValue(
			D2D1_FLOOD_PROP_COLOR,
			D2D1::Vector4F(
				color.r * (color.a * glassOpacity),
				color.g * (color.a * glassOpacity),
				color.b * (color.a * glassOpacity),
				color.a * glassOpacity
			)
		)
	);
	if (samePixelFormat)
	{
		D2D1_POINT_2U dstPoint
		{
			dwmcore::PixelAlign(rect.left - m_sourceRect.left),
			dwmcore::PixelAlign(rect.top - m_sourceRect.top)
		};
		D2D1_RECT_U copyRect
		{
			dwmcore::PixelAlign(rect.left),
			dwmcore::PixelAlign(rect.top),
			dwmcore::PixelAlign(rect.right),
			dwmcore::PixelAlign(rect.bottom)
		};
		RETURN_IF_FAILED(
			m_blurBuffer->CopyFromBitmap(
				&dstPoint,
				backdropBitmap,
				&copyRect
			)
		);
		m_sourceRectBackup = m_sourceRect;
		m_desktopSize = backdropBitmap->GetSize();
	}

	D2D1_RECT_F invalidInputRect
	{
		rect.left - m_sourceRect.left,
		rect.top - m_sourceRect.top,
		rect.left - m_sourceRect.left + (rect.right - rect.left),
		rect.top - m_sourceRect.top + (rect.bottom - rect.top)
	};
	/*OutputDebugStringW(
		std::format(
			L"invalidInputRect:[{},{},{},{}]\n",
			invalidInputRect.left,
			invalidInputRect.top,
			invalidInputRect.right,
			invalidInputRect.bottom
		).c_str()
	);*/
	auto backdropSize{ backdropBitmap->GetSize() };
	D2D1_SIZE_F imageSize{ m_sourceRect.right - m_sourceRect.left, m_sourceRect.bottom - m_sourceRect.top };
	auto truncatedWidth{ m_sourceRect.right - backdropSize.width };
	auto truncatedHeight{ m_sourceRect.bottom - backdropSize.height };
	D2D1_RECT_F imageBounds
	{
		max(0.f - m_sourceRect.left, 0.f),
		max(0.f - m_sourceRect.top, 0.f),
		truncatedWidth > 0.f ? max(imageSize.width - truncatedWidth, 0.f) : imageSize.width,
		truncatedHeight > 0.f ? max(imageSize.height - truncatedHeight, 0.f) : imageSize.height
	};

	winrt::com_ptr<ID2D1Image> inputImage{};
	if (!samePixelFormat)
	{
		m_fallbackBlurBuffer->GetOutput(
			inputImage.put()
		);
	}
	else
	{
		winrt::copy_from_abi(inputImage, static_cast<ID2D1Image*>(m_blurBuffer.get()));
	}
	m_customBlurEffect->Invalidate(
		inputImage.get(),
		invalidInputRect,
		imageBounds,
		!samePixelFormat,
		blurAmount
	);

	return S_OK;
}
HRESULT STDMETHODCALLTYPE GlassEffectManager::CGlassEffect::Render(
	ID2D1Geometry* geometry
)
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
			(m_backdropPixelFormat.alphaMode == D2D1_ALPHA_MODE_IGNORE ? D2D1_LAYER_OPTIONS1_IGNORE_ALPHA : D2D1_LAYER_OPTIONS1_NONE) | D2D1_LAYER_OPTIONS1_INITIALIZE_FROM_BACKGROUND
		),
		nullptr
	);
	m_customBlurEffect->Draw(bounds);
	m_deviceContext->DrawImage(
		m_colorEffect.get(), 
		D2D1::Point2F(bounds.left, bounds.top), 
		D2D1::RectF(0.f, 0.f, bounds.right - bounds.left, bounds.bottom - bounds.top), 
		D2D1_INTERPOLATION_MODE_NEAREST_NEIGHBOR
	);
	/*RETURN_IF_FAILED(
		ReflectionRenderer::Draw(
			m_deviceContext.get(),
			D2D1::Point2F(
				m_sourceRect.left,
				m_sourceRect.top
			),
			m_desktopSize,
			bounds
		)
	);*/
	m_deviceContext->PopLayer();
	m_sourceRect = m_sourceRectBackup;

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