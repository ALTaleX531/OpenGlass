#include "pch.h"
#include "resource.h"
#include "Util.hpp"
#include "D3DGlassRealizer.hpp"
#include "Shared.hpp"
#include "DxgiPrivates.hpp"

using namespace OpenGlass;

namespace OpenGlass
{
	constexpr int c_maxTriangle = 2048;

	[[nodiscard]] D3D11_RECT ToScissorRect(
		const D2D1_RECT_F& rc,
		LONG width,
		LONG height
	)
	{
		auto scissorRect = RectF::ToRectL(rc);
		scissorRect.left = std::max(scissorRect.left, 0l);
		scissorRect.top = std::max(scissorRect.top, 0l);
		scissorRect.right = std::min(scissorRect.right, width);
		scissorRect.bottom = std::min(scissorRect.bottom, height);

		return scissorRect;
	}

	// https://learn.microsoft.com/en-us/archive/msdn-magazine/2014/march/directx-factor-triangles-and-tessellation
	class CTessellationSink : public winrt::implements<CTessellationSink, ID2D1TessellationSink, winrt::non_agile, winrt::no_weak_ref>
	{
		std::vector<D2D1_TRIANGLE> m_triangles{};
	public:
		CTessellationSink() { m_triangles.reserve(32); }
		STDMETHOD_(void, AddTriangles)(_In_reads_(trianglesCount) CONST D2D1_TRIANGLE* triangles, UINT32 trianglesCount) override
		{
			const auto triangleViews = std::span{ triangles, trianglesCount };
			m_triangles.insert(m_triangles.end(), triangleViews.begin(), triangleViews.end());
		}
		STDMETHOD(Close)() override
		{
			return S_OK;
		}
		const std::vector<D2D1_TRIANGLE>& GetTriangles() const
		{
			return m_triangles;
		}
	};
}

