#include "pch.h"
#include "Shared.hpp"
#include "uDWMProjection.hpp"
#include "MaterialRealizer.hpp"
#include "PngAssetValidation.hpp"

using namespace OpenGlass;

HRESULT CMaterialRealizer::LoadEffect(ID2D1DeviceContext* context)
{
	winrt::com_ptr<IStream> stream{ nullptr };
	std::optional<PngAssetValidation::ImageInfo> expectedInfo;
	if (
		Shared::g_materialTexturePath.empty() ||
		PathIsRelativeW(Shared::g_materialTexturePath.data()) ||
		PathIsNetworkPathW(Shared::g_materialTexturePath.data()) ||
		!PathFileExistsW(Shared::g_materialTexturePath.data())
	)
	{
		wil::unique_hmodule wucModule
		{
			LoadLibraryExW(
				L"Windows.UI.Xaml.Controls.dll",
				nullptr,
				LOAD_LIBRARY_AS_IMAGE_RESOURCE | LOAD_LIBRARY_SEARCH_SYSTEM32
			)
		};

		std::span<const UCHAR> materialBytes{};
		RETURN_IF_FAILED(Util::GetResDataView(materialBytes, 2000, wucModule.get()));

		stream = { SHCreateMemStream(materialBytes.data(), static_cast<UINT>(materialBytes.size_bytes())), winrt::take_ownership_from_abi};
	}
	else
	{
		wil::unique_hfile file{ CreateFileW(Shared::g_materialTexturePath.data(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, 0) };
		RETURN_LAST_ERROR_IF(!file.is_valid());

		LARGE_INTEGER fileSize{};
		RETURN_IF_WIN32_BOOL_FALSE(GetFileSizeEx(file.get(), &fileSize));
		RETURN_HR_IF(HRESULT_FROM_WIN32(ERROR_FILE_TOO_LARGE), fileSize.QuadPart <= 0 || fileSize.QuadPart > PngAssetValidation::MaximumFileSize);

		auto buffer{ std::make_unique<BYTE[]>(static_cast<size_t>(fileSize.QuadPart)) };
		DWORD bytesRead{};
		RETURN_IF_WIN32_BOOL_FALSE(ReadFile(file.get(), buffer.get(), static_cast<DWORD>(fileSize.QuadPart), &bytesRead, nullptr));
		RETURN_HR_IF(HRESULT_FROM_WIN32(ERROR_HANDLE_EOF), bytesRead != static_cast<DWORD>(fileSize.QuadPart));
		expectedInfo.emplace();
		RETURN_IF_FAILED(PngAssetValidation::ValidateStructure(
			{ reinterpret_cast<const std::byte*>(buffer.get()), static_cast<std::size_t>(fileSize.QuadPart) },
			*expectedInfo
		));
		stream = { SHCreateMemStream(buffer.get(), static_cast<UINT>(fileSize.QuadPart)), winrt::take_ownership_from_abi };
	}
	RETURN_HR_IF_NULL(E_OUTOFMEMORY, stream);

	winrt::com_ptr<IWICImagingFactory> wicFactory{ nullptr };
	wicFactory.copy_from(uDWM::CDesktopManager::GetInstance()->GetWICFactory());
	winrt::com_ptr<IWICFormatConverter> wicConverter{ nullptr };
	RETURN_IF_FAILED(PngAssetValidation::CreateValidatedWicSource(
		wicFactory.get(),
		stream.get(),
		expectedInfo ? &*expectedInfo : nullptr,
		wicConverter.put()
	));

	winrt::com_ptr<ID2D1Effect> bitmapSourceEffect{ nullptr };
	RETURN_IF_FAILED(
		context->CreateEffect(
			CLSID_D2D1BitmapSource,
			bitmapSourceEffect.put()
		)
	);
	RETURN_IF_FAILED(
		bitmapSourceEffect->SetValue(
			D2D1_BITMAPSOURCE_PROP_ALPHA_MODE,
			D2D1_ALPHA_MODE_PREMULTIPLIED
		)
	);
	RETURN_IF_FAILED(
		bitmapSourceEffect->SetValue(
			D2D1_PROPERTY_CACHED,
			TRUE
		)
	);
	RETURN_IF_FAILED(
		bitmapSourceEffect->SetValue(
			D2D1_BITMAPSOURCE_PROP_WIC_BITMAP_SOURCE,
			wicConverter.get()
		)
	);

	winrt::com_ptr<ID2D1Effect> tileEffect{ nullptr };
	RETURN_IF_FAILED(
		context->CreateEffect(
			CLSID_D2D1Border,
			tileEffect.put()
		)
	);
	RETURN_IF_FAILED(
		tileEffect->SetValue(
			D2D1_BORDER_PROP_EDGE_MODE_X,
			D2D1_BORDER_EDGE_MODE_WRAP
		)
	);
	RETURN_IF_FAILED(
		tileEffect->SetValue(
			D2D1_BORDER_PROP_EDGE_MODE_Y,
			D2D1_BORDER_EDGE_MODE_WRAP
		)
	);
	tileEffect->SetInputEffect(0, bitmapSourceEffect.get());

	if (!m_materialEffect)
	{
		RETURN_IF_FAILED(
			context->CreateEffect(
				CLSID_D2D1Opacity,
				m_materialEffect.put()
			)
		);
		RETURN_IF_FAILED(
			m_materialEffect->SetValue(
				D2D1_OPACITY_PROP_OPACITY,
				Shared::g_materialIntensity
			)
		);
		m_materialEffect->SetInputEffect(0, tileEffect.get());
	}

	return S_OK;
}

HRESULT CMaterialRealizer::Render(
	ID2D1DeviceContext* context,
	const std::span<const D2D1_RECT_F>& rectangles,
	const MaterialContext& materialContext
)
{
	if (!context)
	{
		return E_INVALIDARG;
	}
	if (materialContext.opacity <= 0.f || rectangles.empty())
	{
		return S_OK;
	}

	winrt::com_ptr<ID2D1DeviceContext6> contextForBlending{};
	RETURN_IF_FAILED(context->QueryInterface(contextForBlending.put()));

	if (!m_materialEffect)
	{
		RETURN_IF_FAILED(LoadEffect(context));
	}
	RETURN_IF_FAILED(
		m_materialEffect->SetValue(
			D2D1_OPACITY_PROP_OPACITY,
			materialContext.opacity
		)
	);
	winrt::com_ptr<ID2D1Image> materialImage{};
	m_materialEffect->GetOutput(materialImage.put());

	for (const auto& subRectangle : rectangles)
	{
		D2D1_POINT_2F targetOffset
		{
			subRectangle.left,
			subRectangle.top
		};
		contextForBlending->BlendImage(
			materialImage.get(),
			D2D1_BLEND_MODE_MULTIPLY,
			&targetOffset,
			&subRectangle,
			D2D1_INTERPOLATION_MODE_NEAREST_NEIGHBOR
		);
	}

	return S_OK;
}
