#pragma once
#include "framework.hpp"
#include "cpprt.hpp"
#include "Utils.hpp"
#include "dwmcoreProjection.hpp"
#include "OSHelper.hpp"
#include "HookHelper.hpp"

namespace OpenGlass::uDwm
{
	inline HMODULE g_moduleHandle{ GetModuleHandleW(L"uDwm.dll")};
	inline std::unordered_map<std::string, PVOID> g_symbolMap{};
	template <typename T>
	FORCEINLINE T GetAddressFromSymbolMap(std::string_view functionName)
	{
		auto it{ g_symbolMap.find(std::string{ functionName }) };
		return it != g_symbolMap.end() ? Utils::cast_pointer<T>(it->second) : nullptr;
	}
	template <typename T>
	FORCEINLINE void GetAddressFromSymbolMap(std::string_view functionName, T& target)
	{
		auto it{ g_symbolMap.find(std::string{ functionName })};
		if (it != g_symbolMap.end()) [[likely]]
		{
			target = Utils::cast_pointer<T>(it->second);
		}
#ifdef _DEBUG
		else
		{
			OutputDebugStringA(std::format("{} - symbol missing for {}\n", __FUNCTION__, functionName.data()).c_str());
		}
#endif // _DEBUG
	}

	struct CBaseObject
	{
		[[nodiscard]] void* operator new(size_t size) noexcept(false)
		{
			auto memory{ HeapAlloc(OpenGlass::Utils::g_processHeap, 0, size) };
			THROW_LAST_ERROR_IF_NULL(memory);
			return memory;
		}
		void operator delete(void* ptr) noexcept
		{
			FAIL_FAST_IF_NULL(ptr);
			HeapFree(OpenGlass::Utils::g_processHeap, 0, ptr);
			ptr = nullptr;
		}
		size_t AddRef()
		{
			return InterlockedIncrement(reinterpret_cast<DWORD*>(this) + 2);
		}
		size_t Release()
		{
			auto result{ InterlockedDecrement(reinterpret_cast<DWORD*>(this) + 2) };
			if (!result)
			{
				delete this;
			}
			return result;
		}
		HRESULT QueryInterface(REFIID /*riid*/, PVOID* ppvObject)
		{
			*ppvObject = this;
			return S_OK;
		}
	protected:
		virtual ~CBaseObject() {};
	};
	struct CResourceProxy
	{
		dwmcore::CChannel* GetChannel()
		{
			return *reinterpret_cast<dwmcore::CChannel**>(reinterpret_cast<ULONG_PTR>(this) + 16);
		}
		UINT GetHandleIndex()
		{
			return *reinterpret_cast<UINT*>(reinterpret_cast<ULONG_PTR>(this) + 24);
		}
	};
	struct CResource : CBaseObject 
	{
		CResourceProxy* GetProxy()
		{
			return reinterpret_cast<CResourceProxy**>(this)[2];
		}
	};
	struct CBaseLegacyMilBrushProxy : CResource {};
	struct CBaseGeometryProxy : CResource {};
	struct CBaseTransformProxy : CResource {};

	struct CCombinedGeometryProxy : CBaseGeometryProxy {};
	struct CRgnGeometryProxy : CBaseGeometryProxy
	{
		HRESULT STDMETHODCALLTYPE Update(LPCRECT rectangles, UINT count)
		{
			DEFINE_INVOKER(CRgnGeometryProxy::Update);
			return INVOKE_MEMBERFUNCTION(rectangles, count);
		}
	};

	struct CSolidColorLegacyMilBrushProxy : CBaseLegacyMilBrushProxy {};

	struct VisualCollection;
	struct CVisualProxy : CResource
	{
		HRESULT STDMETHODCALLTYPE SetClip(CBaseGeometryProxy* geometry) 
		{
			DEFINE_INVOKER(CVisualProxy::SetClip);
			return INVOKE_MEMBERFUNCTION(geometry);
		}
		HRESULT STDMETHODCALLTYPE SetEffect(CResourceProxy* effect)
		{
			DEFINE_INVOKER(CVisualProxy::SetEffect);
			return INVOKE_MEMBERFUNCTION(effect);
		}
	};

	struct CVisual : CBaseObject
	{
		LONG GetWidth() const
		{
			LONG width{ 0 };

			if (os::buildNumber < os::build_w11_21h2)
			{
				width = reinterpret_cast<LONG const*>(this)[30];
			}
			else
			{
				width = reinterpret_cast<LONG const*>(this)[32];
			}

			return width;
		}
		LONG GetHeight() const
		{
			LONG height{ 0 };

			if (os::buildNumber < os::build_w11_21h2)
			{
				height = reinterpret_cast<LONG const*>(this)[31];
			}
			else
			{
				height = reinterpret_cast<LONG const*>(this)[33];
			}

			return height;
		}
		MARGINS* GetMargins()
		{
			MARGINS* margins{ nullptr };

			if (os::buildNumber < os::build_w11_21h2)
			{
				margins = reinterpret_cast<MARGINS*>(this) + 8;
			}
			else
			{
				margins = reinterpret_cast<MARGINS*>(reinterpret_cast<ULONG_PTR>(this) + 136);
			}

			return margins;
		}
		int GetDirtyFlags() const
		{
			int flags{ 0 };

			if (os::buildNumber < os::build_w11_21h2)
			{
				flags = reinterpret_cast<int const*>(this)[20];
			}
			else
			{
				flags = reinterpret_cast<int const*>(this)[22];
			}

			return flags;
		}
		VisualCollection* GetVisualCollection() const
		{
			return const_cast<VisualCollection*>(reinterpret_cast<VisualCollection const*>(reinterpret_cast<const char*>(this) + 32));
		}
		CVisualProxy* GetVisualProxy() const
		{
			return reinterpret_cast<CVisualProxy* const*>(this)[2];
		}
		CVisual* GetParent() const
		{
			return reinterpret_cast<CVisual* const*>(this)[3];
		}