HRESULT CD3DGlassRealizer::CreateDeviceResources(ID3D11Device* device)
{
	std::span<const UCHAR> vsBytes{};
	RETURN_IF_FAILED(Util::GetResDataView(vsBytes, IDR_RCDATA_BLUR_VS));

	RETURN_IF_FAILED(
		device->CreateVertexShader(
			vsBytes.data(),
			vsBytes.size_bytes(),
			nullptr,
			m_vertexShader.put()
		)
	);

	D3D11_INPUT_ELEMENT_DESC layout[]
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 8, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 2, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 32, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 3, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 48, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 4, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 64, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};
	UINT numElements{ ARRAYSIZE(layout) };

	RETURN_IF_FAILED(
		device->CreateInputLayout(
			layout,
			numElements,
			vsBytes.data(),
			vsBytes.size_bytes(),
			m_vertexLayout.put()
		)
	);

	std::span<const UCHAR> psBytesBlurH{};
	RETURN_IF_FAILED(Util::GetResDataView(psBytesBlurH, IDR_RCDATA_BLUR_PS_BLURH));

	RETURN_IF_FAILED(
		device->CreatePixelShader(
			psBytesBlurH.data(),
			psBytesBlurH.size_bytes(),
			nullptr,
			m_pixelShaderBlurH.put()
		)
	);

	std::span<const UCHAR> psBytesBlurV{};
	RETURN_IF_FAILED(Util::GetResDataView(psBytesBlurV, IDR_RCDATA_BLUR_PS_BLURV));

	RETURN_IF_FAILED(
		device->CreatePixelShader(
			psBytesBlurV.data(),
			psBytesBlurV.size_bytes(),
			nullptr,
			m_pixelShaderBlurV.put()
		)
	);

	std::span<const UCHAR> psBytesBlurV_Vista{};
	RETURN_IF_FAILED(Util::GetResDataView(psBytesBlurV_Vista, IDR_RCDATA_BLUR_PS_BLURV_VISTA));

	RETURN_IF_FAILED(
		device->CreatePixelShader(
			psBytesBlurV_Vista.data(),
			psBytesBlurV_Vista.size_bytes(),
			nullptr,
			m_pixelShaderBlurV_Vista.put()
		)
	);

	// Create small initial buffers (128) and grow dynamically on demand
	D3D11_BUFFER_DESC bd{};
	bd.Usage = D3D11_USAGE_DYNAMIC;
	bd.ByteWidth = static_cast<UINT>(sizeof(BlurVertex) * m_vertexCapacityBlur);
	bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	RETURN_IF_FAILED(device->CreateBuffer(&bd, nullptr, m_vertexBuffer.put()));

	// Compose buffers
	bd.Usage = D3D11_USAGE_DYNAMIC;
	bd.ByteWidth = static_cast<UINT>(sizeof(BlurVertex) * m_vertexCapacityCompose);
	bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	RETURN_IF_FAILED(device->CreateBuffer(&bd, nullptr, m_vertexBufferCompose.put()));

	bd.Usage = D3D11_USAGE_DYNAMIC;
	bd.ByteWidth = static_cast<UINT>(sizeof(WORD) * m_indexCapacityCompose);
	bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
	bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	RETURN_IF_FAILED(device->CreateBuffer(&bd, nullptr, m_indexBufferCompose.put()));

	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.ByteWidth = sizeof(CConstantBufferVS);
	bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	bd.CPUAccessFlags = 0;
	RETURN_IF_FAILED(device->CreateBuffer(&bd, nullptr, m_constantBufferVS.put()));

	bd = {};
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.ByteWidth = sizeof(CConstantBufferPS);
	bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	bd.CPUAccessFlags = 0;
	RETURN_IF_FAILED(device->CreateBuffer(&bd, nullptr, m_constantBufferPS.put()));

	D3D11_SAMPLER_DESC sampDesc{};
	// Win7 DWM uses D3D10_FILTER_MIN_MAG_MIP_LINEAR for the common blur sampler (index 4).
	// Using MIN_MAG_MIP_LINEAR here keeps behavior aligned (our textures are single-mip anyway).
	sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	// Clamp sampler (closer to Win7 DWM edge behavior)
	sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
	sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
	sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
	sampDesc.MinLOD = 0;
	sampDesc.MaxLOD = D3D11_FLOAT32_MAX;
	RETURN_IF_FAILED(device->CreateSamplerState(&sampDesc, m_samplerLinearClamp.put()));

	D3D11_RASTERIZER_DESC rasterDesc{};
	// Match Win7 DWM 2D rasterizer (m_rgRasterizerStates[0] used by CHwSurfaceRenderTarget::Ensure2DState):
	// solid fill, no cull, scissor enabled, depth clip enabled.
	rasterDesc.FillMode = D3D11_FILL_SOLID;
	rasterDesc.CullMode = D3D11_CULL_NONE;
	rasterDesc.FrontCounterClockwise = FALSE;
	rasterDesc.DepthBias = 0;
	rasterDesc.DepthBiasClamp = 0.0f;
	rasterDesc.SlopeScaledDepthBias = 0.0f;
	rasterDesc.DepthClipEnable = TRUE;
	rasterDesc.ScissorEnable = TRUE;
	rasterDesc.MultisampleEnable = FALSE;
	rasterDesc.AntialiasedLineEnable = FALSE;
	RETURN_IF_FAILED(device->CreateRasterizerState(&rasterDesc, m_rasterizerStateScissor.put()));

	// Explicit opaque blend state (DWM frequently sets BlendModeType::Float with nullptr blend color,
	// so we avoid relying on implicit defaults / leftover state).
	D3D11_BLEND_DESC blendDesc{};
	blendDesc.AlphaToCoverageEnable = FALSE;
	blendDesc.IndependentBlendEnable = FALSE;
	blendDesc.RenderTarget[0].BlendEnable = FALSE;
	blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
	blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_ZERO;
	blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
	blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
	blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
	RETURN_IF_FAILED(device->CreateBlendState(&blendDesc, m_blendStateOpaque.put()));

	return S_OK;
}

size_t CD3DGlassRealizer::GrowCapacity(size_t current, size_t required) const noexcept
{
	if (required <= current)
	{
		return current;
	}

	size_t capacity = (current == 0 ? 128ull : current);
	while (capacity < required)
	{
		capacity <<= 1;
	}
	return capacity;
}

