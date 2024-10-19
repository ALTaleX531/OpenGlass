#include "pch.h"
#include "Utils.hpp"
#include "uDwmProjection.hpp"
#include "GlassFramework.hpp"
#include "GlassEffect.hpp"
#include "AeroEffect.hpp"
#include "BlurEffect.hpp"
#include "ReflectionEffect.hpp"
#include "Shared.hpp"

using namespace OpenGlass;
namespace OpenGlass::GlassEffectFactory
{
	std::unordered_map<dwmcore::CGeometry*, winrt::com_ptr<IGlassEffect>> g_glassEffectMap{};

	class CGlassEffect : public winrt::implements<CGlassEffect, IGlassEffect>
	{
		D2D1_COLOR_F m_color{};
		D2D1_COLOR_F m_afterglow{};
		float m_glassOpacity{ 0.63f };
		float m_blurAmount{ 9.f };
		float m_colorBalance{ 0.f };
		float m_afterglowBalance{ 0.f };
		float m_blurBalance{ 0.f };
		Shared::Type m_type{ Shared::Type::Blur };
		PVOID m_originalVTable{ nullptr };

		D2D1_SIZE_F m_canvasSize{};
		D2D1_POINT_2F m_glassOffset{};
		std::unordered_map<ID2D1Bitmap1*, winrt::com_ptr<ID2D1Bitmap1>> m_inputMap{};
		std::variant<std::nullptr_t, winrt::com_ptr<IBlurEffect>, winrt::com_ptr<IAeroEffect>> m_customEffect{ nullptr };
	public:
		void STDMETHODCALLTYPE SetGlassRenderingParameters(
			const D2D1_COLOR_F& color,
			const D2D1_COLOR_F& afterglow,
			float glassOpacity,
			float blurAmount,
			float colorBalance,
			float afterglowBalance,
			float blurBalance,
			Shared::Type type
		) override;
		HRESULT STDMETHODCALLTYPE Render(
			ID2D1DeviceContext* context,
			ID2D1Geometry* geometry,
			const D2D1_RECT_F& clipWorldBounds,
			bool normalDesktopRender
		) override;
	};
}

void STDMETHODCALLTYPE GlassEffectFactory::CGlassEffect::SetGlassRenderingParameters(
	const D2D1_COLOR_F& color,
	const D2D1_COLOR_F& afterglow,
	float glassOpacity,
	float blurAmount,
	float colorBalance,
	float afterglowBalance,
	float blurBalance,
	Shared::Type type
)
{
	m_color = color;
	m_afterglow = afterglow;
	m_glassOpacity = glassOpacity;
	m_blurAmount = blurAmount;
	m_colorBalance = colorBalance;
	m_afterglowBalance = afterglowBalance;
	m_blurBalance = blurBalance;
	if (m_type != type)
	{
		m_type = type;
		m_customEffect = nullptr;
	}
}

