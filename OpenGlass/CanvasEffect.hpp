#pragma once
#include "framework.hpp"
#include "winrt.hpp"
#include "wil.hpp"

namespace OpenGlass::Win2D
{
	struct NamedProperty
	{
		LPCWSTR Name; // Compile-time constant
		UINT Index; // Property index
		using GRAPHICS_EFFECT_PROPERTY_MAPPING = ABI::Windows::Graphics::Effects::GRAPHICS_EFFECT_PROPERTY_MAPPING;
		GRAPHICS_EFFECT_PROPERTY_MAPPING Mapping;
	};
	class CanvasEffect : public winrt::implements<CanvasEffect, wge::IGraphicsEffect, wge::IGraphicsEffectSource, ABI::Windows::Graphics::Effects::IGraphicsEffectD2D1Interop>
	{
		CLSID m_effectId{};
		winrt::hstring m_name{};
		std::unordered_map<size_t, wf::IPropertyValue> m_properties{};
		std::unordered_map<size_t, wge::IGraphicsEffectSource> m_effectSources{};
	protected:
		std::vector<NamedProperty> m_namedProperties{};
	public:
		CanvasEffect(REFCLSID effectId) : m_effectId{ effectId } {}
		virtual ~CanvasEffect() = default;

		winrt::hstring Name()
		{
			return m_name;
		}
		void Name(const winrt::hstring& string)
		{
			m_name = string;
		}

		IFACEMETHOD(GetEffectId)(CLSID* id) override
		{
			if (id)
			{
				*id = m_effectId;
				return S_OK;
			}

			return E_POINTER;
		}
		IFACEMETHOD(GetSourceCount)(UINT* count) override
		{
			if (count)
			{
				*count = static_cast<UINT>(m_effectSources.size());
				return S_OK;
			}

			return E_POINTER;
		}
		IFACEMETHOD(GetSource)(UINT index, ABI::Windows::Graphics::Effects::IGraphicsEffectSource** source)
		{
			if (!source)
			{
				return E_POINTER;
			}

			m_effectSources.at(index).as<ABI::Windows::Graphics::Effects::IGraphicsEffectSource>().copy_to(source);
			return S_OK;
		}
		IFACEMETHOD(GetPropertyCount)(UINT* count) override
		{
			if (count)
			{
				*count = static_cast<UINT>(m_effectSources.size());
				return S_OK;
			}

			return E_POINTER;
		}
		IFACEMETHOD(GetProperty)(UINT index, ABI::Windows::Foundation::IPropertyValue** value) override
		{
			if (!value)
			{
				return E_POINTER;
			}

			*value = m_properties.at(index).as<ABI::Windows::Foundation::IPropertyValue>().detach();
			return S_OK;
		}
		IFACEMETHOD(GetNamedPropertyMapping)(LPCWSTR name, UINT* index, NamedProperty::GRAPHICS_EFFECT_PROPERTY_MAPPING* mapping) override
		{
			for (UINT i = 0; i < m_namedProperties.size(); ++i)
			{
				const auto& prop = m_namedProperties[i];
				if (!_wcsicmp(name, prop.Name))
				{
					*index = prop.Index;
					*mapping = prop.Mapping;
					return S_OK;
				}
			}
			return E_INVALIDARG;
		}
	public:
		template <typename T>
		static winrt::com_ptr<T> construct_from_abi(T* from)
		{
			winrt::com_ptr<T> to{ nullptr };
			to.copy_from(from);

			return to;
		};
		void SetName(const winrt::hstring& name) { Name(name); }
		void SetInput(UINT index, const wge::IGraphicsEffectSource& source)
		{
			m_effectSources.insert_or_assign(index, source);
		}
		void SetInput(const wge::IGraphicsEffectSource& source)
		{
			SetInput(0, source);
		}
		void SetInput(const wuc::CompositionEffectSourceParameter& source)
		{
			SetInput(0, source.as<wge::IGraphicsEffectSource>());
		}

		void SetInput(const std::vector<wge::IGraphicsEffectSource>& effectSourceList)
		{
			for (UINT i = 0; i < effectSourceList.size(); i++)
			{
				SetInput(i, effectSourceList[i]);
			}
		}
	protected:
		void SetProperty(UINT index, const wf::IPropertyValue& value)
		{
			m_properties.insert_or_assign(index, value);
		}
		template <typename T>
		auto BoxValue(T value)
		{
			return wf::PropertyValue::CreateUInt32(value).as<wf::IPropertyValue>();
		}
		auto BoxValue(bool value)
		{
			return wf::PropertyValue::CreateBoolean(value).as<wf::IPropertyValue>();
		}
		auto BoxValue(float value)
		{
			return wf::PropertyValue::CreateSingle(value).as<wf::IPropertyValue>();
		}
		auto BoxValue(UINT32 value)
		{
			return wf::PropertyValue::CreateUInt32(value).as<wf::IPropertyValue>();
		}
		auto BoxValue(const D2D1_MATRIX_5X4_F& value)
		{
			return wf::PropertyValue::CreateSingleArray({ reinterpret_cast<const float*>(&value), sizeof(value) / sizeof(float) }).as<wf::IPropertyValue>();
		}
		template <size_t size>
		auto BoxValue(float(&value)[size])
		{
			return wf::PropertyValue::CreateSingleArray(value).as<wf::IPropertyValue>();
		}
	};
}