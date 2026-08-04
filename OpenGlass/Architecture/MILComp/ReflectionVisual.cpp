#include "pch.h"
#include "resource.h"
#include "Shared.hpp"
#include "ReflectionVisual.hpp"
#include "PngAssetValidation.hpp"

using namespace OpenGlass;

PVOID CReflectionVisual::s_customVtable[32]{};
PVOID CReflectionVisual::s_CSpriteVisual_vector_deleting_destructor_Org{};
PVOID CReflectionVisual::s_CSpriteVisual_CloneVisualTree_Org{};
LPCVOID CReflectionVisual::s_originalVtable{};

std::unordered_set<CReflectionVisual*> CReflectionVisual::s_activeList{};

void CReflectionVisual::Register(CReflectionVisual* visual)
{
	s_activeList.emplace(visual);
}

void CReflectionVisual::Unregister(CReflectionVisual* visual)
{
	s_activeList.erase(visual);
}

void CReflectionVisual::EnsureCustomVtable(PVOID const* sourceVtable)
{
	if (s_customVtable[0])
	{
		return;
	}

	if (!s_CSpriteVisual_CloneVisualTree_Org)
	{
		s_CSpriteVisual_CloneVisualTree_Org = uDWM::Symbol_CSpriteVisual_CloneVisualTree.get();
		THROW_HR_IF_NULL(E_UNEXPECTED, s_CSpriteVisual_CloneVisualTree_Org);
	}
	s_originalVtable = sourceVtable;
	std::copy_n(sourceVtable, std::size(s_customVtable), s_customVtable);

	s_CSpriteVisual_vector_deleting_destructor_Org = s_customVtable[0];
	s_customVtable[0] = Util::force_cast_from(&CReflectionVisual::vector_deleting_destructor);

	for (size_t i = 1; i < std::size(s_customVtable); ++i)
	{
		if (s_customVtable[i] == s_CSpriteVisual_CloneVisualTree_Org)
		{
			s_customVtable[i] = Util::force_cast_from(&CReflectionVisual::CloneVisualTree);
			break;
		}
	}
}

void* CReflectionVisual::vector_deleting_destructor(UINT flags)
{
	Unregister(this);

	return std::invoke(
		Util::force_cast_to<decltype(&CReflectionVisual::vector_deleting_destructor)>(
			s_CSpriteVisual_vector_deleting_destructor_Org
		),
		this,
		flags
	);
}

HRESULT CReflectionVisual::CloneVisualTree(CReflectionVisual** clonedVisual, UINT cloneOption) try
{
	winrt::com_ptr<CReflectionVisual> cloned{};
	THROW_IF_FAILED(Create(cloned.put()));

	THROW_IF_FAILED(uDWM::CContainerVisual::InitializeVisualTreeClone(cloned.get(), cloneOption));
	winrt::com_ptr<abi::ICompositionBrush> brush{};
	THROW_IF_FAILED(GetBrush(brush.put()));
	THROW_IF_FAILED(cloned->SetBrush(brush.get()));

	float opacity{ 0.f };
	winrt::com_ptr<abi::IVisual> visual{};
	THROW_IF_FAILED(GetVisualProxy()->GetSpriteVisualPartner()->QueryInterface(visual.put()));
	THROW_IF_FAILED(visual->get_Opacity(&opacity));
	THROW_IF_FAILED(cloned->GetVisualProxy()->GetSpriteVisualPartner()->QueryInterface(visual.put()));
	THROW_IF_FAILED(visual->put_Opacity(opacity));

	*clonedVisual = cloned.detach();
	return S_OK;
}
CATCH_RETURN()

void CReflectionVisual::RemoveAll()
{
	for (auto& visual : s_activeList)
	{
		HookHelper::get_vftable_reference_from(visual) = s_originalVtable;

		if (const auto parent = visual->GetTransformParent(); parent)
		{
			FAIL_FAST_IF_FAILED_MSG(parent->GetVisualCollection()->Remove(visual), "Unable to remove a reflection visual during shutdown");
		}
	}
	s_activeList.clear();
}