void CD3DGlassRealizer::FillBlurSamples(
	BlurVertex& vertex,
	BlurAxis axis,
	float baseU,
	float baseV,
	float uvTextureWidth,
	float uvTextureHeight,
	float sampleCenterBias
) const noexcept
{
	const float dim = (axis == BlurAxis::Horizontal) ? uvTextureWidth : uvTextureHeight;
	float s[16]{};
	for (int i = 0; i < 8; ++i)
	{
		float u = baseU;
		float v = baseV;
		// DWM second stage (CBlurEffect::SetupVertexBuffer): adds sampleOffset/width or /height directly.
		// (The -0.5 adjustment is only present in the accumulation-buffer path.)
		const float d = (m_dwmHwSamples.offsets[static_cast<size_t>(i)] + sampleCenterBias) / dim;
		if (axis == BlurAxis::Horizontal)
		{
			u += d;
		}
		else
		{
			v += d;
		}
		s[i * 2 + 0] = u;
		s[i * 2 + 1] = v;
	}
	memcpy(vertex.s01, &s[0], sizeof(float) * 4);
	memcpy(vertex.s23, &s[4], sizeof(float) * 4);
	memcpy(vertex.s45, &s[8], sizeof(float) * 4);
	memcpy(vertex.s67, &s[12], sizeof(float) * 4);
}

BlurVertex CD3DGlassRealizer::MakeBlurVertex(
	float x,
	float y,
	UINT backBufferWidth,
	UINT backBufferHeight
) const noexcept
{
	BlurVertex v{};
	v.x = x * 0.25f;
	v.y = y;
	// Match DWM: base texture coordinates use bounds edges (no +0.5 here).
	// Sample coordinates use (sampleOffset - 0.5)/dim, and the rasterizer's +0.5 comes from interpolation.
	v.u = x / backBufferWidth;
	v.v = y / backBufferHeight;
	const float dim = static_cast<float>(backBufferWidth);
	float s[16]{};
	for (int i = 0; i < 8; ++i)
	{
		float uu = v.u + (m_dwmHwSamples.offsets[static_cast<size_t>(i)] - 0.5f) / dim;
		s[i * 2 + 0] = uu;
		s[i * 2 + 1] = v.v;
	}
	memcpy(v.s01, &s[0], sizeof(float) * 4);
	memcpy(v.s23, &s[4], sizeof(float) * 4);
	memcpy(v.s45, &s[8], sizeof(float) * 4);
	memcpy(v.s67, &s[12], sizeof(float) * 4);
	return v;
}

void CD3DGlassRealizer::SubmitBlurRectBatch(
	const std::span<const D2D1_RECT_F>& rectangles,
	const D2D1_RECT_F& drawingWorldBounds,
	[[maybe_unused]] const D2D1_RECT_F& geometryBounds,
	float expansion,
	UINT backBufferWidth,
	UINT backBufferHeight,
	std::vector<BlurVertex>& vertices
) const
{
	// DWM: DrawPrimitiveUP(TRIANGLELIST, 6 * rectCount, stride=0x50). No index buffer.
	vertices.reserve(vertices.size() + 6 * rectangles.size());
	for (auto subRectangle : rectangles)
	{
		if (RectF::IntersectUnsafe(subRectangle, drawingWorldBounds))
		{
			subRectangle =
			{
				std::max(subRectangle.left - expansion, 0.f),
				std::max(subRectangle.top - expansion, 0.f),
				std::min(subRectangle.right + expansion, static_cast<float>(backBufferWidth)),
				std::min(subRectangle.bottom + expansion, static_cast<float>(backBufferHeight))
			};
			// Match DWM vertex order:
			// 0: (L,B)  1: (L,T)  2: (R,T)
			// 3: (L,B)  4: (R,T)  5: (R,B)
			vertices.push_back(MakeBlurVertex(subRectangle.left, subRectangle.bottom, backBufferWidth, backBufferHeight));
			vertices.push_back(MakeBlurVertex(subRectangle.left, subRectangle.top, backBufferWidth, backBufferHeight));
			vertices.push_back(MakeBlurVertex(subRectangle.right, subRectangle.top, backBufferWidth, backBufferHeight));
			vertices.push_back(MakeBlurVertex(subRectangle.left, subRectangle.bottom, backBufferWidth, backBufferHeight));
			vertices.push_back(MakeBlurVertex(subRectangle.right, subRectangle.top, backBufferWidth, backBufferHeight));
			vertices.push_back(MakeBlurVertex(subRectangle.right, subRectangle.bottom, backBufferWidth, backBufferHeight));
		}
	}
}

