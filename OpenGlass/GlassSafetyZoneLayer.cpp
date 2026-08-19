#include "pch.h"
#include "GlassKernel.hpp"
#include "GlassSafetyZoneLayer.hpp"
#include "DxgiPrivates.hpp"

using namespace OpenGlass;

HRESULT CGlassSafetyZoneLayer::Push(
	ID2D1DeviceContext* context,
	const D2D1_MATRIX_4X4_F& deviceTransform,
	bool rectangleIsDeviceSpace,
	const D2D1_RECT_F& originalRectangle,
	float expansion,
	D2D1_RECT_F& extendedRectangle
)
{
	winrt::com_ptr<ID2D1Bitmap1> renderTargetBitmap{ nullptr };
	RETURN_IF_FAILED(Util::GetTargetBitmapFromD2DContext(context, renderTargetBitmap));

	extendedRectangle = originalRectangle;
	auto pushCleanupScope = wil::scope_exit([this]
	{
		m_renderTargetTexture = nullptr;
	});
	const auto targetSize = renderTargetBitmap->GetPixelSize();
	const D2D1_RECT_F targetRect
	{
		0.f,
		0.f,
		static_cast<float>(targetSize.width),
		static_cast<float>(targetSize.height)
	};

	D2D1_RECT_F originalDeviceRectangle = RectF::ResolveDeviceBounds(
		originalRectangle,
		deviceTransform,
		rectangleIsDeviceSpace
	);
	if (!RectF::IntersectUnsafe(originalDeviceRectangle, targetRect))
	{
		return S_OK;
	}

	extendedRectangle =
	{
		originalRectangle.left - expansion,
		originalRectangle.top - expansion,
		originalRectangle.right + expansion,
		originalRectangle.bottom + expansion
	};
	D2D1_RECT_F extendedDeviceRectangle = RectF::ResolveDeviceBounds(
		extendedRectangle,
		deviceTransform,
		rectangleIsDeviceSpace
	);
	if (
		!RectF::IntersectUnsafe(extendedDeviceRectangle, targetRect) ||
		extendedDeviceRectangle.left > originalDeviceRectangle.left ||
		extendedDeviceRectangle.top > originalDeviceRectangle.top ||
		extendedDeviceRectangle.right < originalDeviceRectangle.right ||
		extendedDeviceRectangle.bottom < originalDeviceRectangle.bottom ||
		(
			extendedDeviceRectangle.left == originalDeviceRectangle.left &&
			extendedDeviceRectangle.top == originalDeviceRectangle.top &&
			extendedDeviceRectangle.right == originalDeviceRectangle.right &&
			extendedDeviceRectangle.bottom == originalDeviceRectangle.bottom
		)
	)
	{
		extendedRectangle = originalRectangle;
		return S_OK;
	}

	BOOL hwProtectionEnabled = FALSE;
	D3D11_TEXTURE2D_DESC renderTargetDesc{};

	RETURN_IF_FAILED(
		Util::GetTextureFromD2DBitmap(
			renderTargetBitmap.get(),
			m_renderTargetTexture
		)
	);
	RETURN_IF_FAILED(
		Util::GetDescAndHwProtectionStateFromTexture(
			m_renderTargetTexture.get(),
			renderTargetDesc,
			hwProtectionEnabled
		)
	);

	winrt::com_ptr<ID3D11Device> d3dDevice{};
	m_renderTargetTexture->GetDevice(d3dDevice.put());
	winrt::com_ptr<ID3D11DeviceContext> d3dContext{};
	d3dDevice->GetImmediateContext(d3dContext.put());

	m_safetyZoneBounds[0] =
	{
		static_cast<UINT32>(extendedDeviceRectangle.left),
		static_cast<UINT32>(extendedDeviceRectangle.top),
		static_cast<UINT32>(originalDeviceRectangle.left),
		static_cast<UINT32>(originalDeviceRectangle.bottom)
	};
	m_safetyZoneBounds[1] =
	{
		static_cast<UINT32>(originalDeviceRectangle.left),
		static_cast<UINT32>(extendedDeviceRectangle.top),
		static_cast<UINT32>(extendedDeviceRectangle.right),
		static_cast<UINT32>(originalDeviceRectangle.top)
	};
	m_safetyZoneBounds[2] =
	{
		static_cast<UINT32>(originalDeviceRectangle.right),
		static_cast<UINT32>(originalDeviceRectangle.top),
		static_cast<UINT32>(extendedDeviceRectangle.right),
		static_cast<UINT32>(extendedDeviceRectangle.bottom)
	};
	m_safetyZoneBounds[3] =
	{
		static_cast<UINT32>(extendedDeviceRectangle.left),
		static_cast<UINT32>(originalDeviceRectangle.bottom),
		static_cast<UINT32>(originalDeviceRectangle.right),
		static_cast<UINT32>(extendedDeviceRectangle.bottom)
	};

	const UINT verticalWidth =
		wil::rect_width(m_safetyZoneBounds[0]) +
		wil::rect_width(m_safetyZoneBounds[2]);
	const UINT verticalHeight = std::max(
		wil::rect_height(m_safetyZoneBounds[0]),
		wil::rect_height(m_safetyZoneBounds[2])
	);
	const UINT horizontalWidth = std::max(
		wil::rect_width(m_safetyZoneBounds[1]),
		wil::rect_width(m_safetyZoneBounds[3])
	);
	const UINT horizontalHeight =
		wil::rect_height(m_safetyZoneBounds[1]) +
		wil::rect_height(m_safetyZoneBounds[3]);

	RETURN_IF_FAILED(m_safetyZoneBufferVertical.Ensure(d3dDevice.get(), verticalWidth, verticalHeight, renderTargetDesc, 0, hwProtectionEnabled));
	RETURN_IF_FAILED(m_safetyZoneBufferHorizontal.Ensure(d3dDevice.get(), horizontalWidth, horizontalHeight, renderTargetDesc, 0, hwProtectionEnabled));

	if (!wil::rect_is_empty(m_safetyZoneBounds[0]))
	{
		Util::CopyTextureRegion(
			d3dContext.get(),
			m_renderTargetTexture.get(),
			m_safetyZoneBufferVertical.m_texture.get(),
			m_safetyZoneBounds[0],
			0,
			0
		);
	}
	if (!wil::rect_is_empty(m_safetyZoneBounds[2]))
	{
		Util::CopyTextureRegion(
			d3dContext.get(),
			m_renderTargetTexture.get(),
			m_safetyZoneBufferVertical.m_texture.get(),
			m_safetyZoneBounds[2],
			wil::rect_width(m_safetyZoneBounds[0]),
			0
		);
	}
	if (!wil::rect_is_empty(m_safetyZoneBounds[1]))
	{
		Util::CopyTextureRegion(
			d3dContext.get(),
			m_renderTargetTexture.get(),
			m_safetyZoneBufferHorizontal.m_texture.get(),
			m_safetyZoneBounds[1],
			0,
			0
		);
	}
	if (!wil::rect_is_empty(m_safetyZoneBounds[3]))
	{
		Util::CopyTextureRegion(
			d3dContext.get(),
			m_renderTargetTexture.get(),
			m_safetyZoneBufferHorizontal.m_texture.get(),
			m_safetyZoneBounds[3],
			0,
			wil::rect_height(m_safetyZoneBounds[1])
		);
	}

	d3dContext->Flush();
	pushCleanupScope.release();
	return S_OK;
}

