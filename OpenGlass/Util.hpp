#pragma once
#include "framework.hpp"
#include "cpprt.hpp"
#include "HookHelper.hpp"
#include "DxgiPrivates.hpp"
#include <winrt/windows.ui.h>

namespace OpenGlass::Util
{
	inline const auto g_processHeap = GetProcessHeap();
	inline const auto g_thisModulePath = wil::GetModuleFileNameW<std::wstring, MAX_PATH>(wil::GetModuleInstanceHandle());

	using unique_rouninitialize_call = wil::unique_call<decltype(&::RoUninitialize), ::RoUninitialize>;

	// ptr to complex
	template <typename T>
	FORCEINLINE constexpr auto force_cast_to(PVOID ptr)
	{
		T target_ptr{ nullptr };
		*reinterpret_cast<PVOID*>(&target_ptr) = ptr;
		return target_ptr;
	}
	// complex to ptr
	template <typename T>
	FORCEINLINE constexpr PVOID force_cast_from(T ptr)
	{
		return *reinterpret_cast<PVOID*>(&ptr);
	}

	struct VersionInfo
	{
		ULONG build;
		ULONG revision;
	};

	template <ULONG build, ULONG revision>
	FORCEINLINE constexpr bool VersionBefore(
		ULONG runtimeBuild,
		ULONG runtimeRevision
	)
	{
		if constexpr (!build)
		{
			return true;
		}
		if constexpr (!revision)
		{
			return runtimeBuild < build;
		}
		return runtimeBuild < build ||
			(runtimeBuild == build && runtimeRevision < revision);
	}

	inline auto make_current_folder_file(std::wstring_view baseFileName)
	{
		const auto bufferSize = g_thisModulePath.size() + baseFileName.size() + 1;
		auto filePath = std::make_unique<WCHAR[]>(bufferSize);
		wcscpy_s(
			filePath.get(),
			bufferSize,
			g_thisModulePath.c_str()
		);
		PathCchRemoveFileSpec(filePath.get(), bufferSize);
		if (!baseFileName.empty())
		{
			PathCchAppend(filePath.get(), bufferSize, baseFileName.data());
		}

		return filePath;
	}
	
	template <UINT id>
	FORCEINLINE const auto GetResourceStringView()
	{
		LPCWSTR buffer{ nullptr };
		auto length = LoadStringW(wil::GetModuleInstanceHandle(), id, reinterpret_cast<LPWSTR>(&buffer), 0);
		return std::wstring_view{ buffer, static_cast<size_t>(length) };
	}
	template <UINT id>
	FORCEINLINE std::wstring GetResourceString()
	{
		return std::wstring{ GetResourceStringView<id>() };
	}

	inline void ClearMessageQueue(HWND hWnd, UINT uMsg)
	{
		MSG msg{};
		while (PeekMessageW(&msg, hWnd, uMsg, uMsg, PM_REMOVE)) {}
	}

	inline HRESULT GetResDataView(std::span<const UCHAR>& resourceView, UINT id, HMODULE moduleHandle = wil::GetModuleInstanceHandle(), LPCWSTR type = RT_RCDATA)
	{
		const auto resourceHandle = FindResourceW(moduleHandle, MAKEINTRESOURCE(id), type);
		RETURN_LAST_ERROR_IF_NULL(resourceHandle);
		const auto globalHandle = LoadResource(moduleHandle, resourceHandle);
		RETURN_LAST_ERROR_IF_NULL(globalHandle);
		const auto resourceAddress = reinterpret_cast<PBYTE>(LockResource(globalHandle));
		RETURN_LAST_ERROR_IF_NULL(resourceAddress);
		const auto resourceSize = SizeofResource(moduleHandle, resourceHandle);
		RETURN_LAST_ERROR_IF(resourceSize == 0);
		resourceView = { resourceAddress, resourceSize };

		return S_OK;
	}

	inline HRESULT MB2WC(std::unique_ptr<WCHAR[]>& buffer, LPCSTR str, int size = -1, int* outLength = nullptr)
	{
		auto length = MultiByteToWideChar(CP_ACP, 0, str, size, nullptr, 0);
		RETURN_LAST_ERROR_IF(length == 0);
		buffer.reset(new WCHAR[length]);
		RETURN_LAST_ERROR_IF_NULL(buffer);
		length = MultiByteToWideChar(CP_ACP, 0, str, size, buffer.get(), length);
		RETURN_LAST_ERROR_IF(length == 0);
		if (outLength) { *outLength = length; }
		return S_OK;
	}
	inline HRESULT WC2MB(std::unique_ptr<CHAR[]>& buffer, LPCWSTR str, int size = -1, int* outLength = nullptr)
	{
		auto length = WideCharToMultiByte(CP_UTF8, 0, str, size, nullptr, 0, nullptr, nullptr);
		RETURN_LAST_ERROR_IF(length == 0);
		buffer.reset(new CHAR[length]);
		RETURN_LAST_ERROR_IF_NULL(buffer);
		length = WideCharToMultiByte(CP_UTF8, 0, str, size, buffer.get(), length, nullptr, nullptr);
		RETURN_LAST_ERROR_IF(length == 0);
		if (outLength) { *outLength = length; }
		return S_OK;
	}