HRESULT CReflectionVisual::Create(CReflectionVisual** result) try
{
	winrt::com_ptr<uDWM::CSpriteVisual> visual{};
	THROW_IF_FAILED(uDWM::CSpriteVisual::Create(visual.put()));

	EnsureCustomVtable(HookHelper::get_vftable_from(visual.get()));
	HookHelper::get_vftable_reference_from(visual.get()) = s_customVtable;
	Register(static_cast<CReflectionVisual*>(visual.get()));

	*result = static_cast<CReflectionVisual*>(visual.detach());
	return S_OK;
}
CATCH_RETURN()

HRESULT CReflectionVisual::CreateSurface(abi::ICompositionSurface** surface) try
{
	winrt::com_ptr<IStream> stream{ nullptr };
	std::optional<PngAssetValidation::ImageInfo> expectedInfo;
	if (
		Shared::g_reflectionTexturePath.empty() ||
		PathIsRelativeW(Shared::g_reflectionTexturePath.data()) ||
		PathIsNetworkPathW(Shared::g_reflectionTexturePath.data()) ||
		!PathFileExistsW(Shared::g_reflectionTexturePath.data())
	)
	{
		LOG_HR_IF_MSG(HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND), !Shared::g_reflectionTexturePath.empty(), "invalid reflection texture path");
		std::span<const UCHAR> textureBytes{};
		THROW_IF_FAILED(Util::GetResDataView(textureBytes, IDB_REFLECTION, wil::GetModuleInstanceHandle(), L"PNG"));

		stream = { SHCreateMemStream(textureBytes.data(), static_cast<UINT>(textureBytes.size_bytes())), winrt::take_ownership_from_abi };
	}
	else
	{
		wil::unique_hfile file{ CreateFileW(Shared::g_reflectionTexturePath.data(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, 0) };
		THROW_LAST_ERROR_IF(!file.is_valid());

		LARGE_INTEGER fileSize{};
		THROW_IF_WIN32_BOOL_FALSE(GetFileSizeEx(file.get(), &fileSize));
		THROW_HR_IF(HRESULT_FROM_WIN32(ERROR_FILE_TOO_LARGE), fileSize.QuadPart <= 0 || fileSize.QuadPart > PngAssetValidation::MaximumFileSize);

		auto buffer{ std::make_unique<BYTE[]>(static_cast<size_t>(fileSize.QuadPart)) };
		DWORD bytesRead{};
		THROW_IF_WIN32_BOOL_FALSE(ReadFile(file.get(), buffer.get(), static_cast<DWORD>(fileSize.QuadPart), &bytesRead, nullptr));
		THROW_HR_IF(HRESULT_FROM_WIN32(ERROR_HANDLE_EOF), bytesRead != static_cast<DWORD>(fileSize.QuadPart));
		expectedInfo.emplace();
		THROW_IF_FAILED(PngAssetValidation::ValidateStructure(
			{ reinterpret_cast<const std::byte*>(buffer.get()), static_cast<std::size_t>(fileSize.QuadPart) },
			*expectedInfo
		));
		stream = { SHCreateMemStream(buffer.get(), static_cast<UINT>(fileSize.QuadPart)), winrt::take_ownership_from_abi };
	}
	THROW_HR_IF_NULL(E_OUTOFMEMORY, stream);

	winrt::com_ptr<IWICImagingFactory> wicFactory{ nullptr };
	wicFactory.copy_from(uDWM::CDesktopManager::GetInstance()->GetWICFactory());
	winrt::com_ptr<IWICFormatConverter> wicConverter{ nullptr };
	THROW_IF_FAILED(PngAssetValidation::CreateValidatedWicSource(
		wicFactory.get(),
		stream.get(),
		expectedInfo ? &*expectedInfo : nullptr,
		wicConverter.put()
	));
	UINT width = 0, height = 0;
	THROW_IF_FAILED(wicConverter->GetSize(&width, &height));

	winrt::com_ptr<abi::ICompositor> compositor{ nullptr };
	THROW_IF_FAILED(uDWM::CDesktopManager::GetInstance()->GetCompositor()->GetInteropCompositorDCompDevicePartner()->QueryInterface(compositor.put()));

	winrt::com_ptr<abi::ICompositionGraphicsDevice> graphicsDevice{ nullptr };
	THROW_IF_FAILED(
		compositor.as<abi::ICompositorInterop>()->CreateGraphicsDevice(
			uDWM::CDesktopManager::GetInstance()->GetD2DDevice(),
			graphicsDevice.put()
		)
	);

	winrt::com_ptr<abi::ICompositionGraphicsDevice2> graphicsDevice2{ nullptr };
	THROW_IF_FAILED(graphicsDevice->QueryInterface(graphicsDevice2.put()));

	winrt::com_ptr<abi::ICompositionDrawingSurface> drawingSurface{ nullptr };
	THROW_IF_FAILED(
		graphicsDevice2->CreateDrawingSurface2(
			{ static_cast<INT32>(width), static_cast<INT32>(height) },
			abi::DirectXPixelFormat_B8G8R8A8UIntNormalized,
			abi::DirectXAlphaMode_Premultiplied,
			drawingSurface.put()
		)
	);
	winrt::com_ptr<abi::ICompositionDrawingSurfaceInterop> drawingSurfaceInterop{ nullptr };
	THROW_IF_FAILED(drawingSurface->QueryInterface(drawingSurfaceInterop.put()));

	winrt::com_ptr<ID2D1DeviceContext> deviceContext{ nullptr };
	POINT offset{ 0, 0 };
	THROW_IF_FAILED(drawingSurfaceInterop->BeginDraw(nullptr, IID_PPV_ARGS(deviceContext.put()), &offset));
	deviceContext->Clear();
	winrt::com_ptr<ID2D1Bitmap1> bitmap{ nullptr };
	RETURN_IF_FAILED(
		deviceContext->CreateBitmapFromWicBitmap(
			wicConverter.get(),
			D2D1::BitmapProperties1(
				D2D1_BITMAP_OPTIONS_NONE,
				D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED)
			),
			bitmap.put()
		)
	);
	deviceContext->DrawBitmap(bitmap.get());
	THROW_IF_FAILED(drawingSurfaceInterop->EndDraw());
	THROW_IF_FAILED(drawingSurface->QueryInterface(surface));

	return S_OK;
}
CATCH_RETURN()