void CGlassSafetyZoneLayer::Pop()
{
	if (!m_renderTargetTexture)
	{
		ZeroMemory(m_safetyZoneBounds, sizeof(m_safetyZoneBounds));
		return;
	}

	winrt::com_ptr<ID3D11Device> d3dDevice{};
	m_renderTargetTexture->GetDevice(d3dDevice.put());
	winrt::com_ptr<ID3D11DeviceContext> d3dContext{};
	d3dDevice->GetImmediateContext(d3dContext.put());

	if (!wil::rect_is_empty(m_safetyZoneBounds[0]))
	{
		const D2D1_RECT_U srcRect
		{
			0u,
			0u,
			wil::rect_width(m_safetyZoneBounds[0]),
			wil::rect_height(m_safetyZoneBounds[0])
		};
		Util::CopyTextureRegion(
			d3dContext.get(),
			m_safetyZoneBufferVertical.m_texture.get(),
			m_renderTargetTexture.get(),
			srcRect,
			m_safetyZoneBounds[0].left,
			m_safetyZoneBounds[0].top
		);
	}
	if (!wil::rect_is_empty(m_safetyZoneBounds[2]))
	{
		const D2D1_RECT_U srcRect
		{
			wil::rect_width(m_safetyZoneBounds[0]),
			0u,
			wil::rect_width(m_safetyZoneBounds[0]) + wil::rect_width(m_safetyZoneBounds[2]),
			wil::rect_height(m_safetyZoneBounds[2])
		};
		Util::CopyTextureRegion(
			d3dContext.get(),
			m_safetyZoneBufferVertical.m_texture.get(),
			m_renderTargetTexture.get(),
			srcRect,
			m_safetyZoneBounds[2].left,
			m_safetyZoneBounds[2].top
		);
	}
	if (!wil::rect_is_empty(m_safetyZoneBounds[1]))
	{
		const D2D1_RECT_U srcRect
		{
			0u,
			0u,
			wil::rect_width(m_safetyZoneBounds[1]),
			wil::rect_height(m_safetyZoneBounds[1])
		};
		Util::CopyTextureRegion(
			d3dContext.get(),
			m_safetyZoneBufferHorizontal.m_texture.get(),
			m_renderTargetTexture.get(),
			srcRect,
			m_safetyZoneBounds[1].left,
			m_safetyZoneBounds[1].top
		);
	}
	if (!wil::rect_is_empty(m_safetyZoneBounds[3]))
	{
		const D2D1_RECT_U srcRect
		{
			0u,
			wil::rect_height(m_safetyZoneBounds[1]),
			wil::rect_width(m_safetyZoneBounds[3]),
			wil::rect_height(m_safetyZoneBounds[1]) + wil::rect_height(m_safetyZoneBounds[3])
		};
		Util::CopyTextureRegion(
			d3dContext.get(),
			m_safetyZoneBufferHorizontal.m_texture.get(),
			m_renderTargetTexture.get(),
			srcRect,
			m_safetyZoneBounds[3].left,
			m_safetyZoneBounds[3].top
		);
	}

	d3dContext->Flush();
	m_renderTargetTexture = nullptr;
	d3dContext = nullptr;
	ZeroMemory(m_safetyZoneBounds, sizeof(m_safetyZoneBounds));
}
