#pragma once
#include "framework.hpp"
#include "cpprt.hpp"
#include "dwmcoreProjection.hpp"

namespace OpenGlass
{
	// [Guid("5258B8F9-28D5-4F90-A2D9-5E716239DD57")]
	DECLARE_INTERFACE_IID_(IGlassCoverageSet, IUnknown, "5258B8F9-28D5-4F90-A2D9-5E716239DD57")
	{
		virtual void SetDeviceTransform(const dwmcore::CMILMatrix * matrix) = 0;
		virtual HRESULT Add(
			const D2D1_RECT_F& coverage,
			int depth,
			const dwmcore::CMILMatrix* matrix
		) = 0;
		virtual void Clear() = 0;
		virtual std::span<dwmcore::CZOrderedRect> GetViews() const = 0;
		virtual bool IsFullyCovered(
			const D2D1_RECT_F & coverage,
			int depth
		) const = 0;
		virtual bool IsPartiallyCovered(
			const D2D1_RECT_F& coverage,
			int depth
		) const = 0;
		virtual bool IsVisible(
			const D2D1_RECT_F& coverage,
			const dwmcore::CArrayBasedCoverageSet* occlusionCoverageSet
		) const = 0;
		virtual bool IsEmpty() const = 0;
	};

	class CArrayBasedGlassCoverageSet : public winrt::implements<CArrayBasedGlassCoverageSet, IGlassCoverageSet, winrt::non_agile, winrt::no_weak_ref>
	{
		DWM::MyDynArrayImpl<dwmcore::CZOrderedRect> m_array{};
		static std::unordered_map<const dwmcore::CArrayBasedCoverageSet*, winrt::com_ptr<IGlassCoverageSet>> s_map;
	public:
		void SetDeviceTransform(const dwmcore::CMILMatrix* matrix) override;
		HRESULT Add(
			const D2D1_RECT_F& coverage,
			int depth,
			const dwmcore::CMILMatrix* matrix
		) override;
		void Clear() override { m_array.Clear(); }
		std::span<dwmcore::CZOrderedRect> GetViews() const override { return m_array.views(); }
		bool IsFullyCovered(
			const D2D1_RECT_F& coverage,
			int depth
		) const override;
		bool IsPartiallyCovered(
			const D2D1_RECT_F& coverage,
			int depth
		) const override;
		bool IsVisible(
			const D2D1_RECT_F& coverage,
			const dwmcore::CArrayBasedCoverageSet* occlusionCoverageSet
		) const override;
		bool IsEmpty() const override;

		static winrt::com_ptr<IGlassCoverageSet> GetOrCreate(
			const dwmcore::CArrayBasedCoverageSet* coverageSet,
			bool createIfNecessary = false
		);
		static void Remove(const dwmcore::CArrayBasedCoverageSet* coverageSet);
		static void RemoveAll();
	};
}