HRESULT CD3DGlassRealizer::EnsureVertexBufferCapacity(
	ID3D11Device* device,
	size_t requiredVertexCount,
	winrt::com_ptr<ID3D11Buffer>& vb,
	size_t& vertexCapacity
)
{
	if (requiredVertexCount <= vertexCapacity)
	{
		return S_OK;
	}

	vertexCapacity = GrowCapacity(vertexCapacity, requiredVertexCount);
	D3D11_BUFFER_DESC bd{};
	bd.Usage = D3D11_USAGE_DYNAMIC;
	bd.ByteWidth = static_cast<UINT>(sizeof(BlurVertex) * vertexCapacity);
	bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	RETURN_IF_FAILED(device->CreateBuffer(&bd, nullptr, vb.put()));
	return S_OK;
}

HRESULT CD3DGlassRealizer::EnsureMeshBuffersCapacity(
	ID3D11Device* device,
	size_t requiredVertexCount,
	size_t requiredIndexCount,
	winrt::com_ptr<ID3D11Buffer>& vb,
	winrt::com_ptr<ID3D11Buffer>& ib,
	size_t& vertexCapacity,
	size_t& indexCapacity
)
{
	bool needVB = requiredVertexCount > vertexCapacity;
	bool needIB = requiredIndexCount > indexCapacity;
	if (!needVB && !needIB)
	{
		return S_OK;
	}

	D3D11_BUFFER_DESC bd{};
	if (needVB)
	{
		vertexCapacity = GrowCapacity(vertexCapacity, requiredVertexCount);
		bd.Usage = D3D11_USAGE_DYNAMIC;
		bd.ByteWidth = static_cast<UINT>(sizeof(BlurVertex) * vertexCapacity);
		bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		RETURN_IF_FAILED(device->CreateBuffer(&bd, nullptr, vb.put()));
	}
	if (needIB)
	{
		indexCapacity = GrowCapacity(indexCapacity, requiredIndexCount);
		if (indexCapacity > std::numeric_limits<WORD>::max())
		{
			indexCapacity = std::numeric_limits<WORD>::max();
		}
		bd = {};
		bd.Usage = D3D11_USAGE_DYNAMIC;
		bd.ByteWidth = static_cast<UINT>(sizeof(WORD) * indexCapacity);
		bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
		bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		RETURN_IF_FAILED(device->CreateBuffer(&bd, nullptr, ib.put()));
	}
	return S_OK;
}

HRESULT CD3DGlassRealizer::Initialize(ID3D11Device* device)
{
	RETURN_IF_FAILED(CreateDeviceResources(device));

	m_initialized = true;
	return S_OK;
}

HRESULT CD3DGlassRealizer::Tessellate(
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
)
{
	const auto tessellationSink = winrt::make_self<CTessellationSink>();
	RETURN_IF_FAILED(
		geometry->Tessellate(
			nullptr,
			1.f,
			tessellationSink.get()
		)
	);

	const auto capacity = tessellationSink->GetTriangles().size() * 3;
	vertices.reserve(vertices.size() + capacity);
	indices.reserve(indices.size() + capacity);

	WORD baseVertexIndex = static_cast<WORD>(vertices.size());
	for (const auto& triangles : tessellationSink->GetTriangles())
	{
		BlurVertex v1{};
		v1.x = triangles.point1.x * positionScaleX;
		v1.y = triangles.point1.y * positionScaleY;
		// Match DWM: texture coordinates are derived from bounds edges (no explicit +0.5).
		// The rasterizer samples at pixel centers, so interpolation naturally accounts for the +0.5.
		v1.u = (triangles.point1.x * uvScaleX) / uvTextureWidth;
		v1.v = (triangles.point1.y * uvScaleY) / uvTextureHeight;
		FillBlurSamples(v1, axis, v1.u, v1.v, uvTextureWidth, uvTextureHeight, sampleCenterBias);
		vertices.push_back(v1);

		BlurVertex v2{};
		v2.x = triangles.point2.x * positionScaleX;
		v2.y = triangles.point2.y * positionScaleY;
		v2.u = (triangles.point2.x * uvScaleX) / uvTextureWidth;
		v2.v = (triangles.point2.y * uvScaleY) / uvTextureHeight;
		FillBlurSamples(v2, axis, v2.u, v2.v, uvTextureWidth, uvTextureHeight, sampleCenterBias);
		vertices.push_back(v2);

		BlurVertex v3{};
		v3.x = triangles.point3.x * positionScaleX;
		v3.y = triangles.point3.y * positionScaleY;
		v3.u = (triangles.point3.x * uvScaleX) / uvTextureWidth;
		v3.v = (triangles.point3.y * uvScaleY) / uvTextureHeight;
		FillBlurSamples(v3, axis, v3.u, v3.v, uvTextureWidth, uvTextureHeight, sampleCenterBias);
		vertices.push_back(v3);

		indices.push_back(baseVertexIndex++);
		indices.push_back(baseVertexIndex++);
		indices.push_back(baseVertexIndex++);
	}

	return S_OK;
}