HRESULT CReflectionVisual::UpdateOpacity(float opacity)
{
	winrt::com_ptr<abi::IVisual> visual{};
	RETURN_IF_FAILED(GetVisualProxy()->GetSpriteVisualPartner()->QueryInterface(visual.put()));
	RETURN_IF_FAILED(visual->put_Opacity(opacity));

	return S_OK;
}

HRESULT CReflectionVisual::UpdateSurface(abi::ICompositionSurface* surface) try
{
	winrt::com_ptr<abi::ICompositionBrush> brush{};
	THROW_IF_FAILED(GetBrush(brush.put()));
	winrt::com_ptr<abi::ICompositionSurfaceBrush> surfaceBrush{};
	if (!brush)
	{
		winrt::com_ptr<abi::ICompositionObject> compositionObject{ nullptr };
		THROW_IF_FAILED(GetVisualProxy()->GetDCompVisualProxy()->QueryInterface(compositionObject.put()));
		winrt::com_ptr<abi::ICompositor> compositor{ nullptr };
		THROW_IF_FAILED(compositionObject->get_Compositor(compositor.put()));
		THROW_IF_FAILED(compositor->CreateSurfaceBrush(surfaceBrush.put()));

		THROW_IF_FAILED(surfaceBrush->put_Stretch(abi::CompositionStretch_None));
		THROW_IF_FAILED(surfaceBrush->put_BitmapInterpolationMode(abi::CompositionBitmapInterpolationMode_Linear));
		THROW_IF_FAILED(surfaceBrush->put_HorizontalAlignmentRatio(0.f));
		THROW_IF_FAILED(surfaceBrush->put_VerticalAlignmentRatio(0.f));

		THROW_IF_FAILED(surfaceBrush->QueryInterface(brush.put()));
		THROW_IF_FAILED(SetBrush(brush.get()));
	}
	else
	{
		THROW_IF_FAILED(brush->QueryInterface(surfaceBrush.put()));
	}

	THROW_IF_FAILED(surfaceBrush->put_Surface(surface));

	return S_OK;
}
CATCH_RETURN()

