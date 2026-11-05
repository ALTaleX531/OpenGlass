#pragma once
#include "framework.hpp"
#include "cpprt.hpp"

namespace OpenGlass
{
	// Owns views for the selected bitmap. Callers choose their lifetime.
	class CGlassRenderTarget
	{
		winrt::com_ptr<ID2D1Bitmap1> m_bitmap;
		winrt::com_ptr<ID3D11Texture2D> m_texture;
		winrt::com_ptr<ID3D11RenderTargetView> m_renderTargetView;
		winrt::com_ptr<ID3D11ShaderResourceView> m_shaderResourceView;
		UINT m_subresource{};
	public:
		HRESULT Ensure(ID2D1DeviceContext* context);
		ID3D11Texture2D* GetTexture() const { return m_texture.get(); }
		UINT GetSubresource() const { return m_subresource; }
		ID3D11RenderTargetView* GetRenderTargetView() const { return m_renderTargetView.get(); }
		ID3D11ShaderResourceView* GetShaderResourceView() const { return m_shaderResourceView.get(); }
	};
}
