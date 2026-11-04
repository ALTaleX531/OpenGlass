#pragma once
#include "framework.hpp"
#include "wil.hpp"

namespace OpenGlass
{
	struct CBuffer2D
	{
		winrt::com_ptr<ID3D11Texture2D> m_texture{};
		winrt::com_ptr<ID3D11RenderTargetView> m_rtv{ nullptr };
		winrt::com_ptr<ID3D11ShaderResourceView> m_srv{ nullptr };

		winrt::com_ptr<ID2D1Bitmap1> m_bitmap{};

		void Reset()
		{
			m_texture = nullptr;
			m_rtv = nullptr;
			m_srv = nullptr;

			m_bitmap = nullptr;
		}
		HRESULT Ensure(
			ID3D11Device* device,
			UINT width,
			UINT height,
			const D3D11_TEXTURE2D_DESC& baseDesc,
			UINT bindFlags,
			bool hwProtectionEnabled
		)
		{
			const UINT safeWidth = std::max(1u, width);
			const UINT safeHeight = std::max(1u, height);
			const UINT desiredMiscFlags =
				(baseDesc.MiscFlags & ~D3D11_RESOURCE_MISC_HW_PROTECTED) |
				(hwProtectionEnabled ? D3D11_RESOURCE_MISC_HW_PROTECTED : 0u);

			if (m_texture)
			{
				D3D11_TEXTURE2D_DESC current;
				m_texture->GetDesc(&current);
				if (
					current.Width == safeWidth &&
					current.Height == safeHeight &&
					current.Format == baseDesc.Format &&
					current.SampleDesc.Count == baseDesc.SampleDesc.Count &&
					current.SampleDesc.Quality == baseDesc.SampleDesc.Quality &&
					current.BindFlags == bindFlags &&
					current.MiscFlags == desiredMiscFlags
				)
				{
					return S_OK;
				}
			}

			D3D11_TEXTURE2D_DESC desc{};
			desc.Width = safeWidth;
			desc.Height = safeHeight;
			desc.MipLevels = 1;
			desc.ArraySize = 1;
			desc.Format = baseDesc.Format;
			desc.SampleDesc = baseDesc.SampleDesc;
			desc.Usage = D3D11_USAGE_DEFAULT;
			desc.BindFlags = bindFlags;
			desc.CPUAccessFlags = 0;
			desc.MiscFlags = desiredMiscFlags;

			winrt::com_ptr<ID3D11Texture2D> texture{};
			RETURN_IF_FAILED(device->CreateTexture2D(&desc, nullptr, texture.put()));

			m_texture = std::move(texture);
			m_rtv = nullptr;
			m_srv = nullptr;
			m_bitmap = nullptr;
			return S_OK;
		}
		HRESULT EnsureRTV(ID3D11Device* device)
		{
			if (!m_texture)
			{
				return E_FAIL;
			}
			if (m_rtv)
			{
				return S_OK;
			}
			RETURN_IF_FAILED(device->CreateRenderTargetView(m_texture.get(), nullptr, m_rtv.put()));
			return S_OK;
		}
		HRESULT EnsureSRV(ID3D11Device* device)
		{
			if (!m_texture)
			{
				return E_FAIL;
			}
			if (m_srv)
			{
				return S_OK;
			}
			RETURN_IF_FAILED(device->CreateShaderResourceView(m_texture.get(), nullptr, m_srv.put()));
			return S_OK;
		}
		HRESULT EnsureD2D(
			ID2D1DeviceContext* d2dContext,
			const D2D1_BITMAP_PROPERTIES1& properties
		)
		{
			if (!m_texture)
			{
				return D2DERR_UNSUPPORTED_OPERATION;
			}

			// CustomBlurEffect::Reset disconnects inputs but retains Build's image-pointer cache.
			// A new wrapper triggers reconnection even when parameters are unchanged.
			// Scratch callers reuse a compatible wrapper before reaching this method.
			winrt::com_ptr<IDXGISurface> dxgiSurface{};
			RETURN_IF_FAILED(m_texture->QueryInterface(dxgiSurface.put()));
			RETURN_IF_FAILED(
				d2dContext->CreateBitmapFromDxgiSurface(
					dxgiSurface.get(),
					&properties,
					m_bitmap.put()
				)
			);
			return S_OK;
		}
	};
}