		bool IsCloneAllowed() const
		{
			const BYTE* properties{ nullptr };

			if (os::buildNumber < os::build_w11_21h2)
			{
				properties = &reinterpret_cast<BYTE const*>(this)[84];
			}
			else
			{
				properties = &reinterpret_cast<BYTE const*>(this)[92];
			}

			bool allowed{ true };
			if (properties)
			{
				allowed = (*properties & 8) == 0;
			}

			return allowed;
		}
		bool AllowVisualTreeClone(bool allow)
		{
			BYTE* properties{ nullptr };

			if (os::buildNumber < os::build_w11_21h2)
			{
				properties = &reinterpret_cast<BYTE*>(this)[84];
			}
			else
			{
				properties = &reinterpret_cast<BYTE*>(this)[92];
			}

			bool allowed{ false };
			if (properties)
			{
				allowed = (*properties & 8) == 0;
				if (allow)
				{
					*properties = *properties & ~8;
				}
				else
				{
					*properties |= 8;
				}
			}

			return allowed;
		}
		
		static HRESULT STDMETHODCALLTYPE Create(CVisual** visual)
		{
			DEFINE_INVOKER(CVisual::Create);
			return INVOKE_FUNCTION(visual);
		}
		void STDMETHODCALLTYPE SetDirtyFlags(int flags)
		{
			DEFINE_INVOKER(CVisual::SetDirtyFlags);
			return INVOKE_MEMBERFUNCTION(flags);
		}
		void STDMETHODCALLTYPE SetOpacity(double opacity)
		{
			DEFINE_INVOKER(CVisual::SetOpacity);
			return INVOKE_MEMBERFUNCTION(opacity);
		}
		HRESULT STDMETHODCALLTYPE UpdateOpacity()
		{
			DEFINE_INVOKER(CVisual::UpdateOpacity);
			return INVOKE_MEMBERFUNCTION();
		}
	};

	struct VisualCollection : CResource
	{
		HRESULT STDMETHODCALLTYPE RemoveAll() 
		{
			DEFINE_INVOKER(VisualCollection::RemoveAll);
			return INVOKE_MEMBERFUNCTION(); 
		}
		HRESULT STDMETHODCALLTYPE Remove(CVisual* visual) 
		{
			DEFINE_INVOKER(VisualCollection::Remove);
			return INVOKE_MEMBERFUNCTION(visual); 
		}
		HRESULT STDMETHODCALLTYPE InsertRelative(
			CVisual* visual,
			CVisual* referenceVisual,
			bool insertAfter,
			bool connectNow
		)
		{
			DEFINE_INVOKER(VisualCollection::InsertRelative);
			return INVOKE_MEMBERFUNCTION(visual, referenceVisual, insertAfter, connectNow);
		}
	};

	struct IRenderDataBuilder : IUnknown
	{
		STDMETHOD(DrawBitmap)(UINT bitmapHandleTableIndex) PURE;
		STDMETHOD(DrawGeometry)(UINT geometryHandleTableIndex, UINT brushHandleTableIndex) PURE;
		STDMETHOD(DrawImage)(const D2D1_RECT_F& rect, UINT imageHandleTableIndex) PURE;
		STDMETHOD(DrawMesh2D)(UINT meshHandleTableIndex, UINT brushHandleTableIndex) PURE;
		STDMETHOD(DrawRectangle)(const D2D1_RECT_F* rect, UINT brushHandleTableIndex) PURE;
		STDMETHOD(DrawTileImage)(UINT imageHandleTableIndex, const D2D1_RECT_F& rect, float opacity, const D2D1_POINT_2F& point) PURE;
		STDMETHOD(DrawVisual)(UINT visualHandleTableIndex) PURE;
		STDMETHOD(Pop)() PURE;
		STDMETHOD(PushTransform)(UINT transformHandleTableInfex) PURE;
		STDMETHOD(DrawSolidRectangle)(const D2D1_RECT_F& rect, const D2D1_COLOR_F& color) PURE;
	};
	struct CRenderDataInstruction : CResource
	{
		STDMETHOD(WriteInstruction)(
			IRenderDataBuilder* builder,
			const struct CVisual* visual
		) PURE;
	};
	struct CDrawGeometryInstruction : CRenderDataInstruction
	{
		static HRESULT STDMETHODCALLTYPE Create(CBaseLegacyMilBrushProxy* brush, CBaseGeometryProxy* geometry, CDrawGeometryInstruction** instruction)
		{
			DEFINE_INVOKER(CDrawGeometryInstruction::Create);
			return INVOKE_FUNCTION(brush, geometry, instruction);
		}
	};