	// type = 1, input desktop (grpdeskRitInput)
	// type = 2, default desktop? (grpdeskIODefault)
	// type = 3, unknown
	// type = 4, winlogon desktop (grpdeskLogon)
	// type = ..., desktop created by CreateDesktop?
	FORCEINLINE bool WINAPI GetDesktopID(ULONG_PTR type, ULONG_PTR* desktopID)
	{
		static const auto sc_pfnGetDesktopID = reinterpret_cast<decltype(&GetDesktopID)>(GetProcAddress(GetModuleHandleW(L"user32.dll"), "GetDesktopID"));
		if (sc_pfnGetDesktopID) [[likely]]
		{
			return sc_pfnGetDesktopID(type, desktopID);
		}

		return false;
	}

	FORCEINLINE bool WINAPI IsTransparencyEnabled()
	{
		DWORD value{ 0 };
		wil::reg::get_value_dword_nothrow(HKEY_CURRENT_USER, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize", L"EnableTransparency", &value);
		return static_cast<bool>(value);
	}
	FORCEINLINE bool WINAPI IsTransparencyEnabled(HKEY key)
	{
		DWORD value{ 0 };
		wil::reg::get_value_dword_nothrow(key, L"EnableTransparency", &value);
		return static_cast<bool>(value);
	}

	FORCEINLINE bool WINAPI IsRunAsLocalSystem()
	{
		BOOL isLocalSystem{ FALSE };
		wil::unique_sid sid{ nullptr };
		SID_IDENTIFIER_AUTHORITY authority{ SECURITY_NT_AUTHORITY };
		if (AllocateAndInitializeSid(&authority, 1ul, SECURITY_LOCAL_SYSTEM_RID, 0, 0, 0, 0, 0, 0, 0, &sid))
		{
			CheckTokenMembership(0, sid.get(), &isLocalSystem);
		}
		return static_cast<bool>(isLocalSystem);
	}

	inline HRESULT WINAPI TaskDialog(
		_In_opt_ HWND hwndOwner,
		_In_opt_ HINSTANCE hInstance,
		_In_opt_ PCWSTR pszWindowTitle,
		_In_opt_ PCWSTR pszMainInstruction,
		_In_opt_ PCWSTR pszContent,
		TASKDIALOG_COMMON_BUTTON_FLAGS dwCommonButtons, 
		_In_opt_ PCWSTR pszIcon,
		_Out_opt_ int* pnButton
	)
	{
		TASKDIALOGCONFIG config
		{ 
			sizeof(TASKDIALOGCONFIG), 
			hwndOwner, 
			hInstance,
			TDF_SIZE_TO_CONTENT | TDF_ALLOW_DIALOG_CANCELLATION,
			dwCommonButtons,
			pszWindowTitle,
			{ .pszMainIcon{ pszIcon } },
			pszMainInstruction,
			pszContent,
			0, 
			nullptr, 
			0, 
			0, 
			nullptr, 
			0, 
			nullptr, 
			nullptr, 
			nullptr, 
			nullptr, 
			{}, 
			nullptr, 
			nullptr, 
			0, 
			0 
		};
		return TaskDialogIndirect(
			&config,
			pnButton,
			nullptr,
			nullptr
		);
	}

	inline VersionInfo GetModuleVersionInfo(HMODULE moduleHandle)
	{
		const auto moduleName = wil::GetModuleFileNameW<std::wstring, MAX_PATH>(moduleHandle);

		DWORD size{ GetFileVersionInfoSizeW(moduleName.c_str(), nullptr)};
		THROW_LAST_ERROR_IF(!size);

		auto data{ std::make_unique_for_overwrite<BYTE[]>(size) };
		#pragma warning(suppress : 6388)
		THROW_LAST_ERROR_IF(!GetFileVersionInfoW(moduleName.c_str(), 0, size, data.get()));

		UINT len{ 0 };
		VS_FIXEDFILEINFO* fileInfo{ nullptr };
		THROW_IF_WIN32_BOOL_FALSE(VerQueryValueW(data.get(), L"\\", reinterpret_cast<PVOID*>(&fileInfo), &len));
		THROW_LAST_ERROR_IF(!len);

		THROW_WIN32_IF(ERROR_INVALID_IMAGE_HASH, fileInfo->dwSignature != VS_FFI_SIGNATURE);

		return { HIWORD(fileInfo->dwFileVersionLS), LOWORD(fileInfo->dwFileVersionLS) };
	}

	inline HRESULT GetSwapchainFromTexure(
		ID3D11Texture2D* texture,
		winrt::com_ptr<IDXGISwapChain1>& dxgiSwapChain
	)
	{
		winrt::com_ptr<IDXGISurface> dxgiSurface{};
		RETURN_IF_FAILED(texture->QueryInterface(dxgiSurface.put()));
		return dxgiSurface->GetParent(IID_PPV_ARGS(dxgiSwapChain.put()));
	}

	inline HRESULT GetDescAndHwProtectionStateFromTexture(
		ID3D11Texture2D* texture,
		D3D11_TEXTURE2D_DESC& textureDesc,
		BOOL& hwProtectionEnabled
	)
	{
		texture->GetDesc(&textureDesc);

		if ((textureDesc.MiscFlags & D3D11_RESOURCE_MISC_HW_PROTECTED) == D3D11_RESOURCE_MISC_HW_PROTECTED)
		{
			hwProtectionEnabled = TRUE;
		}
		else
		{
			winrt::com_ptr<IDXGISwapChain1> dxgiSwapChain{};
			if (SUCCEEDED(GetSwapchainFromTexure(texture, dxgiSwapChain)))
			{
				winrt::com_ptr<IDXGISwapChainDWM1> dxgiSwapchainDwm{};
				RETURN_IF_FAILED(dxgiSwapChain->QueryInterface(dxgiSwapchainDwm.put()));
				RETURN_IF_FAILED(dxgiSwapchainDwm->GetHardwareProtection(&hwProtectionEnabled));
			}
		}

		return S_OK;
	}

	inline HRESULT GetTextureFromD2DBitmap(
		ID2D1Bitmap1* bitmap,
		winrt::com_ptr<ID3D11Texture2D>& texture
	)
	{
		winrt::com_ptr<IDXGISurface> dxgiSurface{};
		RETURN_IF_FAILED(bitmap->GetSurface(dxgiSurface.put()));
		RETURN_IF_FAILED(dxgiSurface->QueryInterface(texture.put()));
		return S_OK;
	}

	inline void CopyTextureRegion(
		ID3D11DeviceContext* context,
		ID3D11Texture2D* src,
		ID3D11Texture2D* dst,
		const D2D1_RECT_U& srcRect,
		UINT dstX,
		UINT dstY
	)
	{
		D3D11_BOX box{};
		box.left = srcRect.left;
		box.top = srcRect.top;
		box.right = srcRect.right;
		box.bottom = srcRect.bottom;
		box.front = 0;
		box.back = 1;
		context->CopySubresourceRegion(dst, 0, dstX, dstY, 0, src, 0, &box);
	}

	inline HRESULT GetTargetBitmapFromD2DContext(
		const ID2D1DeviceContext* context,
		winrt::com_ptr<ID2D1Bitmap1>& targetBitmap
	)
	{
		winrt::com_ptr<ID2D1Image> targetImage{ nullptr };
		context->GetTarget(targetImage.put());
		if (!targetImage)
		{
			return E_FAIL;
		}

		RETURN_IF_FAILED(targetImage->QueryInterface(targetBitmap.put()));
		return S_OK;
	}

	inline HRESULT DrawNineGridBitmap(
		ID2D1DeviceContext* context,
		ID2D1Bitmap1* bitmap,
		const D2D1_RECT_F& destinationRect,
		const MARGINS& margins,
		float opacity
	)
	{
		const auto primitiveBlend = context->GetPrimitiveBlend();
		context->SetPrimitiveBlend(D2D1_PRIMITIVE_BLEND_COPY);
		context->PushAxisAlignedClip(destinationRect, D2D1_ANTIALIAS_MODE_ALIASED);
		const auto restoreBlendMode = wil::scope_exit([&] 
		{
			context->PopAxisAlignedClip();
			context->SetPrimitiveBlend(primitiveBlend);
		});

		const auto destinationWidth = wil::rect_width(destinationRect);
		const auto destinationHeight = wil::rect_height(destinationRect);

		if (destinationWidth <= 0.f || destinationHeight <= 0.f)
		{
			return S_OK; // Nothing to draw for non-positive destination size
		}

		const auto bitmapSize = bitmap->GetSize();

		// Source margin sizes from input MARGINS, converted to float
		// Assuming margins members (cxLeftWidth etc.) are non-negative as is typical.
		D2D1_RECT_F srcMargins
		{
			static_cast<float>(margins.cxLeftWidth),
			static_cast<float>(margins.cyTopHeight),
			static_cast<float>(margins.cxRightWidth),
			static_cast<float>(margins.cyBottomHeight)
		};

		// Source center part dimensions from bitmap
		const auto totalSrcHorizontalMargins = srcMargins.left + srcMargins.right;
		const auto totalSrcVerticalMargins = srcMargins.top + srcMargins.bottom;
		const auto srcCenterPartWidth = std::max(0.f, bitmapSize.width - totalSrcHorizontalMargins);
		const auto srcCenterPartHeight = std::max(0.f, bitmapSize.height - totalSrcVerticalMargins);

		// Initialize effective destination dimensions for margins and center parts
		D2D1_RECT_F destMargins = srcMargins;

		auto destCenterPartWidth = destinationWidth - totalSrcHorizontalMargins;
		auto destCenterPartHeight = destinationHeight - totalSrcVerticalMargins;

		// Adjust horizontal destination parts if calculated center width is negative
		if (destCenterPartWidth < 0.f)
		{
			destCenterPartWidth = 0.f; // Center part cannot be negative
			
			const auto ratio = destinationWidth / totalSrcHorizontalMargins;
			destMargins.left *= ratio;
			destMargins.left = std::floor(destMargins.left);
			destMargins.right = destinationWidth - destMargins.left;
		}

		// Adjust vertical destination parts if calculated center height is negative
		if (destCenterPartHeight < 0.f)
		{
			destCenterPartHeight = 0.f; // Center part cannot be negative

			const auto ratio = destinationHeight / totalSrcVerticalMargins;
			destMargins.top *= ratio;
			destMargins.top = std::floor(destMargins.top);
			destMargins.bottom = destinationHeight - destMargins.top;
		}

		// Top-Left
		context->DrawBitmap(
			bitmap,
			D2D1::RectF(destinationRect.left, destinationRect.top, destinationRect.left + destMargins.left, destinationRect.top + destMargins.top),
			opacity, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR,
			D2D1::RectF(0.f, 0.f, srcMargins.left, srcMargins.top)
		);

		if (destCenterPartWidth)
		{
			// Top-Center
			context->DrawBitmap(
				bitmap,
				D2D1::RectF(destinationRect.left + destMargins.left, destinationRect.top, destinationRect.left + destMargins.left + destCenterPartWidth, destinationRect.top + destMargins.top),
				opacity, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR,
				D2D1::RectF(srcMargins.left, 0.f, srcMargins.left + srcCenterPartWidth, srcMargins.top)
			);
		}

		// Top-Right
		context->DrawBitmap(
			bitmap,
			D2D1::RectF(destinationRect.right - destMargins.right, destinationRect.top, destinationRect.right, destinationRect.top + destMargins.top),
			opacity, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR,
			D2D1::RectF(bitmapSize.width - srcMargins.right, 0.f, bitmapSize.width, srcMargins.top)
		);

		if (destCenterPartHeight)
		{
			// Middle-Left
			context->DrawBitmap(
				bitmap,
				D2D1::RectF(destinationRect.left, destinationRect.top + destMargins.top, destinationRect.left + destMargins.left, destinationRect.top + destMargins.top + destCenterPartHeight),
				opacity, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR,
				D2D1::RectF(0.f, srcMargins.top, srcMargins.left, srcMargins.top + srcCenterPartHeight)
			);
		}

		if (destCenterPartWidth && destCenterPartHeight)
		{
			// Middle-Center
			context->DrawBitmap(
				bitmap,
				D2D1::RectF(destinationRect.left + destMargins.left, destinationRect.top + destMargins.top, destinationRect.left + destMargins.left + destCenterPartWidth, destinationRect.top + destMargins.top + destCenterPartHeight),
				opacity, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR,
				D2D1::RectF(srcMargins.left, srcMargins.top, srcMargins.left + srcCenterPartWidth, srcMargins.top + srcCenterPartHeight)
			);
		}

		if (destCenterPartHeight)
		{
			// Middle-Right
			context->DrawBitmap(
				bitmap,
				D2D1::RectF(destinationRect.right - destMargins.right, destinationRect.top + destMargins.top, destinationRect.right, destinationRect.top + destMargins.top + destCenterPartHeight),
				opacity, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR,
				D2D1::RectF(bitmapSize.width - srcMargins.right, srcMargins.top, bitmapSize.width, srcMargins.top + srcCenterPartHeight)
			);
		}

		// Bottom-Left
		context->DrawBitmap(
			bitmap,
			D2D1::RectF(destinationRect.left, destinationRect.bottom - destMargins.bottom, destinationRect.left + destMargins.left, destinationRect.bottom),
			opacity, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR,
			D2D1::RectF(0.f, bitmapSize.height - srcMargins.bottom, srcMargins.left, bitmapSize.height)
		);

		if (destCenterPartWidth)
		{
			// Bottom-Center
			context->DrawBitmap(
				bitmap,
				D2D1::RectF(destinationRect.left + destMargins.left, destinationRect.bottom - destMargins.bottom, destinationRect.left + destMargins.left + destCenterPartWidth, destinationRect.bottom),
				opacity, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR,
				D2D1::RectF(srcMargins.left, bitmapSize.height - srcMargins.bottom, srcMargins.left + srcCenterPartWidth, bitmapSize.height)
			);
		}

		// Bottom-Right
		context->DrawBitmap(
			bitmap,
			D2D1::RectF(destinationRect.right - destMargins.right, destinationRect.bottom - destMargins.bottom, destinationRect.right, destinationRect.bottom),
			opacity, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR,
			D2D1::RectF(bitmapSize.width - srcMargins.right, bitmapSize.height - srcMargins.bottom, bitmapSize.width, bitmapSize.height)
		);

		return S_OK;
	}
	class D3D11ContextStateGuard
	{
	private:
		winrt::com_ptr<ID3D11DeviceContext3> m_context;

		// Vertex Shader State
		winrt::com_ptr<ID3D11VertexShader> m_vs;
		std::array<winrt::com_ptr<ID3D11Buffer>, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT> m_vsConstantBuffers;
		std::array<winrt::com_ptr<ID3D11ShaderResourceView>, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT> m_vsSRVs;
		std::array<winrt::com_ptr<ID3D11SamplerState>, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT> m_vsSamplers;

		// Pixel Shader State
		winrt::com_ptr<ID3D11PixelShader> m_ps;
		std::array<winrt::com_ptr<ID3D11Buffer>, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT> m_psConstantBuffers;
		std::array<winrt::com_ptr<ID3D11ShaderResourceView>, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT> m_psSRVs;
		std::array<winrt::com_ptr<ID3D11SamplerState>, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT> m_psSamplers;

		// Input Assembler State
		winrt::com_ptr<ID3D11InputLayout> m_inputLayout;
		D3D11_PRIMITIVE_TOPOLOGY m_primitiveTopology;
		std::array<winrt::com_ptr<ID3D11Buffer>, D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT> m_vertexBuffers;
		std::array<UINT, D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT> m_vertexStrides;
		std::array<UINT, D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT> m_vertexOffsets;
		winrt::com_ptr<ID3D11Buffer> m_indexBuffer;
		DXGI_FORMAT m_indexFormat;
		UINT m_indexOffset;

		// Rasterizer State
		winrt::com_ptr<ID3D11RasterizerState> m_rasterizerState;
		std::array<D3D11_VIEWPORT, D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE> m_viewports;
		UINT m_numViewports;
		std::array<D3D11_RECT, D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE> m_scissorRects;
		UINT m_numScissorRects;

		// Output Merger State
		std::array<winrt::com_ptr<ID3D11RenderTargetView>, D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT> m_renderTargetViews;
		winrt::com_ptr<ID3D11DepthStencilView> m_depthStencilView;
		winrt::com_ptr<ID3D11DepthStencilState> m_depthStencilState;
		UINT m_stencilRef;
		winrt::com_ptr<ID3D11BlendState> m_blendState;
		std::array<FLOAT, 4> m_blendFactor;
		UINT m_sampleMask;

		BOOL m_hwProtectionEnabled;
	public:
		explicit D3D11ContextStateGuard(ID3D11DeviceContext* context)
			: m_primitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED)
			, m_indexFormat(DXGI_FORMAT_UNKNOWN)
			, m_indexOffset(0)
			, m_numViewports(0)
			, m_numScissorRects(0)
			, m_stencilRef(0)
			, m_sampleMask(0)
			, m_hwProtectionEnabled(FALSE)
		{
			THROW_IF_FAILED(context->QueryInterface(m_context.put()));
			SaveState();
		}

		~D3D11ContextStateGuard()
		{
			RestoreState();
		}

		D3D11ContextStateGuard(const D3D11ContextStateGuard&) = delete;
		D3D11ContextStateGuard& operator=(const D3D11ContextStateGuard&) = delete;

	private:
		void SaveState()
		{
			if (!m_context) return;

			// Save Vertex Shader State
			ID3D11VertexShader* vs = nullptr;
			m_context->VSGetShader(&vs, nullptr, nullptr);
			m_vs.attach(vs);

			ID3D11Buffer* vsBuffers[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT] = {};
			m_context->VSGetConstantBuffers(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT, vsBuffers);
			for (UINT i = 0; i < D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT; ++i)
			{
				m_vsConstantBuffers[i].attach(vsBuffers[i]);
			}

			ID3D11ShaderResourceView* vsSRVs[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT] = {};
			m_context->VSGetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, vsSRVs);
			for (UINT i = 0; i < D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT; ++i)
			{
				m_vsSRVs[i].attach(vsSRVs[i]);
			}

			ID3D11SamplerState* vsSamplers[D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT] = {};
			m_context->VSGetSamplers(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT, vsSamplers);
			for (UINT i = 0; i < D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT; ++i)
			{
				m_vsSamplers[i].attach(vsSamplers[i]);
			}

			// Save Pixel Shader State
			ID3D11PixelShader* ps = nullptr;
			m_context->PSGetShader(&ps, nullptr, nullptr);
			m_ps.attach(ps);

			ID3D11Buffer* psBuffers[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT] = {};
			m_context->PSGetConstantBuffers(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT, psBuffers);
			for (UINT i = 0; i < D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT; ++i)
			{
				m_psConstantBuffers[i].attach(psBuffers[i]);
			}

			ID3D11ShaderResourceView* psSRVs[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT] = {};
			m_context->PSGetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, psSRVs);
			for (UINT i = 0; i < D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT; ++i)
			{
				m_psSRVs[i].attach(psSRVs[i]);
			}

