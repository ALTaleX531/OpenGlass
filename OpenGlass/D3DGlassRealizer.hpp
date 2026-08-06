#pragma once
#include "framework.hpp"
#include "cpprt.hpp"
#include "dwmcoreProjection.hpp"
#include "AeroEffect.hpp"
#include "Shared.hpp"
#include "Buffer2D.hpp"

namespace OpenGlass
{
	struct CConstantBufferVS
	{
		D2D1_VECTOR_2F viewportSize;
		D2D1_VECTOR_2F _padding;
	};

	struct CConstantBufferPS
	{
		D2D1_VECTOR_4F conv[8];
		D2D1_VECTOR_4F afterglow;
		D2D1_VECTOR_4F blurBalance;
		D2D1_VECTOR_4F color;
		D2D1_VECTOR_4F fallback;
	};

	// 80 bytes/vertex：pos(2) + baseUV(2) + 8 samples packed into 4 float4
	struct BlurVertex
	{
		float x;
		float y;
		float u;
		float v;
		float s01[4];
		float s23[4];
		float s45[4];
		float s67[4];
	};

	struct DwmHwSamples
	{
		int radius{}; // filterRadius = ceil(sigma * 2.3)
		std::array<float, 8> weights{};
		std::array<float, 8> offsets{}; // pixel offsets (match DWM sample calculator output)
	};

	class CD3DGlassRealizer
	{
		bool m_initialized{ false };

		float										m_sigma{ 0.f };
		DwmHwSamples								m_dwmHwSamples{};

		CConstantBufferVS						    m_cbVS{};
		CConstantBufferPS						    m_cbPS{};
		winrt::com_ptr<ID3D11VertexShader>          m_vertexShader{ nullptr };
		winrt::com_ptr<ID3D11PixelShader>           m_pixelShaderBlurH{ nullptr };
		winrt::com_ptr<ID3D11PixelShader>           m_pixelShaderBlurV{ nullptr };
		winrt::com_ptr<ID3D11PixelShader>           m_pixelShaderBlurV_Vista{ nullptr };
		winrt::com_ptr<ID3D11InputLayout>           m_vertexLayout{ nullptr };
		winrt::com_ptr<ID3D11Buffer>                m_vertexBuffer{ nullptr };

		// Dedicated buffers: blur pass uses m_vertexBuffer (non-indexed); compose pass uses the following
		winrt::com_ptr<ID3D11Buffer>                m_vertexBufferCompose{ nullptr };
		winrt::com_ptr<ID3D11Buffer>                m_indexBufferCompose{ nullptr };

		// Dynamic capacities (counts, not bytes)
		size_t                                      m_vertexCapacityBlur{ 128 };
		size_t                                      m_vertexCapacityCompose{ 128 };
		size_t                                      m_indexCapacityCompose{ 128 };
		winrt::com_ptr<ID3D11Buffer>                m_constantBufferVS{ nullptr };
		winrt::com_ptr<ID3D11Buffer>                m_constantBufferPS{ nullptr };
		winrt::com_ptr<ID3D11SamplerState>          m_samplerLinearClamp{ nullptr };

		// first stage buffer is quarter width, full height
		CBuffer2D									m_quarterResBuffer{};

		// Fallback when the caller can't provide an SRV for the back buffer:
		// copy the needed region from backBuffer into this texture and sample it in pass 1.
		CBuffer2D							m_intermediateBuffer{};

		winrt::com_ptr<ID3D11RasterizerState>       m_rasterizerStateScissor{ nullptr };
		winrt::com_ptr<ID3D11BlendState>            m_blendStateOpaque{ nullptr };

		HRESULT CreateDeviceResources(ID3D11Device* device);
		HRESULT Initialize(ID3D11Device* device);
		HRESULT EnsureVertexBufferCapacity(
			ID3D11Device* device,
			size_t requiredVertexCount,
			winrt::com_ptr<ID3D11Buffer>& vb,
			size_t& vertexCapacity
		);
		HRESULT EnsureMeshBuffersCapacity(
			ID3D11Device* device,
			size_t requiredVertexCount,
			size_t requiredIndexCount,
			winrt::com_ptr<ID3D11Buffer>& vb,
			winrt::com_ptr<ID3D11Buffer>& ib,
			size_t& vertexCapacity,
			size_t& indexCapacity
		);
		size_t GrowCapacity(size_t current, size_t required) const noexcept;
		enum class BlurAxis
		{
			Horizontal,
			Vertical,
		};
		void FillBlurSamples(
			BlurVertex& vertex,
			BlurAxis axis,
			float baseU,
			float baseV,
			float uvTextureWidth,
			float uvTextureHeight,
			float sampleCenterBias
		) const noexcept;
		BlurVertex MakeBlurVertex(
			float x,
			float y,
			UINT backBufferWidth,
			UINT backBufferHeight
		) const noexcept;
		void SubmitBlurRectBatch(
			const std::span<const D2D1_RECT_F>& rectangles,
			const D2D1_RECT_F& drawingWorldBounds,
			const D2D1_RECT_F& geometryBounds,
			float expansion,
			UINT backBufferWidth,
			UINT backBufferHeight,
			std::vector<BlurVertex>& vertices
		) const;
		HRESULT Tessellate(
			ID2D1Geometry* geometry,
			std::vector<BlurVertex>& vertices,
			std::vector<WORD>& indices,
			BlurAxis axis,
			float positionScaleX,
			float positionScaleY,
			float uvScaleX,
			float uvScaleY,
			float uvTextureWidth,
			float uvTextureHeight,
			float sampleCenterBias
		);
		void CalculateDwmHwSamples(float sigma);
	public:
		HRESULT Render(
			ID3D11Device* device,
			ID3D11DeviceContext* context,
			ID3D11Texture2D* backBuffer,
			ID3D11RenderTargetView* backBufferRTV,
			ID3D11ShaderResourceView* backBufferSRV,
			ID2D1Geometry* geometry,
			const D2D1_RECT_F& drawingWorldBounds,
			const std::span<const D2D1_RECT_F>& rectangles,
			const CAeroParams& params,
			Shared::GlassType type
		);
		static float GetBlurRadius(float blurAmount);
	};
}