void CD3DGlassRealizer::CalculateDwmHwSamples(float sigma)
{
	if (m_sigma == sigma)
	{
		return;
	}
	else
	{
		m_sigma = sigma;
	}

	// Mirrors Win7 DWM logic:
	//  - filterRadius = ceil(sigma * 2.3)
	//  - input samples = 2*radius + 1
	//  - pair input samples (0,1),(2,3)... and final single
	//  - hwOffset = (i-radius) + w1/(w0+w1)
	//  - normalize weights
	const double sigma_d = static_cast<double>(sigma);
	const int radius = static_cast<int>(std::ceil(sigma * 2.3f));
	const int inputCount = radius * 2 + 1;

	std::vector<double> inputW(static_cast<size_t>(inputCount));
	const double twoSigmaSquare = 2.0 * sigma_d * sigma_d;
	for (int i = 0; i < inputCount; ++i)
	{
		const int x = i - radius;
		inputW[static_cast<size_t>(i)] = std::exp(-(static_cast<double>(x) * static_cast<double>(x)) / twoSigmaSquare);
	}

	m_dwmHwSamples.radius = radius;
	double sum = 0.0;
	for (int hw = 0; hw < 8; ++hw)
	{
		const int i0 = hw * 2;
		const int i1 = i0 + 1;
		const double w0 = (i0 < inputCount) ? inputW[static_cast<size_t>(i0)] : 0.0;
		const double w1 = (i1 < inputCount) ? inputW[static_cast<size_t>(i1)] : 0.0;
		const double hwW = w0 + w1;
		const double frac = (hwW != 0.0) ? (w1 / hwW) : 0.0;
		const double hwOff = static_cast<double>(i0 - radius) + frac;
		m_dwmHwSamples.weights[static_cast<size_t>(hw)] = static_cast<float>(hwW);
		m_dwmHwSamples.offsets[static_cast<size_t>(hw)] = static_cast<float>(hwOff);
		sum += hwW;
	}
	if (sum != 0.0)
	{
		for (int i = 0; i < 8; ++i)
		{
			m_dwmHwSamples.weights[static_cast<size_t>(i)] = static_cast<float>(static_cast<double>(m_dwmHwSamples.weights[static_cast<size_t>(i)]) / sum);
		}
	}
}