	class CEmptyDrawInstruction : public CRenderDataInstruction
	{
		DWORD m_refCount{ 1 };
	public:
		STDMETHOD(WriteInstruction)(
			IRenderDataBuilder* /*builder*/,
			const struct CVisual* /*visual*/
			) override
		{
			return S_OK;
		}
	};
	class CDrawVisualTreeInstruction : public CRenderDataInstruction
	{
		DWORD m_refCount{ 1 };
		winrt::com_ptr<CVisual> m_visual{ nullptr };
	public:
		CDrawVisualTreeInstruction(CVisual* visual) : CRenderDataInstruction{} { m_visual.copy_from(visual); }
		STDMETHOD(WriteInstruction)(
			IRenderDataBuilder* builder,
			const struct CVisual* visual
			) override
		{
			UINT visualHandleTableIndex{ 0 };
			if (visual)
			{
				visualHandleTableIndex = visual->GetVisualProxy()->GetProxy()->GetHandleIndex();
			}

			return builder->DrawVisual(visualHandleTableIndex);
		}
	};
	struct CRenderDataVisual : CVisual
	{
		HRESULT STDMETHODCALLTYPE AddInstruction(CRenderDataInstruction* instruction)
		{
			DEFINE_INVOKER(CRenderDataVisual::AddInstruction);
			return INVOKE_MEMBERFUNCTION(instruction);
		}
		HRESULT STDMETHODCALLTYPE ClearInstructions()
		{
			DEFINE_INVOKER(CRenderDataVisual::ClearInstructions);
			return INVOKE_MEMBERFUNCTION();
		}
	};
	struct CCanvasVisual : CRenderDataVisual
	{
		static HRESULT STDMETHODCALLTYPE Create(CCanvasVisual** visual)
		{
			DEFINE_INVOKER(CCanvasVisual::Create);
			return INVOKE_FUNCTION(visual);
		}
	};
	struct CText : CRenderDataVisual 
	{
		bool IsRTL() const
		{
			return (reinterpret_cast<BYTE const*>(this)[280] & 2) != 0;
		}
	};

	struct ACCENT_POLICY
	{
		DWORD AccentState;
		DWORD AccentFlags;
		DWORD dwGradientColor;
		DWORD dwAnimationId;

		bool IsActive() const
		{
			return AccentState >= 1 && AccentState <= 4;
		}
		bool IsAccentBlurRectEnabled() const
		{
			return (AccentFlags & (1 << 9)) != 0;
		}
		bool IsGdiRegionRespected() const
		{
			return (AccentFlags & (1 << 4)) != 0;
		}
	};
	struct CAccent : CVisual
	{
	};

	struct IDwmWindow;
	struct CTopLevelWindow;
	struct CWindowData : CBaseObject
	{
		bool STDMETHODCALLTYPE IsWindowVisibleAndUncloaked()
		{
			DEFINE_INVOKER(CWindowData::IsWindowVisibleAndUncloaked);
			return INVOKE_MEMBERFUNCTION();
		}
		ULONG_PTR GetDesktopID() const
		{
			ULONG_PTR desktopID{ 0 };

			if (os::buildNumber < os::build_w11_21h2)
			{
				desktopID = reinterpret_cast<ULONG_PTR const*>(this)[15];
			} 
			else
			{
				desktopID = reinterpret_cast<ULONG_PTR const*>(this)[17];
			}

			return desktopID;
		}
		HWND GetHwnd() const
		{
			return reinterpret_cast<const HWND*>(this)[5];
		}
		IDwmWindow* GetWindowContext() const
		{
			return reinterpret_cast<IDwmWindow* const*>(this)[3];
		}
		CTopLevelWindow* GetWindow() const
		{
			CTopLevelWindow* window{ nullptr };

			if (os::buildNumber < os::build_w10_1903)
			{
				window = reinterpret_cast<CTopLevelWindow* const*>(this)[49];
			}
			else if (os::buildNumber < os::build_w10_2004)
			{
				window = reinterpret_cast<CTopLevelWindow* const*>(this)[50];
			}
			else if (os::buildNumber < os::build_w11_21h2)
			{
				window = reinterpret_cast<CTopLevelWindow* const*>(this)[48];
			}
			else
			{
				window = reinterpret_cast<CTopLevelWindow* const*>(this)[55];
			}

			return window;
		}

		ACCENT_POLICY* GetAccentPolicy() const
		{
			ACCENT_POLICY* accentPolicy{ nullptr };

			if (os::buildNumber < os::build_w11_21h2)
			{
				accentPolicy = reinterpret_cast<ACCENT_POLICY*>(reinterpret_cast<ULONG_PTR>(this) + 152);
			}
			else
			{
				accentPolicy = reinterpret_cast<ACCENT_POLICY*>(reinterpret_cast<ULONG_PTR>(this) + 168);
			}

			return accentPolicy;
		}

