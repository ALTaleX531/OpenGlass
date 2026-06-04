#pragma once
#include "uDwmProjection.hpp"

namespace OpenGlass
{
	class CReflectionVisual : public uDWM::CSpriteVisual
	{
		static PVOID s_customVtable[32];
		static PVOID s_CSpriteVisual_vector_deleting_destructor_Org;
		static PVOID s_CSpriteVisual_CloneVisualTree_Org;
		static LPCVOID s_originalVtable;

		static std::unordered_set<CReflectionVisual*> s_activeList;

		static void Register(CReflectionVisual* visual);
		static void Unregister(CReflectionVisual* visual);
		static void EnsureCustomVtable(PVOID const* sourceVtable);

		void* vector_deleting_destructor(UINT flags);
		HRESULT CloneVisualTree(CReflectionVisual** clonedVisual, UINT cloneOption);
	public:
		static void RemoveAll();
		static HRESULT Create(CReflectionVisual** result);
		static HRESULT CreateSurface(abi::ICompositionSurface** surface);

		HRESULT UpdateOpacity(float opacity);
		HRESULT UpdateSurface(abi::ICompositionSurface* surface);
		HRESULT UpdateViewport(
			const POINT& offset,
			float parallaxIntensity = 0.f,
			bool mirrored = false,
			LONG width = 0,
			const D2D1_SIZE_F& scale = { 1.f, 1.f }
		);
	};
}