HRESULT CD3DGlassRealizer::Render(
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
)
{
	BOOL hwProtectionEnabled = FALSE;
	D3D11_TEXTURE2D_DESC backBufferDesc{};
	RETURN_IF_FAILED(
		Util::GetDescAndHwProtectionStateFromTexture(
			backBuffer,
			backBufferDesc,
			hwProtectionEnabled
		)
	);

	if (!m_initialized)
	{
		RETURN_IF_FAILED(Initialize(device));
	}

	RETURN_IF_FAILED(
		m_quarterResBuffer.Ensure(
			device,
			backBufferDesc.Width >> 2,
			backBufferDesc.Height,
			backBufferDesc,
			D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET,
			hwProtectionEnabled
		)
	);
	RETURN_IF_FAILED(m_quarterResBuffer.EnsureRTV(device));
	RETURN_IF_FAILED(m_quarterResBuffer.EnsureSRV(device));

	CalculateDwmHwSamples(3.f);
	// DWM inflates bounds by filterRadius = ceil(sigma * 2.3) (sigma=3.0 => 7)
	const float expansion = static_cast<float>(m_dwmHwSamples.radius);
	const UINT quarterWidth = std::max<UINT>(1u, backBufferDesc.Width >> 2);

	std::vector<BlurVertex> verticesBlur{};
	std::vector<BlurVertex> verticesCompose{};
	std::vector<WORD> indicesCompose{};
	RETURN_IF_FAILED(
		Tessellate(
			geometry,
			verticesCompose,
			indicesCompose,
			BlurAxis::Vertical,
			1.f,
			1.f,
			0.25f,
			1.f,
			static_cast<float>(quarterWidth),
			static_cast<float>(backBufferDesc.Height),
			0.0f
		)
	);

	// Pass 1 (accumulation stage):
	D2D1_RECT_F samplingWorldBounds =
	{
		drawingWorldBounds.left - expansion,
		drawingWorldBounds.top - expansion,
		drawingWorldBounds.right + expansion,
		drawingWorldBounds.bottom + expansion
	};
	D2D1_RECT_F geometryBounds{};
	RETURN_IF_FAILED(geometry->GetBounds(nullptr, &geometryBounds));
	SubmitBlurRectBatch(rectangles, drawingWorldBounds, geometryBounds, expansion, backBufferDesc.Width, backBufferDesc.Height, verticesBlur);

	if (verticesCompose.empty() || indicesCompose.empty() || verticesBlur.empty())
	{
		return S_OK;
	}

	// Ensure buffer capacities (pass 1 blur is non-indexed; compose uses verticesCompose/indicesCompose)
	RETURN_IF_FAILED(EnsureVertexBufferCapacity(device, verticesBlur.size(), m_vertexBuffer, m_vertexCapacityBlur));
	RETURN_IF_FAILED(EnsureMeshBuffersCapacity(device, verticesCompose.size(), indicesCompose.size(), m_vertexBufferCompose, m_indexBufferCompose, m_vertexCapacityCompose, m_indexCapacityCompose));

	Util::D3D11ContextStateGuard contextStateGuard(context);

	{
		winrt::com_ptr<ID3D11DeviceContext3> context3{};
		RETURN_IF_FAILED(context->QueryInterface(context3.put()));
		context3->SetHardwareProtectionState(static_cast<BOOL>(hwProtectionEnabled));
	}

	UINT stride{ sizeof(BlurVertex) };
	UINT offset{ 0 };
	context->IASetInputLayout(m_vertexLayout.get());
	context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	//----------------------------------------------------------------------------------
	// Update PS Constant Buffer (Win7 DWM style layout)
	//----------------------------------------------------------------------------------
	CConstantBufferPS cbPS{};
	for (int i = 0; i < 8; ++i)
	{
		const float w = m_dwmHwSamples.weights[static_cast<size_t>(i)];
		cbPS.conv[i].x = w;
		cbPS.conv[i].y = w;
		cbPS.conv[i].z = w;
	}
	if (type == Shared::GlassType::Aero)
	{
		cbPS.fallback.x = params.fallback.r;
		cbPS.fallback.y = params.fallback.g;
		cbPS.fallback.z = params.fallback.b;
		cbPS.fallback.w = params.fallback.a;
		cbPS.color.x = params.color.r;
		cbPS.color.y = params.color.g;
		cbPS.color.z = params.color.b;
		cbPS.color.w = params.color.a;
		cbPS.afterglow.x = params.afterglow.r;
		cbPS.afterglow.y = params.afterglow.g;
		cbPS.afterglow.z = params.afterglow.b;
		cbPS.afterglow.w = params.afterglow.a;
		cbPS.blurBalance.x = params.blurBalance;
		cbPS.blurBalance.y = 0.f;
		cbPS.blurBalance.z = 0.f;
		cbPS.blurBalance.w = 0.f;
	}
	else
	{
		cbPS.color.x = params.color.r;
		cbPS.color.y = params.color.g;
		cbPS.color.z = params.color.b;
		cbPS.color.w = params.color.a;
	}
	if (memcmp(&m_cbPS, &cbPS, sizeof(cbPS)) != 0)
	{
		m_cbPS = cbPS;
		context->UpdateSubresource(m_constantBufferPS.get(), 0, nullptr, &cbPS, 0, 0);
	}

	D3D11_MAPPED_SUBRESOURCE mappedResource;

	RETURN_IF_FAILED(context->Map(m_vertexBuffer.get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource));
	memcpy(mappedResource.pData, verticesBlur.data(), sizeof(BlurVertex) * verticesBlur.size());
	context->Unmap(m_vertexBuffer.get(), 0);

	// Pass 1 blur is non-indexed (DWM style), so we don't upload indices here.
	RETURN_IF_FAILED(context->Map(m_vertexBufferCompose.get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource));
	memcpy(mappedResource.pData, verticesCompose.data(), sizeof(BlurVertex) * verticesCompose.size());
	context->Unmap(m_vertexBufferCompose.get(), 0);

	RETURN_IF_FAILED(context->Map(m_indexBufferCompose.get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource));
	memcpy(mappedResource.pData, indicesCompose.data(), sizeof(WORD) * indicesCompose.size());
	context->Unmap(m_indexBufferCompose.get(), 0);

	D3D11_VIEWPORT vp{};
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	vp.TopLeftX = 0;
	vp.TopLeftY = 0;

	D3D11_RECT clipRect;
	context->OMSetBlendState(m_blendStateOpaque.get(), nullptr, 0xffffffff);
	context->VSSetShader(m_vertexShader.get(), nullptr, 0);
	ID3D11Buffer* vsCBs[] = { m_constantBufferVS.get() };
	ID3D11Buffer* psCBs[] = { m_constantBufferPS.get() };
	context->VSSetConstantBuffers(0, 1, vsCBs);
	context->PSSetConstantBuffers(0, 1, psCBs);
	ID3D11SamplerState* samplers[] = { m_samplerLinearClamp.get() };
	context->PSSetSamplers(0, 1, samplers);
	context->RSSetState(m_rasterizerStateScissor.get());

	// If the caller can't provide an SRV for the back buffer, create an intermediate SRV-capable texture
	// and copy only the needed blur region into it. We keep coordinates identical (copy into same offset)
	// so UVs remain unchanged.
	ID3D11ShaderResourceView* pass1InputSRV = backBufferSRV;
	if (!pass1InputSRV)
	{
		RETURN_IF_FAILED(
			m_intermediateBuffer.Ensure(
				device,
				backBufferDesc.Width,
				backBufferDesc.Height,
				backBufferDesc,
				D3D11_BIND_SHADER_RESOURCE,
				hwProtectionEnabled
			)
		);
		RETURN_IF_FAILED(m_intermediateBuffer.EnsureSRV(device));

		// Avoid resource binding hazards (and we will set RTVs explicitly afterwards anyway).
		context->OMSetRenderTargets(0, nullptr, nullptr);
		if (rectangles.size() <= 12)
		{
			for (auto subRectangle : rectangles)
			{
				if (RectF::IntersectUnsafe(subRectangle, drawingWorldBounds))
				{
					subRectangle =
					{
						std::max(subRectangle.left - expansion, 0.f),
						std::max(subRectangle.top - expansion, 0.f),
						std::min(subRectangle.right + expansion, static_cast<float>(backBufferDesc.Width)),
						std::min(subRectangle.bottom + expansion, static_cast<float>(backBufferDesc.Height))
					};

					const auto copyRegionRect = RectF::ToRectU(subRectangle);
					Util::CopyTextureRegion(
						context,
						backBuffer,
						m_intermediateBuffer.m_texture.get(),
						copyRegionRect,
						copyRegionRect.left,
						copyRegionRect.top
					);
				}
			}
		}
		else
		{
			const auto copyRegionRect = RectF::ToRectU(samplingWorldBounds);
			Util::CopyTextureRegion(
				context,
				backBuffer,
				m_intermediateBuffer.m_texture.get(),
				copyRegionRect,
				copyRegionRect.left,
				copyRegionRect.top
			);
		}

		pass1InputSRV = m_intermediateBuffer.m_srv.get();
	}
	else
	{
		m_intermediateBuffer.Reset();
	}

	//----------------------------------------------------------------------------------
	// Pass 1: horizontal blur into quarter-width buffer
	//----------------------------------------------------------------------------------
	ID3D11Buffer* vbBlur[] = { m_vertexBuffer.get() };
	context->IASetVertexBuffers(0, 1, vbBlur, &stride, &offset);
	context->IASetIndexBuffer(nullptr, DXGI_FORMAT_UNKNOWN, 0);
	ID3D11RenderTargetView* rtvQuarter[] = { m_quarterResBuffer.m_rtv.get() };
	context->OMSetRenderTargets(1, rtvQuarter, nullptr);

	vp.Width = static_cast<float>(quarterWidth);
	vp.Height = static_cast<float>(backBufferDesc.Height);
	context->RSSetViewports(1, &vp);

	// VS constants must match the current render target pixel space
	CConstantBufferVS cbVS{};
	cbVS.viewportSize.x = vp.Width;
	cbVS.viewportSize.y = vp.Height;
	if (memcmp(&m_cbVS, &cbVS, sizeof(cbVS)) != 0)
	{
		m_cbVS = cbVS;
		context->UpdateSubresource(m_constantBufferVS.get(), 0, nullptr, &cbVS, 0, 0);
	}

	context->PSSetShader(m_pixelShaderBlurH.get(), nullptr, 0);
	ID3D11ShaderResourceView* srvsBack[] = { pass1InputSRV };
	context->PSSetShaderResources(0, 1, srvsBack);

	// Tighten scissor to reduce pass 1 fill cost.
	clipRect = ToScissorRect(
		D2D1::RectF(
			samplingWorldBounds.left * 0.25f,
			samplingWorldBounds.top,
			samplingWorldBounds.right * 0.25f,
			samplingWorldBounds.bottom
		),
		static_cast<LONG>(quarterWidth),
		static_cast<LONG>(backBufferDesc.Height)
	);

	context->RSSetScissorRects(1, &clipRect);
	context->Draw(static_cast<UINT>(verticesBlur.size()), 0);
	{ ID3D11ShaderResourceView* nullSRV[1] = { nullptr }; context->PSSetShaderResources(0, 1, nullSRV); }

	//----------------------------------------------------------------------------------
	// Pass 2: vertical blur + colorization, write into final back buffer
	//----------------------------------------------------------------------------------
	ID3D11Buffer* vbCompose[] = { m_vertexBufferCompose.get() };
	context->IASetVertexBuffers(0, 1, vbCompose, &stride, &offset);
	context->IASetIndexBuffer(m_indexBufferCompose.get(), DXGI_FORMAT_R16_UINT, 0);
	ID3D11RenderTargetView* rtvBack[] = { backBufferRTV };
	context->OMSetRenderTargets(1, rtvBack, nullptr);

	vp.Width = static_cast<float>(backBufferDesc.Width);
	vp.Height = static_cast<float>(backBufferDesc.Height);
	context->RSSetViewports(1, &vp);
	cbVS = {};
	cbVS.viewportSize.x = vp.Width;
	cbVS.viewportSize.y = vp.Height;
	if (memcmp(&m_cbVS, &cbVS, sizeof(cbVS)) != 0)
	{
		m_cbVS = cbVS;
		context->UpdateSubresource(m_constantBufferVS.get(), 0, nullptr, &cbVS, 0, 0);
	}

	if (type == Shared::GlassType::Aero)
	{
		context->PSSetShader(m_pixelShaderBlurV.get(), nullptr, 0);
	}
	else
	{
		context->PSSetShader(m_pixelShaderBlurV_Vista.get(), nullptr, 0);
	}
	ID3D11ShaderResourceView* srvsQuarter[] = { m_quarterResBuffer.m_srv.get() };
	context->PSSetShaderResources(0, 1, srvsQuarter);

	clipRect = ToScissorRect(
		D2D1::RectF(
			drawingWorldBounds.left,
			drawingWorldBounds.top,
			drawingWorldBounds.right,
			drawingWorldBounds.bottom
		),
		static_cast<LONG>(backBufferDesc.Width),
		static_cast<LONG>(backBufferDesc.Height)
	);
	context->RSSetScissorRects(1, &clipRect);
	context->DrawIndexed(static_cast<UINT>(indicesCompose.size()), 0, 0);
	{ ID3D11ShaderResourceView* nullSRV[1] = { nullptr }; context->PSSetShaderResources(0, 1, nullSRV); }
	context->Flush();

	return S_OK;
}

float CD3DGlassRealizer::GetBlurRadius(float blurAmount)
{
	blurAmount = 3.f;
	return std::ceil(blurAmount * 2.3f + 0.5f);
}