HRESULT CReflectionVisual::UpdateViewport(
	const POINT& offset,
	float parallaxIntensity,
	bool mirrored,
	LONG width,
	const D2D1_SIZE_F& scale
) try
{
	winrt::com_ptr<abi::ICompositionBrush> brush{};
	THROW_IF_FAILED(GetBrush(brush.put()));
	winrt::com_ptr<abi::ICompositionSurfaceBrush> surfaceBrush{};
	if (!brush)
	{
		winrt::com_ptr<abi::ICompositionObject> compositionObject{ nullptr };
		THROW_IF_FAILED(GetVisualProxy()->GetDCompVisualProxy()->QueryInterface(compositionObject.put()));
		winrt::com_ptr<abi::ICompositor> compositor{ nullptr };
		THROW_IF_FAILED(compositionObject->get_Compositor(compositor.put()));
		THROW_IF_FAILED(compositor->CreateSurfaceBrush(surfaceBrush.put()));

		THROW_IF_FAILED(surfaceBrush->put_Stretch(abi::CompositionStretch_None));
		THROW_IF_FAILED(surfaceBrush->put_BitmapInterpolationMode(abi::CompositionBitmapInterpolationMode_Linear));
		THROW_IF_FAILED(surfaceBrush->put_HorizontalAlignmentRatio(0.f));
		THROW_IF_FAILED(surfaceBrush->put_VerticalAlignmentRatio(0.f));

		THROW_IF_FAILED(SetBrush(brush.get()));
	}
	else
	{
		THROW_IF_FAILED(brush->QueryInterface(surfaceBrush.put()));
	}

	winrt::com_ptr<abi::ICompositionSurface> surface{};
	THROW_IF_FAILED(surfaceBrush->get_Surface(surface.put()));
	if (surface)
	{
		abi::Size surfaceSize{};
		THROW_IF_FAILED(surface.as<abi::ICompositionDrawingSurface>()->get_Size(&surfaceSize));

		winrt::com_ptr<abi::ICompositionSurfaceBrush2> surfaceBrush2{};
		THROW_IF_FAILED(surfaceBrush->QueryInterface(surfaceBrush2.put()));

		D2D1_RECT_F viewport{};
		LONG left = offset.x;
		if (mirrored)
		{
			left = GetSystemMetrics(SM_XVIRTUALSCREEN) + (GetSystemMetrics(SM_XVIRTUALSCREEN) + GetSystemMetrics(SM_CXVIRTUALSCREEN)) - (offset.x + width);
		}
		viewport.left = (GetSystemMetrics(SM_XVIRTUALSCREEN) - left) + ((left + width / 2.f) - (GetSystemMetrics(SM_XVIRTUALSCREEN) + GetSystemMetrics(SM_CXVIRTUALSCREEN) / 2.f)) * parallaxIntensity;
		viewport.top = static_cast<float>(
			GetSystemMetrics(SM_YVIRTUALSCREEN) -
			offset.y
		);

		viewport.right = viewport.left + static_cast<float>(GetSystemMetrics(SM_CXVIRTUALSCREEN));
		viewport.bottom = viewport.top + static_cast<float>(GetSystemMetrics(SM_CYVIRTUALSCREEN));

		viewport.left /= static_cast<float>(scale.width);
		viewport.top /= static_cast<float>(scale.height);
		viewport.right /= static_cast<float>(scale.width);
		viewport.bottom /= static_cast<float>(scale.height);

		THROW_IF_FAILED(surfaceBrush2->put_CenterPoint({ 0.f, 0.f }));
		THROW_IF_FAILED(surfaceBrush2->put_Offset({ static_cast<float>(viewport.left), static_cast<float>(viewport.top) }));
		THROW_IF_FAILED(surfaceBrush2->put_Scale({ wil::rect_width(viewport) / static_cast<float>(surfaceSize.Width), wil::rect_height(viewport) / static_cast<float>(surfaceSize.Height) }));
	}

	return S_OK;
}
CATCH_RETURN()
