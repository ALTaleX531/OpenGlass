#include "pch.h"
#include "Renderer.hpp"
#include "GlassRealizer.hpp"
#include "D2DPrivates.hpp"
#include "Util.hpp"

using namespace OpenGlass;
using namespace OpenGlass::RenderTest;

CAeroParams Settings::Parameters() const
{
	CAeroParams params{};
	params.blurAmount = blurAmount;
	params.optimization = optimization;
	params.color = { .08f, .16f, .24f, 1.f };
	params.afterglow = { .12f, .18f, .24f, 1.f };
	params.blurBalance = .65f;
	return params;
}

HRESULT GaussianEffect::Initialize(ID2D1DeviceContext* context)
{
	winrt::com_ptr<ID2D1Factory> factory;
	context->GetFactory(factory.put());
	auto effectFactory = factory.as<ID2D1Factory1>();
	RETURN_IF_FAILED(CAeroColorizationEffect::Register(effectFactory.get()));
	m_factory = std::move(effectFactory);
	RETURN_IF_FAILED(context->CreateEffect(CLSID_D2D1Scale, m_scaleEffect.put()));
	RETURN_IF_FAILED(context->CreateEffect(CLSID_D2D1Crop, m_cropEffect.put()));
	RETURN_IF_FAILED(context->CreateEffect(CLSID_D2D1Border, m_borderEffect.put()));
	RETURN_IF_FAILED(context->CreateEffect(CLSID_D2D1GaussianBlur, m_gaussianBlurEffect.put()));
	RETURN_IF_FAILED(context->CreateEffect(CLSID_AeroColorizationEffect, m_colorizationEffect.put()));
	RETURN_IF_FAILED(m_scaleEffect->SetValue(D2D1_SCALE_PROP_SCALE, D2D1::Vector2F(.5f, .5f)));
	RETURN_IF_FAILED(m_scaleEffect->SetValue(D2D1_SCALE_PROP_INTERPOLATION_MODE, D2D1_SCALE_INTERPOLATION_MODE_LINEAR));
	RETURN_IF_FAILED(m_scaleEffect->SetValue(D2D1_SCALE_PROP_BORDER_MODE, D2D1_BORDER_MODE_HARD));
	RETURN_IF_FAILED(m_cropEffect->SetValue(D2D1_CROP_PROP_BORDER_MODE, D2D1_BORDER_MODE_HARD));
	RETURN_IF_FAILED(m_borderEffect->SetValue(D2D1_BORDER_PROP_EDGE_MODE_X, D2D1_BORDER_EDGE_MODE_MIRROR));
	RETURN_IF_FAILED(m_borderEffect->SetValue(D2D1_BORDER_PROP_EDGE_MODE_Y, D2D1_BORDER_EDGE_MODE_MIRROR));
	RETURN_IF_FAILED(m_gaussianBlurEffect->SetValue(D2D1_GAUSSIANBLUR_PROP_BORDER_MODE, D2D1_BORDER_MODE_HARD));
	m_cropEffect->SetInputEffect(0, m_scaleEffect.get());
	m_borderEffect->SetInputEffect(0, m_cropEffect.get());
	m_gaussianBlurEffect->SetInputEffect(0, m_borderEffect.get());
	m_colorizationEffect->SetInputEffect(0, m_gaussianBlurEffect.get());
	return S_OK;
}

GaussianEffect::~GaussianEffect()
{
	if (m_factory)
	{
		LOG_IF_FAILED(CAeroColorizationEffect::UnRegister(m_factory.get()));
	}
}

HRESULT GaussianEffect::Build(ID2D1DeviceContext* context, ID2D1Image* inputImage,
	const D2D1_RECT_F& imageRectangle, const void* additionalParams)
{
	if (!m_colorizationEffect)
	{
		RETURN_IF_FAILED(Initialize(context));
	}
	const auto& params = *static_cast<const CAeroParams*>(additionalParams);
	m_scaleEffect->SetInput(0, inputImage);
	RETURN_IF_FAILED(m_cropEffect->SetValue(D2D1_CROP_PROP_RECT,
		D2D1::RectF(imageRectangle.left * .5f, imageRectangle.top * .5f, imageRectangle.right * .5f, imageRectangle.bottom * .5f)));
	RETURN_IF_FAILED(m_gaussianBlurEffect->SetValue(D2D1_GAUSSIANBLUR_PROP_STANDARD_DEVIATION, params.blurAmount));
	RETURN_IF_FAILED(m_gaussianBlurEffect->SetValue(D2D1_GAUSSIANBLUR_PROP_OPTIMIZATION, params.optimization));
	RETURN_IF_FAILED(m_colorizationEffect->SetValue(AEROCOLORIZATION_PROP_AFTERGLOW,
		D2D1::Vector4F(params.afterglow.r, params.afterglow.g, params.afterglow.b, params.afterglow.a)));
	RETURN_IF_FAILED(m_colorizationEffect->SetValue(AEROCOLORIZATION_PROP_BLURBALANCE, D2D1::Vector4F(params.blurBalance)));
	RETURN_IF_FAILED(m_colorizationEffect->SetValue(AEROCOLORIZATION_PROP_COLOR,
		D2D1::Vector4F(params.color.r, params.color.g, params.color.b, params.color.a)));
	RETURN_IF_FAILED(m_colorizationEffect->SetValue(AEROCOLORIZATION_PROP_FALLBACK,
		D2D1::Vector4F(params.fallback.r, params.fallback.g, params.fallback.b, params.fallback.a)));
	return S_OK;
}