		bool IsUsingDarkMode() const
		{
			bool darkMode{ false };

			if (os::buildNumber < os::build_w10_1903)
			{
				darkMode = (reinterpret_cast<BYTE const*>(this)[601] & 8) != 0;
			}
			else if (os::buildNumber < os::build_w10_2004)
			{
				darkMode = (reinterpret_cast<BYTE const*>(this)[609] & 8) != 0;
			}
			else if (os::buildNumber < os::build_w11_21h2)
			{
				darkMode = (reinterpret_cast<BYTE const*>(this)[613] & 8) != 0;
			}
			else if (os::buildNumber < os::build_w11_22h2)
			{
				darkMode = (reinterpret_cast<BYTE const*>(this)[669] & 4) != 0;
			}
			else
			{
				darkMode = (reinterpret_cast<BYTE const*>(this)[677] & 4) != 0;
			}

			return darkMode;
		}
		DWORD GetNonClientAttribute() const
		{
			DWORD attribute{ 0 };

			if (os::buildNumber < os::build_w10_1903)
			{
				attribute = *reinterpret_cast<const DWORD*>(reinterpret_cast<BYTE const*>(this) + 596);
			}
			else if (os::buildNumber < os::build_w10_2004)
			{
				attribute = *reinterpret_cast<const DWORD*>(reinterpret_cast<BYTE const*>(this) + 604);
			}
			else if (os::buildNumber < os::build_w11_21h2)
			{
				attribute = *reinterpret_cast<const DWORD*>(reinterpret_cast<BYTE const*>(this) + 608);
			}
			else if (os::buildNumber < os::build_w11_22h2)
			{
				attribute = *reinterpret_cast<const DWORD*>(reinterpret_cast<BYTE const*>(this) + 664);
			}
			else
			{
				attribute = *reinterpret_cast<const DWORD*>(reinterpret_cast<BYTE const*>(this) + 672);
			}

			return attribute;
		}
		const MARGINS* GetExtendedFrameMargins() const
		{
			const MARGINS* margins{ nullptr };

			if (os::buildNumber < os::build_w11_21h2)
			{
				margins = reinterpret_cast<const MARGINS*>(reinterpret_cast<ULONG_PTR>(this) + 80);
			}
			else
			{
				margins = reinterpret_cast<const MARGINS*>(reinterpret_cast<ULONG_PTR>(this) + 96);
			}

			return margins;
		}
		bool IsFrameExtendedIntoClientAreaLRB() const
		{
			const MARGINS* margins{ GetExtendedFrameMargins() };

			return margins->cxLeftWidth || margins->cxRightWidth || margins->cyBottomHeight;
		}
		bool IsFullGlass() const
		{
			const MARGINS* margins{ GetExtendedFrameMargins() };

			return margins->cxLeftWidth == -1 || margins->cxRightWidth == -1 || margins->cyTopHeight == -1 || margins->cyBottomHeight == -1;
		}
	};
	struct CTopLevelWindow : CVisual
	{
		CRgnGeometryProxy* GetBorderGeometry() const
		{
			CRgnGeometryProxy* geometry{ nullptr };

			if (os::buildNumber < os::build_w10_2004)
			{
				geometry = reinterpret_cast<CRgnGeometryProxy* const*>(this)[68];
			}
			else if (os::buildNumber < os::build_w11_21h2)
			{
				geometry = reinterpret_cast<CRgnGeometryProxy* const*>(this)[69];
			}
			else if (os::buildNumber < os::build_w11_22h2)
			{
				geometry = reinterpret_cast<CRgnGeometryProxy* const*>(this)[71];
			}
			else
			{
				auto legacyBackgroundVisual{ reinterpret_cast<CVisual* const*>(this)[39] };
				if (legacyBackgroundVisual)
				{
					geometry = reinterpret_cast<CRgnGeometryProxy* const*>(legacyBackgroundVisual)[40];
				}
			}

			return geometry;
		}
		CRgnGeometryProxy* GetCaptionGeometry() const
		{
			CRgnGeometryProxy* geometry{ nullptr };

			if (os::buildNumber < os::build_w10_2004)
			{
				geometry = reinterpret_cast<CRgnGeometryProxy* const*>(this)[69];
			}
			else if (os::buildNumber < os::build_w11_21h2)
			{
				geometry = reinterpret_cast<CRgnGeometryProxy* const*>(this)[70];
			}
			else if (os::buildNumber < os::build_w11_22h2)
			{
				geometry = reinterpret_cast<CRgnGeometryProxy* const*>(this)[72];
			}
			else
			{
				auto legacyBackgroundVisual{ reinterpret_cast<CVisual* const*>(this)[39] };
				if (legacyBackgroundVisual)
				{
					geometry = reinterpret_cast<CRgnGeometryProxy* const*>(legacyBackgroundVisual)[39];
				}
			}

			return geometry;
		}
		void GetBorderMargins(MARGINS* margins) const
		{
			DEFINE_INVOKER(CTopLevelWindow::GetBorderMargins);
			return INVOKE_MEMBERFUNCTION(margins);
		}
		bool STDMETHODCALLTYPE TreatAsActiveWindow()
		{
			DEFINE_INVOKER(CTopLevelWindow::TreatAsActiveWindow);
			return INVOKE_MEMBERFUNCTION();
		}
		HRESULT STDMETHODCALLTYPE ValidateVisual()
		{
			DEFINE_INVOKER(CTopLevelWindow::ValidateVisual);
			return INVOKE_MEMBERFUNCTION();
		}
		HRESULT STDMETHODCALLTYPE OnClipUpdated()
		{
			DEFINE_INVOKER(CTopLevelWindow::OnClipUpdated);
			return INVOKE_MEMBERFUNCTION();
		}
		HRESULT STDMETHODCALLTYPE OnBlurBehindUpdated()
		{
			DEFINE_INVOKER(CTopLevelWindow::OnBlurBehindUpdated);
			return INVOKE_MEMBERFUNCTION();
		}
		HRESULT STDMETHODCALLTYPE OnAccentPolicyUpdated()
		{
			DEFINE_INVOKER(CTopLevelWindow::OnAccentPolicyUpdated);
			return INVOKE_MEMBERFUNCTION();
		}
		HRESULT STDMETHODCALLTYPE OnSystemBackdropUpdated()
		{
			DEFINE_INVOKER(CTopLevelWindow::OnSystemBackdropUpdated);
			return INVOKE_MEMBERFUNCTION();
		}
		RECT* STDMETHODCALLTYPE GetActualWindowRect(
			RECT* rect,
			char eraseOffset,
			char includeNonClient,
			bool excludeBorderMargins
		) const
		{
			DEFINE_INVOKER(CTopLevelWindow::GetActualWindowRect);
			return INVOKE_MEMBERFUNCTION(rect, eraseOffset, includeNonClient, excludeBorderMargins);
		}
		CWindowData* GetData() const
		{
			CWindowData* windowData{ nullptr };

			if (os::buildNumber < os::build_w10_2004)
			{
				windowData = reinterpret_cast<CWindowData* const*>(this)[90];
			}
			else if (os::buildNumber < os::build_w11_21h2)
			{
				windowData = reinterpret_cast<CWindowData* const*>(this)[91];
			}
			else
			{
				windowData = reinterpret_cast<CWindowData* const*>(this)[94];
			}

			return windowData;
		}
		CText* GetTextVisual() const
		{
			CText* visual{ nullptr };

			if (os::buildNumber < os::build_w10_2004)
			{
				visual = reinterpret_cast<CText* const*>(this)[64];
			}
			else if (os::buildNumber < os::build_w11_21h2)
			{
				visual = reinterpret_cast<CText* const*>(this)[65];
			}
			else if (os::buildNumber < os::build_w11_22h2)
			{
				visual = reinterpret_cast<CText* const*>(this)[67];
			}

			return visual;
		}
		CCanvasVisual* GetNonClientVisual() const
		{
			CCanvasVisual* visual{ nullptr };

			if (os::buildNumber < os::build_w10_2004)
			{
				visual = reinterpret_cast<CCanvasVisual* const*>(this)[32];
			}
			else if (os::buildNumber < os::build_w11_21h2)
			{
				visual = reinterpret_cast<CCanvasVisual* const*>(this)[33];
			}
			else if (os::buildNumber < os::build_w11_22h2)
			{
				visual = reinterpret_cast<CCanvasVisual* const*>(this)[34];
			}
			else
			{
				visual = reinterpret_cast<CCanvasVisual* const*>(this)[36];
			}

			return visual;
		}
		CVisual* GetClientAreaContainerVisual() const
		{
			CVisual* visual{ nullptr };

			if (os::buildNumber < os::build_w10_2004)
			{
				visual = reinterpret_cast<CVisual* const*>(this)[66];
			}
			else if (os::buildNumber < os::build_w11_21h2)
			{
				visual = reinterpret_cast<CVisual* const*>(this)[67];
			}
			else if (os::buildNumber < os::build_w11_22h2)
			{
				visual = reinterpret_cast<CVisual* const*>(this)[69];
			}
			else
			{
				visual = reinterpret_cast<CVisual* const*>(this)[73];
			}

			return visual;
		}
		CVisual* GetClientAreaContainerParentVisual() const
		{
			CVisual* visual{ nullptr };

			if (os::buildNumber < os::build_w10_2004)
			{
				visual = reinterpret_cast<CVisual* const*>(this)[67];
			}
			else if (os::buildNumber < os::build_w11_21h2)
			{
				visual = reinterpret_cast<CVisual* const*>(this)[68];
			}
			else if (os::buildNumber < os::build_w11_22h2)
			{
				visual = reinterpret_cast<CVisual* const*>(this)[70];
			}
			else
			{
				visual = reinterpret_cast<CVisual* const*>(this)[74];
			}

			return visual;
		}
		CAccent* GetAccent() const
		{
			CAccent* accent{ nullptr };

			if (os::buildNumber < os::build_w10_2004)
			{
				accent = reinterpret_cast<CAccent* const*>(this)[33];
			}
			else if (os::buildNumber < os::build_w11_21h2)
			{
				accent = reinterpret_cast<CAccent* const*>(this)[34];
			}
			else if (os::buildNumber < os::build_w11_22h2)
			{
				accent = reinterpret_cast<CAccent* const*>(this)[35];
			}
			else
			{
				accent = reinterpret_cast<CAccent* const*>(this)[37];
			}

			return accent;
		}
		CCanvasVisual* GetLegacyVisual() const
		{
			CCanvasVisual* visual{ nullptr };

			if (os::buildNumber < os::build_w10_2004)
			{
				visual = reinterpret_cast<CCanvasVisual* const*>(this)[35];
			}
			else if (os::buildNumber < os::build_w11_21h2)
			{
				visual = reinterpret_cast<CCanvasVisual* const*>(this)[36];
			}
			else if (os::buildNumber < os::build_w11_22h2)
			{
				visual = reinterpret_cast<CCanvasVisual* const*>(this)[37];
			}
			else
			{
				visual = reinterpret_cast<CCanvasVisual* const*>(this)[39];
			}

			return visual;
		}
		CCanvasVisual* GetClientBlurVisual() const
		{
			CCanvasVisual* visual{ nullptr };

			if (os::buildNumber < os::build_w10_2004)
			{
				visual = reinterpret_cast<CCanvasVisual* const*>(this)[36];
			}
			else if (os::buildNumber < os::build_w11_21h2)
			{
				visual = reinterpret_cast<CCanvasVisual* const*>(this)[37];
			}
			else if (os::buildNumber < os::build_w11_22h2)
			{
				visual = reinterpret_cast<CCanvasVisual* const*>(this)[39];
			}
			else
			{
				visual = reinterpret_cast<CCanvasVisual* const*>(this)[42];
			}

			return visual;
		}
		CVisual* GetSystemBackdropVisual() const
		{
			CVisual* visual{ nullptr };

			if (os::buildNumber < os::build_w11_21h2)
			{
			}
			else if (os::buildNumber < os::build_w11_22h2)
			{
				visual = reinterpret_cast<CVisual* const*>(this)[38];
			}
			else
			{
				visual = reinterpret_cast<CVisual* const*>(this)[40];
			}

			return visual;
		}
		CCanvasVisual* GetAccentColorVisual() const
		{
			CCanvasVisual* visual{ nullptr };

			if (os::buildNumber < os::build_w11_21h2)
			{
			}
			else if (os::buildNumber < os::build_w11_22h2)
			{
			}
			else
			{
				visual = reinterpret_cast<CCanvasVisual* const*>(this)[41];
			}

			return visual;
		}
		bool IsRTLMirrored() const
		{
			bool rtlMirrored{ false };
			
			if (os::buildNumber < os::build_w10_2004)
			{
				rtlMirrored = (reinterpret_cast<DWORD const*>(this)[146] & 0x20000) != 0;
			}
			else if (os::buildNumber < os::build_w11_21h2)
			{
				rtlMirrored = (reinterpret_cast<DWORD const*>(this)[148] & 0x20000) != 0;
			}
			else if (os::buildNumber < os::build_w11_22h2)
			{
				rtlMirrored = (reinterpret_cast<DWORD const*>(this)[152] & 0x20000) != 0;
			}
			else
			{
				rtlMirrored = (reinterpret_cast<DWORD const*>(this)[156] & 0x20000) != 0;
			}

			return rtlMirrored;
		}
		bool HasNonClientBackground(CWindowData* data = nullptr) const
		{
			if (!data)
			{
				data = GetData();
			}
			if ((data->GetNonClientAttribute() & 8) == 0)
			{
				return false;
			}

			bool nonClientEmpty{ false };
			if (os::buildNumber < os::build_w10_2004)
			{
				nonClientEmpty = !reinterpret_cast<DWORD const*>(this)[151] &&
					!reinterpret_cast<DWORD const*>(this)[152] &&
					!reinterpret_cast<DWORD const*>(this)[153] &&
					!reinterpret_cast<DWORD const*>(this)[154];
			}
			else if (os::buildNumber < os::build_w11_21h2)
			{
				nonClientEmpty = !reinterpret_cast<DWORD const*>(this)[153] &&
					!reinterpret_cast<DWORD const*>(this)[154] &&
					!reinterpret_cast<DWORD const*>(this)[155] &&
					!reinterpret_cast<DWORD const*>(this)[156];
			}
			else if (os::buildNumber < os::build_w11_22h2)
			{
				nonClientEmpty = !reinterpret_cast<DWORD const*>(this)[157] &&
					!reinterpret_cast<DWORD const*>(this)[158] &&
					!reinterpret_cast<DWORD const*>(this)[159] &&
					!reinterpret_cast<DWORD const*>(this)[160];
			}
			else
			{
				nonClientEmpty = !reinterpret_cast<DWORD const*>(this)[161] &&
					!reinterpret_cast<DWORD const*>(this)[162] &&
					!reinterpret_cast<DWORD const*>(this)[163] &&
					!reinterpret_cast<DWORD const*>(this)[164];
			}

			if (nonClientEmpty)
			{
				return false;
			}

			return true;
		}
		bool IsTrullyMinimized()
		{
			RECT borderRect{};
			THROW_HR_IF_NULL(E_INVALIDARG, GetActualWindowRect(&borderRect, false, true, false));

			return borderRect.left <= -32000 || borderRect.top <= -32000;
		}