			ID3D11SamplerState* psSamplers[D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT] = {};
			m_context->PSGetSamplers(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT, psSamplers);
			for (UINT i = 0; i < D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT; ++i)
			{
				m_psSamplers[i].attach(psSamplers[i]);
			}

			// Save Input Assembler State
			ID3D11InputLayout* inputLayout = nullptr;
			m_context->IAGetInputLayout(&inputLayout);
			m_inputLayout.attach(inputLayout);

			m_context->IAGetPrimitiveTopology(&m_primitiveTopology);

			ID3D11Buffer* vertexBuffers[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT] = {};
			m_context->IAGetVertexBuffers(0, D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT,
				vertexBuffers, m_vertexStrides.data(), m_vertexOffsets.data());
			for (UINT i = 0; i < D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT; ++i)
			{
				m_vertexBuffers[i].attach(vertexBuffers[i]);
			}

			ID3D11Buffer* indexBuffer = nullptr;
			m_context->IAGetIndexBuffer(&indexBuffer, &m_indexFormat, &m_indexOffset);
			m_indexBuffer.attach(indexBuffer);

			// Save Rasterizer State
			ID3D11RasterizerState* rasterizerState = nullptr;
			m_context->RSGetState(&rasterizerState);
			m_rasterizerState.attach(rasterizerState);

