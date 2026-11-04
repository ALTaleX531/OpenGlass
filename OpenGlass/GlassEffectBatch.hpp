#pragma once
#include "Buffer2D.hpp"
#include "AeroEffect.hpp"
#include "Shared.hpp"
#include <span>
#include <vector>

namespace OpenGlass
{
	// Prepare input in the composition buffer, blur horizontally into a second buffer,
	// then reuse the composition buffer for vertical blur/colorization and final composition.
	// Owns scheduling resources; CustomBlurEffect owns the blur math.
	class CGlassEffectBatch
	{
		struct KernelInputMapping
		{
			// Input bounds for a unit output pixel and the per-pixel input mapping.
			D2D1_RECT_F m_unitInputBounds{};
			D2D1_POINT_2F m_inputScale{};

			HRESULT Update(ID2D1DeviceContext* stageContext, ID2D1Effect* blurKernel);
			D2D1_RECT_F Map(const D2D1_RECT_F& outputBounds) const;
		};
		struct ScratchBuffer
		{
			CBuffer2D m_buffer;
			winrt::com_ptr<ID2D1Effect> m_translationEffect;
			D2D1_POINT_2F m_imageOrigin{};

			static UINT GrowCapacity(UINT currentCapacity, UINT requiredSize, UINT targetSize);
			HRESULT Ensure(
				ID2D1DeviceContext* stageContext,
				ID3D11Device* device,
				const D3D11_TEXTURE2D_DESC& targetDesc,
				bool hwProtectionEnabled,
				D2D1_PIXEL_FORMAT pixelFormat,
				D2D1_SIZE_U requiredSize
			);
			HRESULT GetView(ID2D1DeviceContext* stageContext, winrt::com_ptr<ID2D1Image>& imageView);
		};

		winrt::com_ptr<ID2D1DeviceContext> m_stageContext;
		winrt::com_ptr<ID2D1Effect> m_horizontalBlurEffect;
		winrt::com_ptr<ID2D1Effect> m_verticalBlurEffect;
		winrt::com_ptr<ID2D1Image> m_preparedInputImage;
		KernelInputMapping m_horizontalInputMapping;
		KernelInputMapping m_verticalInputMapping;
		float m_cachedBlurAmount{ -1.f };
		D2D1_GAUSSIANBLUR_OPTIMIZATION m_cachedBlurOptimization{};
		D2D1_SIZE_F m_cachedOutputScale{};
		ScratchBuffer m_compositionBuffer;
		ScratchBuffer m_horizontalBlurBuffer;
		D2D1_SIZE_U m_compositionRequiredSize{};
		D2D1_SIZE_U m_horizontalBlurRequiredSize{};
		winrt::com_ptr<ID2D1BitmapBrush1> m_compositionBrush;
		std::vector<D2D1_RECT_F> m_worldBands;
		std::vector<D2D1_RECT_F> m_effectOutputRanges;
		std::vector<D2D1_RECT_F> m_horizontalBlurRanges;
		std::vector<D2D1_RECT_F> m_preparedInputRanges;

		HRESULT Bind(ID2D1Image* effectOutputImage, Shared::GlassType glassType, float tintAlpha);
		void Plan(const D2D1_MATRIX_3X2_F& worldToEffectTransform, D2D1_POINT_2F effectOutputOffset);
		static D2D1_RECT_F ToKernelOutput(const D2D1_RECT_F& effectBounds, D2D1_POINT_2F effectOutputOffset);
		void DrawRanges(ID2D1Image* sourceImage, std::span<const D2D1_RECT_F> sourceRanges, D2D1_POINT_2F destinationOffset);
		HRESULT DrawStages(ID2D1DeviceContext* destinationContext, ID2D1Bitmap1* destinationBitmap, ID2D1Image* effectOutputImage);
		void RestoreGraph();
	public:
		void ResetGraph();
		bool PlanBands(const D2D1_RECT_F& drawingWorldBounds, std::span<const D2D1_RECT_F> drawingRectangles);
		HRESULT Render(
			ID2D1DeviceContext* destinationContext,
			ID2D1Bitmap1* destinationBitmap,
			ID3D11Device* device,
			const D3D11_TEXTURE2D_DESC& targetDesc,
			bool hwProtectionEnabled,
			IRenderingEffect* glassEffect,
			const CAeroParams& aeroParams,
			Shared::GlassType glassType,
			const D2D1_RECT_F& drawingWorldBounds,
			std::span<const D2D1_RECT_F> drawingRectangles
		);
	};
}