void GaussianEffect::GetOutput(ID2D1Image** output) const
{
	m_colorizationEffect->GetOutput(output);
}

void GaussianEffect::Reset()
{
	if (m_scaleEffect)
	{
		m_scaleEffect->SetInput(0, nullptr);
	}
}

HRESULT RectRenderer::Render(ID2D1DeviceContext* context, ID2D1Geometry* geometry,
	const D2D1_RECT_F& bounds, std::span<const D2D1_RECT_F> rectangles, const CAeroParams& params,
	float sourceBlurScale)
{
	winrt::com_ptr<ID2D1Bitmap1> target;
	RETURN_IF_FAILED(Util::GetTargetBitmapFromD2DContext(context, target));
	D2D1_RECT_F geometryBounds{};
	RETURN_IF_FAILED(geometry->GetBounds(nullptr, &geometryBounds));
	const auto size = target->GetSize();
	const float expansion = CGlassRealizer::GetBlurExpansion(params.blurAmount * sourceBlurScale);
	auto sampling = D2D1::RectF(std::max(bounds.left - expansion, 0.f), std::max(bounds.top - expansion, 0.f),
		std::min(bounds.right + expansion, size.width), std::min(bounds.bottom + expansion, size.height));
	RectF::IntersectUnsafe(sampling, geometryBounds);

	winrt::com_ptr<ID2D1Bitmap1> shared;
	winrt::com_ptr<ID2D1PrivateCompositorDeviceContext> compositor;
	const auto properties = D2D1::BitmapProperties1(target->GetOptions(), target->GetPixelFormat());
	if (rectangles.size() == 1 && !(properties.bitmapOptions & D2D1_BITMAP_OPTIONS_CANNOT_DRAW) &&
		SUCCEEDED(context->QueryInterface(compositor.put())))
	{
		RETURN_IF_FAILED(compositor->CreateSharedAtlasBitmap(target.get(), &properties, shared.put()));
	}
	if (!shared)
	{
		winrt::com_ptr<ID3D11Texture2D> texture;
		RETURN_IF_FAILED(Util::GetTextureFromD2DBitmap(target.get(), texture));
		D3D11_TEXTURE2D_DESC desc{};
		BOOL protectedTarget{};
		RETURN_IF_FAILED(Util::GetDescAndHwProtectionStateFromTexture(texture.get(), desc, protectedTarget));
		winrt::com_ptr<ID3D11Device> device;
		texture->GetDevice(device.put());
		winrt::com_ptr<ID3D11DeviceContext> immediate;
		device->GetImmediateContext(immediate.put());
		RETURN_IF_FAILED(m_capture.Ensure(device.get(), desc.Width, desc.Height, desc,
			D3D11_BIND_SHADER_RESOURCE, protectedTarget));
		RETURN_IF_FAILED(m_capture.EnsureD2D(context,
			D2D1::BitmapProperties1(D2D1_BITMAP_OPTIONS_NONE, target->GetPixelFormat())));
		// Preserve the old <= 12 policy and post-copy Flush for this baseline.
		if (rectangles.size() <= 12)
		{
			for (auto rect : rectangles)
			{
				if (RectF::IntersectUnsafe(rect, bounds))
				{
					rect = { std::max(rect.left - expansion, 0.f), std::max(rect.top - expansion, 0.f),
						std::min(rect.right + expansion, size.width), std::min(rect.bottom + expansion, size.height) };
					RectF::IntersectUnsafe(rect, geometryBounds);
					const auto source = RectF::ToRectU(rect);
					Util::CopyTextureRegion(immediate.get(), texture.get(), m_capture.m_texture.get(), source, source.left, source.top);
				}
			}
		}
		else
		{
			const auto source = RectF::ToRectU(sampling);
			Util::CopyTextureRegion(immediate.get(), texture.get(), m_capture.m_texture.get(), source, source.left, source.top);
		}
		immediate->Flush();
	}
	const auto reset = wil::scope_exit([&] { m_effect->Reset(); });
	RETURN_IF_FAILED(m_effect->Build(context, shared ? shared.get() : m_capture.m_bitmap.get(), sampling, &params));
	winrt::com_ptr<ID2D1Image> output;
	m_effect->GetOutput(output.put());
	auto matrix = m_effect->GetOutputMatrix();
	auto inverse = matrix;
	D2D1InvertMatrix(&inverse);
	const auto offset = m_effect->GetOutputOffset();
	D2D1_MATRIX_3X2_F original;
	context->GetTransform(&original);
	D2D1_RENDERING_CONTROLS controls;
	context->GetRenderingControls(&controls);
	const auto restore = wil::scope_exit([&]
	{
		context->SetTransform(original);
		context->SetRenderingControls(controls);
	});
	const auto pixels = target->GetPixelSize();
	if (controls.tileSize.width < pixels.width || controls.tileSize.height < pixels.height)
	{
		auto enlarged = controls;
		enlarged.tileSize = pixels;
		context->SetRenderingControls(enlarged);
	}
	context->SetTransform(matrix);
	for (auto rect : rectangles)
	{
		if (RectF::IntersectUnsafe(rect, bounds))
		{
			const auto source = RectF::TransformRect(rect, inverse);
			context->DrawImage(output.get(), D2D1::Point2F(source.left, source.top),
				D2D1::RectF(source.left + offset.x, source.top + offset.y, source.right + offset.x, source.bottom + offset.y),
				D2D1_INTERPOLATION_MODE_LINEAR, D2D1_COMPOSITE_MODE_BOUNDED_SOURCE_COPY);
		}
	}
	return S_OK;
}