			m_numViewports = D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;
			m_context->RSGetViewports(&m_numViewports, m_viewports.data());

			m_numScissorRects = D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;
			m_context->RSGetScissorRects(&m_numScissorRects, m_scissorRects.data());

			// Save Output Merger State
			ID3D11RenderTargetView* renderTargetViews[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT] = {};
			ID3D11DepthStencilView* depthStencilView = nullptr;
			m_context->OMGetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, renderTargetViews, &depthStencilView);

			for (UINT i = 0; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i)
			{
				m_renderTargetViews[i].attach(renderTargetViews[i]);
			}
			m_depthStencilView.attach(depthStencilView);

			ID3D11DepthStencilState* depthStencilState = nullptr;
			m_context->OMGetDepthStencilState(&depthStencilState, &m_stencilRef);
			m_depthStencilState.attach(depthStencilState);

			ID3D11BlendState* blendState = nullptr;
			m_context->OMGetBlendState(&blendState, m_blendFactor.data(), &m_sampleMask);
			m_blendState.attach(blendState);

			m_context->GetHardwareProtectionState(&m_hwProtectionEnabled);
		}

		void RestoreState()
		{
			if (!m_context) return;

			// Restore Vertex Shader State
			m_context->VSSetShader(m_vs.get(), nullptr, 0);

			ID3D11Buffer* vsBuffers[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT] = {};
			for (UINT i = 0; i < D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT; ++i)
			{
				vsBuffers[i] = m_vsConstantBuffers[i].get();
			}
			m_context->VSSetConstantBuffers(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT, vsBuffers);

			ID3D11ShaderResourceView* vsSRVs[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT] = {};
			for (UINT i = 0; i < D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT; ++i)
			{
				vsSRVs[i] = m_vsSRVs[i].get();
			}
			m_context->VSSetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, vsSRVs);

			ID3D11SamplerState* vsSamplers[D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT] = {};
			for (UINT i = 0; i < D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT; ++i)
			{
				vsSamplers[i] = m_vsSamplers[i].get();
			}
			m_context->VSSetSamplers(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT, vsSamplers);

			// Restore Pixel Shader State
			m_context->PSSetShader(m_ps.get(), nullptr, 0);

			ID3D11Buffer* psBuffers[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT] = {};
			for (UINT i = 0; i < D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT; ++i)
			{
				psBuffers[i] = m_psConstantBuffers[i].get();
			}
			m_context->PSSetConstantBuffers(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT, psBuffers);

			ID3D11ShaderResourceView* psSRVs[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT] = {};
			for (UINT i = 0; i < D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT; ++i)
			{
				psSRVs[i] = m_psSRVs[i].get();
			}
			m_context->PSSetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, psSRVs);

			ID3D11SamplerState* psSamplers[D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT] = {};
			for (UINT i = 0; i < D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT; ++i)
			{
				psSamplers[i] = m_psSamplers[i].get();
			}
			m_context->PSSetSamplers(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT, psSamplers);

			// Restore Input Assembler State
			m_context->IASetInputLayout(m_inputLayout.get());
			m_context->IASetPrimitiveTopology(m_primitiveTopology);

			ID3D11Buffer* vertexBuffers[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT] = {};
			for (UINT i = 0; i < D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT; ++i)
			{
				vertexBuffers[i] = m_vertexBuffers[i].get();
			}
			m_context->IASetVertexBuffers(0, D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT,
				vertexBuffers, m_vertexStrides.data(), m_vertexOffsets.data());

			m_context->IASetIndexBuffer(m_indexBuffer.get(), m_indexFormat, m_indexOffset);

			// Restore Rasterizer State
			m_context->RSSetState(m_rasterizerState.get());
			m_context->RSSetViewports(m_numViewports, m_viewports.data());
			m_context->RSSetScissorRects(m_numScissorRects, m_scissorRects.data());

			// Restore Output Merger State
			ID3D11RenderTargetView* renderTargetViews[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT] = {};
			for (UINT i = 0; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i)
			{
				renderTargetViews[i] = m_renderTargetViews[i].get();
			}
			m_context->OMSetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT,
				renderTargetViews, m_depthStencilView.get());

			m_context->OMSetDepthStencilState(m_depthStencilState.get(), m_stencilRef);
			m_context->OMSetBlendState(m_blendState.get(), m_blendFactor.data(), m_sampleMask);

			m_context->SetHardwareProtectionState(m_hwProtectionEnabled);
		}
	};

	// Object pool with key-based reuse and TTL eviction.
	// Released objects are kept for reuse instead of being freed immediately,
	// avoiding repeated allocation of expensive resources (e.g. D3D11 textures).
	// Stale entries are evicted when Cleanup is called (typically at EndDraw).
	// Caller must ensure single-threaded access.
	// Released objects are heap-allocated; only unique_ptr moves, never the object.
	// Acquire transfers ownership to caller; Release transfers it back to the pool.
	template <typename Key, typename T>
	class ObjectPool
	{
		using Clock = std::chrono::steady_clock;
		using TimePoint = Clock::time_point;

		struct Entry
		{
			std::unique_ptr<T> m_object;
			TimePoint m_timestamp;
		};

		std::unordered_multimap<Key, Entry> m_released;

	public:
		~ObjectPool() = default;

		ObjectPool() = default;
		ObjectPool(const ObjectPool&) = delete;
		ObjectPool& operator=(const ObjectPool&) = delete;
		ObjectPool(ObjectPool&&) = delete;
		ObjectPool& operator=(ObjectPool&&) = delete;

		FORCEINLINE std::unique_ptr<T> Acquire(const Key& key)
		{
			const auto it = m_released.find(key);
			if (it != m_released.end())
			{
				auto result = std::move(it->second.m_object);
				m_released.erase(it);
				return result;
			}
			return std::make_unique<T>();
		}

		FORCEINLINE void Release(const Key& key, std::unique_ptr<T> object)
		{
			if (!object)
			{
				return;
			}
			m_released.emplace(
				std::piecewise_construct,
				std::forward_as_tuple(key),
				std::forward_as_tuple(std::move(object), Clock::now())
			);
		}

		template <typename Duration = std::chrono::seconds>
		FORCEINLINE void Cleanup(Duration ttl, std::optional<Key> key = std::nullopt)
		{
			const auto now = Clock::now();
			if (key)
			{
				const auto [begin, end] = m_released.equal_range(*key);
				for (auto it = begin; it != end; )
				{
					if (now - it->second.m_timestamp > ttl)
					{
						it = m_released.erase(it);
					}
					else
					{
						++it;
					}
				}
			}
			else
			{
				for (auto it = m_released.begin(); it != m_released.end(); )
				{
					if (now - it->second.m_timestamp > ttl)
					{
						it = m_released.erase(it);
					}
					else
					{
						++it;
					}
				}
			}
		}
	};

	template <typename T>
	struct ScopedAvgTimer
	{
		const wchar_t* m_tag{};
		std::chrono::steady_clock::time_point m_start{ std::chrono::steady_clock::now() };
		inline static std::chrono::steady_clock::time_point s_last{};
		inline static double s_accumulatedDuration{};
		inline static size_t s_accumulatedCount{};

		explicit ScopedAvgTimer(const wchar_t* tag) noexcept : m_tag(tag) 
		{
			if (!s_last.time_since_epoch().count())
			{
				s_last = std::chrono::steady_clock::now();
			}
		}
		~ScopedAvgTimer() noexcept
		{
			const auto end = std::chrono::steady_clock::now();
			const double us = std::chrono::duration<double, std::micro>(end - m_start).count();
			s_accumulatedDuration += us;
			s_accumulatedCount += 1;

			if (end - s_last >= std::chrono::seconds{ 1 })
			{
				const auto avg = s_accumulatedDuration / s_accumulatedCount;

				wchar_t buf[256]{};
				swprintf_s(buf, L"[OpenGlass] %s avg time: %.3f us (total: %lld)\n", m_tag ? m_tag : L"scope", avg, s_accumulatedCount);
				OutputDebugStringW(buf);

				s_last = end;
				s_accumulatedDuration = 0;
				s_accumulatedCount = 0;
			}
		}
	};

}
namespace OpenGlass
{
	template<class T>
	struct CClassFactoryT : winrt::implements<CClassFactoryT<T>, IClassFactory, winrt::non_agile, winrt::no_weak_ref>
	{
		HRESULT CreateInstance(IUnknown* pUnkOuter, REFIID riid, void** ppvObject) noexcept override
		{
			if (!pUnkOuter)
			{
				*ppvObject = nullptr;
				return winrt::make<T>().as(riid, ppvObject);
			}

			return CLASS_E_NOAGGREGATION;
		}
		HRESULT LockServer(BOOL fLock) noexcept override
		{
			if (fLock)
			{
				++winrt::get_module_lock();
			}
			else
			{
				--winrt::get_module_lock();
			}

			return S_OK;
		}
	};
}
namespace OpenGlass
{
	namespace Color
	{
		FORCEINLINE D2D1_COLOR_F sRGBToscRGB(const D2D1_COLOR_F& color, float sdrBoost)
		{
			auto result = D2D1ConvertColorSpace(
				D2D1_COLOR_SPACE_SRGB,
				D2D1_COLOR_SPACE_SCRGB,
				&color
			);
			if (sdrBoost > 1.f)
			{
				result.r *= sdrBoost;
				result.g *= sdrBoost;
				result.b *= sdrBoost;
			}

			return result;
		}
		FORCEINLINE D2D1_COLOR_F scRGBTosRGB(const D2D1_COLOR_F& color, float sdrBoost)
		{
			auto result = D2D1ConvertColorSpace(
				D2D1_COLOR_SPACE_SCRGB,
				D2D1_COLOR_SPACE_SRGB,
				&color
			);
			if (sdrBoost > 1.f)
			{
				result.r /= sdrBoost;
				result.g /= sdrBoost;
				result.b /= sdrBoost;
			}

			return result;
		}
		FORCEINLINE constexpr D2D1_COLOR_F FromAbgr(DWORD color, bool ignoreAlpha = true)
		{
			return
			{
				static_cast<float>((color & 0x000000FF) >> 0) / 255.f,
				static_cast<float>((color & 0x0000FF00) >> 8) / 255.f,
				static_cast<float>((color & 0x00FF0000) >> 16) / 255.f,
				ignoreAlpha ? 1.f : static_cast<float>((color & 0xFF000000) >> 24) / 255.f
			};
		}
		FORCEINLINE constexpr D2D1_COLOR_F FromArgb(DWORD color, bool ignoreAlpha = true)
		{
			return
			{
				static_cast<float>((color & 0x00FF0000) >> 16) / 255.f,
				static_cast<float>((color & 0x0000FF00) >> 8) / 255.f,
				static_cast<float>((color & 0x000000FF) >> 0) / 255.f,
				ignoreAlpha ? 1.f : static_cast<float>((color & 0xFF000000) >> 24) / 255.f
			};
		}
		FORCEINLINE constexpr DWORD ToArgb(const D2D1_COLOR_F& color, bool ignoreAlpha = true)
		{
			return
				((ignoreAlpha ? 0xFFul : std::min(static_cast<DWORD>(color.a * 255.f + 0.5f), 255ul)) << 24) |
				(std::min(static_cast<DWORD>(color.r * 255.f + 0.5f), 255ul) << 16) |
				(std::min(static_cast<DWORD>(color.g * 255.f + 0.5f), 255ul) << 8) |
				(std::min(static_cast<DWORD>(color.b * 255.f + 0.5f), 255ul) << 0);
		}
		FORCEINLINE constexpr DWORD ToAbgr(const D2D1_COLOR_F& color, bool ignoreAlpha = true)
		{
			return
				((ignoreAlpha ? 0xFFul : std::min(static_cast<DWORD>(color.a * 255.f + 0.5f), 255ul)) << 24) |
				(std::min(static_cast<DWORD>(color.b * 255.f + 0.5f), 255ul) << 16) |
				(std::min(static_cast<DWORD>(color.g * 255.f + 0.5f), 255ul) << 8) |
				(std::min(static_cast<DWORD>(color.r * 255.f + 0.5f), 255ul) << 0);
		}
		FORCEINLINE constexpr winrt::Windows::UI::Color ToWinUIColor(const D2D1_COLOR_F& color)
		{
			return
			{
				static_cast<uint8_t>(std::min(static_cast<DWORD>(color.a * 255.f + 0.5f), 255ul)),
				static_cast<uint8_t>(std::min(static_cast<DWORD>(color.r * 255.f + 0.5f), 255ul)),
				static_cast<uint8_t>(std::min(static_cast<DWORD>(color.g * 255.f + 0.5f), 255ul)),
				static_cast<uint8_t>(std::min(static_cast<DWORD>(color.b * 255.f + 0.5f), 255ul))
			};
		}
	}

