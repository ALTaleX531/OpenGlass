#pragma once
#include "framework.hpp"
#include "dwmcoreProjection.hpp"
#include "Buffer2D.hpp"

namespace OpenGlass
{
	class CGlassSafetyZoneLayer
	{
		CBuffer2D m_safetyZoneBufferVertical{};
		CBuffer2D m_safetyZoneBufferHorizontal{};
		D2D1_RECT_U m_safetyZoneBounds[4]{};
		winrt::com_ptr<ID3D11Texture2D> m_renderTargetTexture{};
	public:
		HRESULT Push(
			ID2D1DeviceContext* context,
			const D2D1_MATRIX_4X4_F& deviceTransform,
			bool rectangleIsDeviceSpace,
			const D2D1_RECT_F& originalRectangle,
			float expansion,
			D2D1_RECT_F& extendedRectangle
		);
		void Pop();
		ID3D11Texture2D* GetOwner() const { return m_renderTargetTexture.get(); }
	};
}