		DWORD STDMETHODCALLTYPE GetSolidColorCaptionColor() const
		{
			DEFINE_INVOKER(CTopLevelWindow::GetSolidColorCaptionColor);
			return INVOKE_MEMBERFUNCTION();
		}
		DWORD STDMETHODCALLTYPE GetWindowColorizationColor(BYTE flags) const
		{
			DEFINE_INVOKER(CTopLevelWindow::GetWindowColorizationColor);
			return INVOKE_MEMBERFUNCTION(flags);
		}
		DWORD* STDMETHODCALLTYPE GetCurrentDefaultColorizationFlags(DWORD* flags) const
		{
			DEFINE_INVOKER(CTopLevelWindow::GetCurrentDefaultColorizationFlags);
			return INVOKE_MEMBERFUNCTION(flags);
		}
		DWORD GetCurrentColorizationColor() const
		{
			DWORD color{};

			if (os::buildNumber < os::build_w11_22h2)
			{
				DWORD flags{};
				color = GetWindowColorizationColor(static_cast<BYTE>(*GetCurrentDefaultColorizationFlags(&flags) | 8u));
			}
			else
			{
				color = GetSolidColorCaptionColor();
			}

			return color;
		}
	};
	struct CWindowList : CBaseObject
	{
		PRLIST_ENTRY STDMETHODCALLTYPE GetWindowListForDesktop(ULONG_PTR desktopID)
		{
			DEFINE_INVOKER(CWindowList::GetWindowListForDesktop);
			return INVOKE_MEMBERFUNCTION(desktopID);
		}
		HWND STDMETHODCALLTYPE GetShellWindowForDesktop(ULONG_PTR desktopID)
		{
			DEFINE_INVOKER(CWindowList::GetShellWindowForDesktop);
			return INVOKE_MEMBERFUNCTION(desktopID);
		}
		CRenderDataVisual* STDMETHODCALLTYPE GetRootVisualForDesktop(ULONG_PTR desktopID)
		{
			DEFINE_INVOKER(CWindowList::GetRootVisualForDesktop);
			return INVOKE_MEMBERFUNCTION(desktopID);
		}
		HRESULT STDMETHODCALLTYPE GetSyncedWindowDataByHwnd(HWND hwnd, CWindowData** windowData)
		{
			DEFINE_INVOKER(CWindowList::GetSyncedWindowDataByHwnd);
			return INVOKE_MEMBERFUNCTION(hwnd, windowData);
		}
	};