	namespace RectF
	{
		FORCEINLINE [[nodiscard]] D2D1_RECT_F Align(const D2D1_RECT_F& rectangle)
		{
			return
			{
				std::floor(rectangle.left + 0.5f),
				std::floor(rectangle.top + 0.5f),
				std::floor(rectangle.right + 0.5f),
				std::floor(rectangle.bottom + 0.5f)
			};
		}
		FORCEINLINE [[nodiscard]] D2D1_RECT_F FromRectU(const D2D1_RECT_U& rectangle)
		{
			return
			{
				static_cast<float>(rectangle.left),
				static_cast<float>(rectangle.top),
				static_cast<float>(rectangle.right),
				static_cast<float>(rectangle.bottom)
			};
		}
		FORCEINLINE [[nodiscard]] D2D1_RECT_F FromRectL(const D2D1_RECT_L& rectangle)
		{
			return
			{
				static_cast<float>(rectangle.left),
				static_cast<float>(rectangle.top),
				static_cast<float>(rectangle.right),
				static_cast<float>(rectangle.bottom)
			};
		}
		FORCEINLINE [[nodiscard]] D2D1_RECT_U ToRectU(const D2D1_RECT_F& rectangle)
		{
			return
			{
				static_cast<UINT32>(std::floor(rectangle.left)),
				static_cast<UINT32>(std::floor(rectangle.top)),
				static_cast<UINT32>(std::ceil(rectangle.right)),
				static_cast<UINT32>(std::ceil(rectangle.bottom))
			};
		}
		FORCEINLINE [[nodiscard]] D2D1_RECT_L ToRectL(const D2D1_RECT_F& rectangle)
		{
			return
			{
				static_cast<LONG>(std::floor(rectangle.left)),
				static_cast<LONG>(std::floor(rectangle.top)),
				static_cast<LONG>(std::ceil(rectangle.right)),
				static_cast<LONG>(std::ceil(rectangle.bottom))
			};
		}
		FORCEINLINE [[nodiscard]] D2D1_RECT_F TransformRect(const D2D1_RECT_F& rectangle, const D2D1_MATRIX_3X2_F& matrix)
		{
			const auto matrixHelper = D2D1::Matrix3x2F::ReinterpretBaseType(&matrix);
			const auto topLeft = matrixHelper->TransformPoint(D2D1::Point2F(rectangle.left, rectangle.top));
			const auto topRight = matrixHelper->TransformPoint(D2D1::Point2F(rectangle.right, rectangle.top));
			const auto bottomLeft = matrixHelper->TransformPoint(D2D1::Point2F(rectangle.left, rectangle.bottom));
			const auto bottomRight = matrixHelper->TransformPoint(D2D1::Point2F(rectangle.right, rectangle.bottom));

			D2D1_RECT_F transformedRect{};
			transformedRect.left = std::min({ topLeft.x, topRight.x, bottomLeft.x, bottomRight.x });
			transformedRect.top = std::min({ topLeft.y, topRight.y, bottomLeft.y, bottomRight.y });
			transformedRect.right = std::max({ topLeft.x, topRight.x, bottomLeft.x, bottomRight.x });
			transformedRect.bottom = std::max({ topLeft.y, topRight.y, bottomLeft.y, bottomRight.y });

			return transformedRect;
		}
		FORCEINLINE constexpr bool DoesIntersectUnsafe(
			const D2D1_RECT_F& rc1,
			const D2D1_RECT_F& rc2
		)
		{
			if (
				!wil::rect_is_empty(rc1) &&
				!wil::rect_is_empty(rc2) &&

				rc1.right > rc2.left &&
				rc2.right > rc1.left &&
				rc1.bottom > rc2.top &&
				rc2.bottom > rc1.top
			)
			{
				return true;
			}
			return false;
		}
		inline constexpr bool IntersectUnsafe(
			D2D1_RECT_F& dst,
			const D2D1_RECT_F& src
		)
		{
			dst.left = std::max(dst.left, src.left);
			dst.top = std::max(dst.top, src.top);
			dst.right = std::min(dst.right, src.right);
			dst.bottom = std::min(dst.bottom, src.bottom);

			if (!wil::rect_is_empty(dst))
			{
				return true;
			}

			dst.left = dst.top = dst.right = dst.bottom = 0.f;
			return false;
		}
	}
}
