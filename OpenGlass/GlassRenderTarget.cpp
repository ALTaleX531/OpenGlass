#include "pch.h"
#include "GlassRenderTarget.hpp"
#include "Util.hpp"

using namespace OpenGlass;

HRESULT CGlassRenderTarget::Ensure(ID2D1DeviceContext* context)
{
	winrt::com_ptr<ID2D1Bitmap1> bitmap;
	RETURN_IF_FAILED(Util::GetTargetBitmapFromD2DContext(context, bitmap));
	if (bitmap == m_bitmap)
	{
		return S_OK;
	}

	winrt::com_ptr<ID3D11Texture2D> texture;
	UINT subresource;
	RETURN_IF_FAILED(Util::GetTextureFromD2DBitmap(bitmap.get(), texture, &subresource));
	D3D11_TEXTURE2D_DESC desc;
	texture->GetDesc(&desc);
	winrt::com_ptr<ID3D11Device> device;
	texture->GetDevice(device.put());

	// The bitmap supplies both the typed format and the current mip/eye.
	D3D11_RENDER_TARGET_VIEW_DESC targetDesc{};
	D3D11_SHADER_RESOURCE_VIEW_DESC sourceDesc{};
	targetDesc.Format = sourceDesc.Format = bitmap->GetPixelFormat().format;
	if (desc.ArraySize > 1)
	{
		targetDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
		targetDesc.Texture2DArray = { subresource % desc.MipLevels, subresource / desc.MipLevels, 1 };
		// Texture2D shader inputs accept array views with ArraySize == 1.
		// The selected slice becomes slice zero within this view.
		sourceDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
		sourceDesc.Texture2DArray = { subresource % desc.MipLevels, 1, subresource / desc.MipLevels, 1 };
	}
	else
	{
		targetDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
		targetDesc.Texture2D.MipSlice = subresource;
		sourceDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		sourceDesc.Texture2D = { subresource, 1 };
	}
	winrt::com_ptr<ID3D11RenderTargetView> targetView;
	winrt::com_ptr<ID3D11ShaderResourceView> sourceView;
	RETURN_IF_FAILED(device->CreateRenderTargetView(texture.get(), &targetDesc, targetView.put()));
	if (desc.BindFlags & D3D11_BIND_SHADER_RESOURCE)
	{
		RETURN_IF_FAILED(device->CreateShaderResourceView(texture.get(), &sourceDesc, sourceView.put()));
	}
	m_texture = std::move(texture);
	m_renderTargetView = std::move(targetView);
	m_shaderResourceView = std::move(sourceView);
	m_subresource = subresource;
	m_bitmap = std::move(bitmap);
	return S_OK;
}