HRESULT STDMETHODCALLTYPE GlassEffectFactory::CGlassEffect::Render(
	ID2D1DeviceContext* context,
	ID2D1Geometry* geometry,
	const D2D1_RECT_F& clipWorldBounds,
	bool normalDesktopRender
)
{
	winrt::com_ptr<ID2D1Bitmap1> targetBitmap{ nullptr };
	{
		winrt::com_ptr<ID2D1Image> image{ nullptr };
		context->GetTarget(image.put());
		if (!image)
		{
			return D2DERR_BITMAP_CANNOT_DRAW;
		}
		else
		{
			RETURN_IF_FAILED(image->QueryInterface(targetBitmap.put()));
		}
	}
	if (!m_originalVTable)
	{
		m_originalVTable = *reinterpret_cast<PVOID*>(targetBitmap.get());
	}
	auto pixelFormat = targetBitmap->GetPixelFormat();

	D2D1_MATRIX_3X2_F matrix{};
	context->GetTransform(&matrix);
	D2D1_RECT_F geometryWorldBounds{};
	RETURN_IF_FAILED(geometry->GetBounds(&matrix, &geometryWorldBounds));

	winrt::com_ptr<ID2D1Bitmap1> inputBitmap{ nullptr };
	for (auto it = m_inputMap.begin(); it != m_inputMap.end(); )
	{
		if (targetBitmap.get() == it->first)
		{
			inputBitmap = it->second;
		}
		if (IsBadReadPtr(it->first, sizeof(PVOID*)) || *reinterpret_cast<PVOID*>(it->first) != m_originalVTable)
		{
#ifdef _DEBUG
			OutputDebugStringW(std::format(L"target bitmap: [{}] released, release corresponding input bitmap: [{}].\n", (void*)it->first, (void*)it->second.get()).c_str());
#endif // _DEBUG
			it = m_inputMap.erase(it);
		}
		else
		{
#ifdef _DEBUG
			auto ref = it->first->AddRef();
			OutputDebugStringW(
				std::format(
					L"target bitmap: [{}] current ref count: {}.\n",
					(void*)it->first,
					ref
				).c_str()
			);
			it->first->Release();
#endif // _DEBUG
			it++;
		}
	}
#ifdef _DEBUG
	OutputDebugStringW(std::format(L"inputMap count: {}\n", m_inputMap.size()).c_str());
#endif // _DEBUG

	if (std::holds_alternative<std::nullptr_t>(m_customEffect))
	{
		switch (m_type)
		{
		case Shared::Type::Blur:
		{
			m_customEffect = winrt::make<CBlurEffect>();
			break;
		}
		case Shared::Type::Aero:
		{
			m_customEffect = winrt::make<CAeroEffect>();
			break;
		}
		default:
			break;
		}
	}
	// size changed
	if (inputBitmap)
	{
		if (
			D2D1_SIZE_F size{ inputBitmap->GetSize() }, actualSize{ geometryWorldBounds.right - geometryWorldBounds.left, geometryWorldBounds.bottom - geometryWorldBounds.top };
			size.width != actualSize.width ||
			size.height != actualSize.height
		)
		{
			inputBitmap = nullptr;
		}
	}
	if (!inputBitmap)
	{
		RETURN_IF_FAILED(
			context->CreateBitmap(
				D2D1::SizeU(
					dwmcore::PixelAlign(geometryWorldBounds.right - geometryWorldBounds.left),
					dwmcore::PixelAlign(geometryWorldBounds.bottom - geometryWorldBounds.top)
				),
				nullptr,
				0,
				D2D1::BitmapProperties1(
					D2D1_BITMAP_OPTIONS_NONE,
					pixelFormat
				),
				inputBitmap.put()
			)
		);
		m_inputMap.insert_or_assign(targetBitmap.get(), inputBitmap);

#ifdef _DEBUG
		OutputDebugStringW(std::format(L"allocated input bitmap: [{}] for target bitmap: [{}].\n", (void*)inputBitmap.get(), (void*)targetBitmap.get()).c_str());
#endif // _DEBUG

		switch (m_type)
		{
		case Shared::Type::Blur:
		{
			std::get<winrt::com_ptr<IBlurEffect>>(m_customEffect)->Reset();
			break;
		}
		case Shared::Type::Aero:
		{
			std::get<winrt::com_ptr<IAeroEffect>>(m_customEffect)->Reset();
			break;
		}
		default:
			break;
		}
	}

	D2D1_RECT_F drawingWorldBounds
	{
		max(clipWorldBounds.left, geometryWorldBounds.left),
		max(clipWorldBounds.top, geometryWorldBounds.top),
		min(clipWorldBounds.right, geometryWorldBounds.right),
		min(clipWorldBounds.bottom, geometryWorldBounds.bottom)
	};
	D2D1_RECT_F dirtyRectangle
	{
		drawingWorldBounds.left - geometryWorldBounds.left,
		drawingWorldBounds.top - geometryWorldBounds.top
	};
	dirtyRectangle.right = dirtyRectangle.left + (drawingWorldBounds.right - drawingWorldBounds.left);
	dirtyRectangle.bottom = dirtyRectangle.top + (drawingWorldBounds.bottom - drawingWorldBounds.top);
	D2D1_POINT_2U dest
	{
		dwmcore::PixelAlign(dirtyRectangle.left),
		dwmcore::PixelAlign(dirtyRectangle.top)
	};
	D2D1_RECT_U src
	{
		dwmcore::PixelAlign(drawingWorldBounds.left),
		dwmcore::PixelAlign(drawingWorldBounds.top),
		dwmcore::PixelAlign(drawingWorldBounds.right),
		dwmcore::PixelAlign(drawingWorldBounds.bottom)
	};
	RETURN_IF_FAILED(
		inputBitmap->CopyFromBitmap(
			&dest,
			targetBitmap.get(),
			&src
		)
	);

	auto imageBounds = D2D1::RectF(
		0.f,
		0.f,
		geometryWorldBounds.right - geometryWorldBounds.left,
		geometryWorldBounds.bottom - geometryWorldBounds.top
	);
	switch (m_type)
	{
	case Shared::Type::Blur:
	{
		RETURN_IF_FAILED(
			std::get<winrt::com_ptr<IBlurEffect>>(m_customEffect)->SetInput(
				context,
				inputBitmap.get(),
				dirtyRectangle,
				imageBounds,
				m_blurAmount,
				m_color,
				m_glassOpacity
			)
		);
		break;
	}
	case Shared::Type::Aero:
	{
		RETURN_IF_FAILED(
			std::get<winrt::com_ptr<IAeroEffect>>(m_customEffect)->SetInput(
				context,
				inputBitmap.get(),
				dirtyRectangle,
				imageBounds,
				m_blurAmount,
				m_color,
				m_afterglow,
				m_colorBalance,
				m_afterglowBalance,
				m_blurBalance
			)
		);
		break;
	}
	default:
		break;
	}

	context->SetTransform(D2D1::IdentityMatrix());
	context->PushLayer(
		D2D1::LayerParameters1(
			drawingWorldBounds,
			geometry,
			D2D1_ANTIALIAS_MODE_ALIASED,
			matrix,
			1.f,
			nullptr,
			D2D1_LAYER_OPTIONS1_INITIALIZE_FROM_BACKGROUND |
			(
				pixelFormat.alphaMode == D2D1_ALPHA_MODE_IGNORE ?
				D2D1_LAYER_OPTIONS1_IGNORE_ALPHA :
				D2D1_LAYER_OPTIONS1_NONE
			)
		),
		nullptr
	);

	ID2D1Image* outputImage{ nullptr };
	switch (m_type)
	{
	case Shared::Type::Blur:
	{
		outputImage = std::get<winrt::com_ptr<IBlurEffect>>(m_customEffect)->GetOutput();
		break;
	}
	case Shared::Type::Aero:
	{
		outputImage = std::get<winrt::com_ptr<IAeroEffect>>(m_customEffect)->GetOutput();
		break;
	}
	default:
		break;
	}
	if (outputImage)
	{
		context->DrawImage(
			outputImage,
			D2D1::Point2F(
				drawingWorldBounds.left,
				drawingWorldBounds.top
			),
			D2D1::RectF(
				dirtyRectangle.left,
				dirtyRectangle.top,
				dirtyRectangle.right,
				dirtyRectangle.bottom
			),
			D2D1_INTERPOLATION_MODE_NEAREST_NEIGHBOR,
			D2D1_COMPOSITE_MODE_BOUNDED_SOURCE_COPY
		);
	}

	context->PopLayer();
	context->SetTransform(matrix);

	if (normalDesktopRender)
	{
		m_canvasSize = context->GetSize();
		m_glassOffset = D2D1::Point2F(matrix.dx, matrix.dy);
	}
	RETURN_IF_FAILED(
		ReflectionEffect::Render(
			context,
			geometry,
			Shared::g_reflectionIntensity,
			Shared::g_reflectionParallaxIntensity,
			&m_canvasSize,
			&m_glassOffset
		)
	);

	return S_OK;
}

winrt::com_ptr<IGlassEffect> GlassEffectFactory::GetOrCreate(dwmcore::CGeometry* geometry, bool createIfNecessary)
{
	auto it= g_glassEffectMap.find(geometry);

	if (createIfNecessary)
	{
		if (it == g_glassEffectMap.end())
		{
			auto result = g_glassEffectMap.emplace(geometry, winrt::make<CGlassEffect>());
			if (result.second == true)
			{
				it = result.first;
			}
		}
	}

	return it == g_glassEffectMap.end() ? nullptr : it->second;
}
void GlassEffectFactory::Remove(dwmcore::CGeometry* geometry)
{
	auto it = g_glassEffectMap.find(geometry);

	if (it != g_glassEffectMap.end())
	{
		g_glassEffectMap.erase(it);
	}
}
void GlassEffectFactory::Shutdown()
{
	g_glassEffectMap.clear();
}