	struct CCompositor
	{
		HRESULT CreateVisualProxyFromSharedHandle(HANDLE handle, CVisualProxy** visualProxy)
		{
			DEFINE_USER_INVOKER(CCompositor::CreateVisualProxyFromSharedHandle, "CCompositor::CreateProxyFromSharedHandle<CVisualProxy>");
			return INVOKE_MEMBERFUNCTION(handle, visualProxy);
		}
		dwmcore::CChannel* GetChannel() const
		{
			dwmcore::CChannel* channel{ nullptr };

			if (os::buildNumber < os::build_w10_1903)
			{
				channel = reinterpret_cast<dwmcore::CChannel*>(const_cast<CCompositor*>(this));
			}
			else if (os::buildNumber < os::build_w10_2004)
			{
				channel = reinterpret_cast<dwmcore::CChannel* const*>(this)[2];
			}

			return channel;
		}
	};
	struct CDesktopManager
	{
		inline static CDesktopManager* s_pDesktopManagerInstance{ nullptr };
		inline static LPCRITICAL_SECTION s_csDwmInstance{ nullptr };

		// flags & 
		// 0x1 -> update system metrics
		// 0x4 -> indicate high contrast is disabled
		HRESULT STDMETHODCALLTYPE UpdateSettings(USHORT flags)
		{
			DEFINE_INVOKER(CDesktopManager::UpdateSettings);
			return INVOKE_MEMBERFUNCTION(flags);
		}
		bool IsWindowAnimationEnabled() const
		{
			return reinterpret_cast<bool const*>(this)[22];
		}
		CCompositor* GetCompositor() const
		{
			CCompositor* compositor{ nullptr };

			if (os::buildNumber < os::build_w11_21h2)
			{
				compositor = reinterpret_cast<CCompositor* const*>(this)[5];
			}
			else
			{
				compositor = reinterpret_cast<CCompositor* const*>(this)[6];
			}

			return compositor;
		}
		CWindowList* GetWindowList() const
		{
			CWindowList* windowList{ nullptr };
			if (os::buildNumber < os::build_w11_21h2)
			{
				windowList = reinterpret_cast<CWindowList* const*>(this)[61];
			}
			else if (os::buildNumber < os::build_w11_22h2)
			{
				windowList = reinterpret_cast<CWindowList* const*>(this)[52];
			}
			else
			{
				windowList = reinterpret_cast<CWindowList* const*>(this)[54];
			}
			return windowList;
		}
		IWICImagingFactory2* GetWICFactory() const
		{
			IWICImagingFactory2* factory{ nullptr };

			if (os::buildNumber < os::build_w11_21h2)
			{
				factory = reinterpret_cast<IWICImagingFactory2* const*>(this)[39];
			}
			else if (os::buildNumber < os::build_w11_22h2)
			{
				factory = reinterpret_cast<IWICImagingFactory2* const*>(this)[30];
			}
			else
			{
				factory = reinterpret_cast<IWICImagingFactory2* const*>(this)[31];
			}

			return factory;
		}
		ID2D1Device* GetD2DDevice() const
		{
			ID2D1Device* d2dDevice{ nullptr };

			if (os::buildNumber < os::build_w11_21h2)
			{
				d2dDevice = reinterpret_cast<ID2D1Device* const*>(this)[29];
			}
			else if (os::buildNumber < os::build_w11_22h2)
			{
				d2dDevice = reinterpret_cast<ID2D1Device**>(reinterpret_cast<void* const*>(this)[6])[3];
			}
			else
			{
				d2dDevice = reinterpret_cast<ID2D1Device**>(reinterpret_cast<void* const*>(this)[7])[3];
			}

			return d2dDevice;
		}
	};
	FORCEINLINE HWND GetShellWindowForCurrentDesktop()
	{
		ULONG_PTR desktopID{ 0 };
		Utils::GetDesktopID(1, &desktopID);

		return CDesktopManager::s_pDesktopManagerInstance->GetWindowList()->GetShellWindowForDesktop(desktopID);
	}

