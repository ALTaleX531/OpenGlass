#pragma once
#include "framework.hpp"
#include "cpprt.hpp"
#include "winrt.hpp"
#include "Utils.hpp"
#include "dcompProjection.hpp"
#include "OSHelper.hpp"
#include "HookHelper.hpp"

namespace OpenGlass::uDwm
{
	inline HANDLE g_hProcessHeap{ GetProcessHeap() };
	inline HMODULE g_moduleHandle{ GetModuleHandleW(L"uDwm.dll")};
	inline std::unordered_map<std::string, PVOID> g_offsetMap{};

	struct CBaseObject
	{
		void* operator new(size_t size)
		{
			return HeapAlloc(g_hProcessHeap, 0, size);
		}
		void operator delete(void* ptr)
		{
			if (ptr)
			{
				HeapFree(g_hProcessHeap, 0, ptr);
				ptr = nullptr;
			}
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
		HRESULT QueryInterface(REFIID riid, PVOID* ppvObject)
		{
			*ppvObject = this;
			return S_OK;
		}
	protected:
		virtual ~CBaseObject() {};
	};
	struct CResourceProxy : CBaseObject {};
	struct CBaseLegacyMilBrushProxy : CBaseObject {};
	struct CBaseGeometryProxy : CBaseObject {};
	struct CBaseTransformProxy : CBaseObject {};

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
	struct CVisualProxy : CBaseObject
	{
		HRESULT STDMETHODCALLTYPE SetClip(CBaseGeometryProxy* geometry) 
		{
			DEFINE_INVOKER(CVisualProxy::SetClip);
			return INVOKE_MEMBERFUNCTION(geometry);
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
		
		HRESULT STDMETHODCALLTYPE InitializeFromSharedHandle(HANDLE handle)
		{
			DEFINE_INVOKER(CVisual::InitializeFromSharedHandle);
			return INVOKE_MEMBERFUNCTION(handle);
		}
		static HRESULT STDMETHODCALLTYPE CreateFromSharedHandle(HANDLE handle, CVisual** visual)
		{
			DEFINE_INVOKER(CVisual::CreateFromSharedHandle);
			return INVOKE_FUNCTION(handle, visual);
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

	struct VisualCollection : CBaseObject
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
	struct CRenderDataInstruction : CBaseObject
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
			IRenderDataBuilder* builder,
			const struct CVisual* visual
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
		HRESULT STDMETHODCALLTYPE Initialize() { return S_OK; }
		STDMETHOD(WriteInstruction)(
			IRenderDataBuilder* builder,
			const struct CVisual* visual
			) override
		{
			UINT visualHandleTableIndex{ 0 };
			auto visualProxy{ m_visual->GetVisualProxy() };
			if (visualProxy)
			{
				visualHandleTableIndex = *reinterpret_cast<UINT*>(
					*reinterpret_cast<ULONG_PTR*>(reinterpret_cast<ULONG_PTR>(visualProxy) + 16) + 24ull
					);
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
	struct CText : CRenderDataVisual {};

	struct ACCENT_POLICY
	{
		DWORD AccentState;
		DWORD AccentFlags;
		DWORD dwGradientColor;
		DWORD dwAnimationId;

		bool IsActive() const
		{
			return AccentState >= 3 && AccentState < 5;
		}
		bool IsClipEnabled() const
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
		CBaseGeometryProxy* const& GetClipGeometry() const
		{
			CBaseGeometryProxy* const* geometry{ nullptr };

			if (os::buildNumber < os::build_w11_21h2)
			{
				geometry = &reinterpret_cast<CBaseGeometryProxy* const*>(this)[52];
			}
			else if (os::buildNumber < os::build_w11_22h2)
			{
				geometry = &reinterpret_cast<CBaseGeometryProxy* const*>(this)[53];
			}
			else
			{
				geometry = &reinterpret_cast<CBaseGeometryProxy* const*>(this)[48];
			}

			return *geometry;
		}
	};

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
		CTopLevelWindow* GetWindow() const
		{
			CTopLevelWindow* window{ nullptr };

			if (os::buildNumber < os::build_w11_21h2)
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

			if (os::buildNumber < os::build_w11_21h2)
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

			if (os::buildNumber < os::build_w11_21h2)
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
	};
	struct CTopLevelWindow : CVisual
	{
		CRgnGeometryProxy* GetBorderGeometry() const
		{
			CRgnGeometryProxy* geometry{ nullptr };

			if (os::buildNumber < os::build_w11_21h2)
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

			if (os::buildNumber < os::build_w11_21h2)
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

			if (os::buildNumber < os::build_w11_21h2)
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

			if (os::buildNumber < os::build_w11_21h2)
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
		CVisual* GetTopLevelAtlasedRectsVisual() const
		{
			CVisual* visual{ nullptr };

			if (os::buildNumber < os::build_w11_21h2)
			{
				visual = reinterpret_cast<CVisual* const*>(this)[35];
			}
			else if (os::buildNumber < os::build_w11_22h2)
			{
				visual = reinterpret_cast<CVisual* const*>(this)[36];
			}
			else
			{
				visual = reinterpret_cast<CVisual* const*>(this)[38];
			}

			return visual;
		}
		CVisual* GetClientAreaContainerVisual() const
		{
			CVisual* visual{ nullptr };

			if (os::buildNumber < os::build_w11_21h2)
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

			if (os::buildNumber < os::build_w11_21h2)
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

			if (os::buildNumber < os::build_w11_21h2)
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

			if (os::buildNumber < os::build_w11_21h2)
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

			if (os::buildNumber < os::build_w11_21h2)
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
			if (os::buildNumber < os::build_w11_21h2)
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

		enum class BackgroundType
		{
			Legacy,
			Accent,
			SystemBackdrop_BackdropMaterial,
			SystemBackdrop_CaptionAccentColor,
			SystemBackdrop_Default
		};
		BackgroundType GetBackgroundType()
		{
			auto backgroundType{ BackgroundType::Legacy };
			auto windowData{ GetData() };

			if (os::buildNumber < os::build_w11_21h2)
			{
				backgroundType = windowData ?
					(windowData->GetAccentPolicy()->IsActive() ? BackgroundType::Accent : BackgroundType::Legacy) :
					BackgroundType::Legacy;
			}
			else if (os::buildNumber < os::build_w11_22h2)
			{
				backgroundType = *reinterpret_cast<DWORD*>(reinterpret_cast<ULONG_PTR>(GetData()) + 204) ?
					BackgroundType::SystemBackdrop_BackdropMaterial :
					(
						windowData ?
						(windowData->GetAccentPolicy()->IsActive() ? BackgroundType::Accent : BackgroundType::Legacy) :
						BackgroundType::Legacy
						);
			}
			else
			{
				backgroundType = reinterpret_cast<BackgroundType const*>(this)[210];
			}

			return backgroundType;
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
		HRESULT STDMETHODCALLTYPE ShowHide(CWindowData* data, bool updateReplacement)
		{
			DEFINE_INVOKER(CWindowList::ShowHide);
			return INVOKE_MEMBERFUNCTION(data, updateReplacement);
		}
	};

	struct CDesktopManager
	{
		inline static CDesktopManager* s_pDesktopManagerInstance{ nullptr };
		inline static LPCRITICAL_SECTION s_csDwmInstance{ nullptr };

		bool IsWindowMaximized() const
		{
			return reinterpret_cast<bool const*>(this)[21];
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
		dcomp::IDCompositionDesktopDevicePartner* GetDCompDevice() const
		{
			dcomp::IDCompositionDesktopDevicePartner* interopDevice{ nullptr };

			if (os::buildNumber < os::build_w11_21h2)
			{
				interopDevice = reinterpret_cast<dcomp::IDCompositionDesktopDevicePartner* const*>(this)[27];
			}
			else if (os::buildNumber < os::build_w11_22h2)
			{
				interopDevice = reinterpret_cast<dcomp::IDCompositionDesktopDevicePartner**>(reinterpret_cast<void* const*>(this)[5])[4];
			}
			else
			{
				interopDevice = reinterpret_cast<dcomp::IDCompositionDesktopDevicePartner**>(reinterpret_cast<void* const*>(this)[6])[4];
			}

			return interopDevice;
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
			uDwm::CRgnGeometryProxy** geometry
		)
		{
			DEFINE_INVOKER(ResourceHelper::CreateGeometryFromHRGN);
			return INVOKE_FUNCTION(hrgn, geometry);
		}
	}

	inline void InitializeFromSymbol(std::string_view fullyUnDecoratedFunctionName, const HookHelper::OffsetStorage& offset)
	{
		if (
			fullyUnDecoratedFunctionName.starts_with("CVisual::") ||
			fullyUnDecoratedFunctionName.starts_with("CVisualProxy::") ||
			fullyUnDecoratedFunctionName.starts_with("CCanvasVisual::") ||
			fullyUnDecoratedFunctionName.starts_with("CRenderDataVisual::") ||
			fullyUnDecoratedFunctionName.starts_with("CRgnGeometryProxy::") ||
			fullyUnDecoratedFunctionName.starts_with("CTopLevelWindow::") ||
			fullyUnDecoratedFunctionName.starts_with("CWindowData::") ||
			fullyUnDecoratedFunctionName.starts_with("ResourceHelper::") ||
			fullyUnDecoratedFunctionName.starts_with("VisualCollection::") ||
			fullyUnDecoratedFunctionName == "CWindowList::GetSyncedWindowDataByHwnd" ||
			fullyUnDecoratedFunctionName == "CWindowList::GetWindowListForDesktop" ||
			fullyUnDecoratedFunctionName == "CWindowList::GetRootVisualForDesktop" ||
			fullyUnDecoratedFunctionName == "CWindowList::GetShellWindowForDesktop"
		)
		{
			g_offsetMap.insert_or_assign(
				std::string{ fullyUnDecoratedFunctionName },
				offset.To(g_moduleHandle)
			);
		}
		if (fullyUnDecoratedFunctionName == "CWindowList::ShowHide" && g_offsetMap.find("CWindowList::ShowHide") == g_offsetMap.end())
		{
			auto functionAddress{ offset.To<UCHAR*>(g_moduleHandle) };
			// CWindowList::ShowHide(CWindowList *__hidden this, struct IDwmWindow *)
			// mov     [rsp+arg_0], rbx
			// push    rbp
			if (functionAddress[5] != 0x55)
			{
				g_offsetMap.insert_or_assign(
					std::string{ fullyUnDecoratedFunctionName },
					functionAddress
				);
			}
		}
		if (fullyUnDecoratedFunctionName == "CDesktopManager::s_pDesktopManagerInstance")
		{
			CDesktopManager::s_pDesktopManagerInstance = *offset.To<uDwm::CDesktopManager**>(g_moduleHandle);
		}
		if (fullyUnDecoratedFunctionName == "CDesktopManager::s_csDwmInstance")
		{
			offset.To(g_moduleHandle, CDesktopManager::s_csDwmInstance);
		}
	}

	template <bool insertAtBack>
	class CSpriteVisual
	{
	protected:
		uDwm::CVisual* m_parentVisual{ nullptr };
		winrt::com_ptr<uDwm::CVisual> m_udwmVisual{ nullptr };
		winrt::com_ptr<IDCompositionVisual2> m_dcompVisual{ nullptr };
		winrt::com_ptr<dcomp::InteropCompositionTarget> m_dcompTarget{ nullptr };
		winrt::com_ptr<dcomp::IDCompositionDesktopDevicePartner> m_dcompDevice{ nullptr };
		wuc::VisualCollection m_visualCollection{ nullptr };

		void InitializeInteropDevice(dcomp::IDCompositionDesktopDevicePartner* interopDevice)
		{
			m_dcompDevice.copy_from(interopDevice);
		}
		virtual HRESULT InitializeVisual()
		{
			RETURN_IF_FAILED(
				m_dcompDevice->CreateSharedResource(
					IID_PPV_ARGS(m_dcompTarget.put())
				)
			);
			RETURN_IF_FAILED(m_dcompDevice->CreateVisual(m_dcompVisual.put()));
			RETURN_IF_FAILED(m_dcompVisual->SetBorderMode(DCOMPOSITION_BORDER_MODE_SOFT));
#ifdef _DEBUG
			m_dcompVisual.as<IDCompositionVisualDebug>()->EnableRedrawRegions();
#endif
			RETURN_IF_FAILED(m_dcompTarget->SetRoot(m_dcompVisual.get()));
			RETURN_IF_FAILED(m_dcompDevice->Commit());
			m_visualCollection = m_dcompVisual.as<dcomp::IDCompositionVisualPartnerWinRTInterop>()->GetVisualCollection();

			wil::unique_handle resourceHandle{ nullptr };
			RETURN_IF_FAILED(
				m_dcompDevice->OpenSharedResourceHandle(m_dcompTarget.get(), resourceHandle.put())
			);
			RETURN_IF_FAILED(uDwm::CVisual::CreateFromSharedHandle(resourceHandle.get(), m_udwmVisual.put()));
			m_udwmVisual->AllowVisualTreeClone(false);
			if (m_parentVisual)
			{
				RETURN_IF_FAILED(
					m_parentVisual->GetVisualCollection()->InsertRelative(
						m_udwmVisual.get(),
						nullptr,
						insertAtBack,
						true
					)
				);
			}

			return S_OK;
		}
		virtual void UninitializeVisual()
		{
			if (m_udwmVisual)
			{
				if (m_parentVisual)
				{
					m_parentVisual->GetVisualCollection()->Remove(
						m_udwmVisual.get()
					);
				}
				m_udwmVisual = nullptr;
			}
			if (m_dcompVisual)
			{
#ifdef _DEBUG
				m_dcompVisual.as<IDCompositionVisualDebug>()->DisableRedrawRegions();
#endif
				m_visualCollection.RemoveAll();
				m_dcompVisual = nullptr;
			}
			if (m_dcompTarget)
			{
				m_dcompTarget->SetRoot(nullptr);
				m_dcompTarget = nullptr;
			}
		}

		CSpriteVisual(uDwm::CVisual* parentVisual) : m_parentVisual{ parentVisual } {}
		virtual ~CSpriteVisual() { UninitializeVisual(); }
	};
	using CBackdropVisual = CSpriteVisual<true>;
	using COverlayVisual = CSpriteVisual<false>;

	FORCEINLINE bool CheckDeviceState(const winrt::com_ptr<dcomp::IDCompositionDesktopDevicePartner>& dcompDevice)
	{
		BOOL valid{ FALSE };
		if (
			FAILED(dcompDevice.as<IDCompositionDevice>()->CheckDeviceState(&valid)) ||
			!valid ||
			dcompDevice.get() != uDwm::CDesktopManager::s_pDesktopManagerInstance->GetDCompDevice()
			)
		{
			return false;
		}

		return true;
	}
}