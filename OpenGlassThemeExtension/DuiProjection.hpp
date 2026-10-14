#pragma once
#include "ProjectionInvoke.hpp"

namespace OpenGlass::DirectUI
{
	struct Element;
	struct CCPushButton;
	struct DUIXmlParser;
	struct PropertyInfo;
	struct Value;
	struct InputEvent;
	struct Event;

	struct IElementListener
	{
		virtual void OnListenerAttach(Element*) = 0;
		virtual void OnListenerDetach(Element*) = 0;
		virtual bool OnListenedPropertyChanging(Element*, const PropertyInfo*, int, Value*, Value*) = 0;
		virtual void OnListenedPropertyChanged(Element*, const PropertyInfo*, int, Value*, Value*) = 0;
		virtual void OnListenedInput(Element*, InputEvent*) = 0;
		virtual void OnListenedEvent(Element*, Event*) = 0;
	};
	struct IXProviderCP
	{
		virtual HRESULT CreateDUICP(Element*, HWND, HWND, Element**, DUIXmlParser**) = 0;
		virtual HRESULT CreateParserCP(DUIXmlParser**) = 0;
		virtual void DestroyCP() = 0;
	};

	// Opaque native objects: only exported methods are projected, with no field layout.
	struct Element
	{
		inline HRESULT Destroy(bool delayed)
		{
			OPENGLASS_MUSTTAIL return Projection::Invoke<&Element::Destroy>(this, delayed);
		}
		inline Element* FindDescendent(ATOM id)
		{
			OPENGLASS_MUSTTAIL return Projection::Invoke<&Element::FindDescendent>(this, id);
		}
		inline HRESULT AddListener(IElementListener* listener)
		{
			OPENGLASS_MUSTTAIL return Projection::Invoke<&Element::AddListener>(this, listener);
		}
		inline void RemoveListener(IElementListener* listener)
		{
			OPENGLASS_MUSTTAIL return Projection::Invoke<&Element::RemoveListener>(this, listener);
		}
		inline bool GetSelected()
		{
			OPENGLASS_MUSTTAIL return Projection::Invoke<&Element::GetSelected>(this);
		}
		inline HRESULT SetSelected(bool selected)
		{
			OPENGLASS_MUSTTAIL return Projection::Invoke<&Element::SetSelected>(this, selected);
		}
		inline static const PropertyInfo* SelectedProp()
		{
			OPENGLASS_MUSTTAIL return Projection::Invoke<&Element::SelectedProp>();
		}
		inline PCWSTR GetAccName(Value** value)
		{
			OPENGLASS_MUSTTAIL return Projection::Invoke<&Element::GetAccName>(this, value);
		}
		inline HRESULT SetContentString(PCWSTR content)
		{
			OPENGLASS_MUSTTAIL return Projection::Invoke<&Element::SetContentString>(this, content);
		}
	};

	struct Selector : Element
	{
		inline Element* GetSelection()
		{
			OPENGLASS_MUSTTAIL return Projection::Invoke<&Selector::GetSelection>(this);
		}
		inline static const PropertyInfo* SelectionProp()
		{
			OPENGLASS_MUSTTAIL return Projection::Invoke<&Selector::SelectionProp>();
		}
	};

	struct Value
	{
		inline void Release()
		{
			OPENGLASS_MUSTTAIL return Projection::Invoke<&Value::Release>(this);
		}
	};

	struct DUIXmlParser
	{
		using ValueParser = Value* (*)(PCWSTR, void*);
		using ParseErrorHandler = void (*)(PCWSTR, PCWSTR, int, void*);

		inline static HRESULT Create(DUIXmlParser** parser, ValueParser valueParser, void* valueContext, ParseErrorHandler errorHandler, void* errorContext)
		{
			OPENGLASS_MUSTTAIL return Projection::Invoke<&DUIXmlParser::Create>(parser, valueParser, valueContext, errorHandler, errorContext);
		}
		inline void Destroy()
		{
			OPENGLASS_MUSTTAIL return Projection::Invoke<&DUIXmlParser::Destroy>(this);
		}
		inline HRESULT SetXMLFromResourceWithTheme(UINT resource, HINSTANCE xmlModule, HINSTANCE resourceModule, HINSTANCE classModule)
		{
			OPENGLASS_MUSTTAIL return Projection::Invoke<&DUIXmlParser::SetXMLFromResourceWithTheme>(this, resource, xmlModule, resourceModule, classModule);
		}
		inline HRESULT CreateElement(PCWSTR id, Element* parent, Element* host, DWORD* deferKey, Element** element)
		{
			OPENGLASS_MUSTTAIL return Projection::Invoke<&DUIXmlParser::CreateElement>(this, id, parent, host, deferKey, element);
		}
	};

	struct XResourceProvider
	{
		inline static HRESULT Create(HINSTANCE resources, PCWSTR resource, PCWSTR element, PCWSTR theme, IXProviderCP** provider)
		{
			OPENGLASS_MUSTTAIL return Projection::Invoke<&XResourceProvider::Create>(resources, resource, element, theme, provider);
		}
	};

	inline ATOM StrToID(PCWSTR value)
	{
		OPENGLASS_MUSTTAIL return Projection::Invoke<&StrToID>(value);
	}

	inline constexpr PCSTR XResourceProvider_Create_Name = "?Create@XResourceProvider@DirectUI@@SAJPEAUHINSTANCE__@@PEBG11PEAPEAV12@@Z";
	void InitializeProjection();
}