	namespace ResourceHelper
	{
		FORCEINLINE HRESULT STDMETHODCALLTYPE CreateGeometryFromHRGN(
			HRGN hrgn,
			CRgnGeometryProxy** geometry
		)
		{
			DEFINE_INVOKER(ResourceHelper::CreateGeometryFromHRGN);
			return INVOKE_FUNCTION(hrgn, geometry);
		}
	}

	inline bool OnSymbolParsing(std::string_view functionName, std::string_view fullyUnDecoratedFunctionName, const HookHelper::OffsetStorage& offset, const PSYMBOL_INFO /*originalSymInfo*/)
	{
		if (
			fullyUnDecoratedFunctionName.starts_with("CCompositor::") ||
			fullyUnDecoratedFunctionName.starts_with("CText::") ||
			fullyUnDecoratedFunctionName.starts_with("CVisual::") ||
			fullyUnDecoratedFunctionName.starts_with("CVisualProxy::") ||
			fullyUnDecoratedFunctionName.starts_with("CCanvasVisual::") ||
			fullyUnDecoratedFunctionName.starts_with("CRenderDataVisual::") ||
			fullyUnDecoratedFunctionName.starts_with("CRgnGeometryProxy::") ||
			fullyUnDecoratedFunctionName.starts_with("CMatrixTransformProxy::") ||
			fullyUnDecoratedFunctionName.starts_with("CTopLevelWindow::") ||
			fullyUnDecoratedFunctionName.starts_with("CWindowData::") ||
			fullyUnDecoratedFunctionName.starts_with("VisualCollection::") ||
			fullyUnDecoratedFunctionName.starts_with("CDesktopManager::") ||
			fullyUnDecoratedFunctionName == "CWindowList::UpdateAccentBlurRect" ||
			fullyUnDecoratedFunctionName == "CWindowList::GetSyncedWindowDataByHwnd" ||
			fullyUnDecoratedFunctionName == "CWindowList::GetWindowListForDesktop" ||
			fullyUnDecoratedFunctionName == "CWindowList::GetRootVisualForDesktop" ||
			fullyUnDecoratedFunctionName == "CWindowList::GetShellWindowForDesktop" ||
			fullyUnDecoratedFunctionName == "CDrawGeometryInstruction::Create" ||
			(fullyUnDecoratedFunctionName.starts_with("ResourceHelper::") && fullyUnDecoratedFunctionName != "ResourceHelper::CreateRectangleGeometry") ||
			functionName == "?CreateRectangleGeometry@ResourceHelper@@SAJPEBUtagRECT@@PEAPEAVCRectangleGeometryProxy@@@Z" ||
			functionName == "?CreateRectangleGeometry@ResourceHelper@@SAJPEBUtagRECT@@PEAPEAVCResource@@@Z"
		)
		{
			g_symbolMap.insert_or_assign(
				std::string{ fullyUnDecoratedFunctionName },
				offset.To(g_moduleHandle)
			);
		}
		if (fullyUnDecoratedFunctionName == "CDesktopManager::s_pDesktopManagerInstance")
		{
			CDesktopManager::s_pDesktopManagerInstance = *offset.To<CDesktopManager**>(g_moduleHandle);
		}
		if (fullyUnDecoratedFunctionName == "CDesktopManager::s_csDwmInstance")
		{
			offset.To(g_moduleHandle, CDesktopManager::s_csDwmInstance);
		}

		return true;
	}
}