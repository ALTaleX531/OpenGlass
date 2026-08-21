#pragma once
#include "HookHelper.hpp"
#include "OSHelper.hpp"
#include "Util.hpp"
#include <tuple>
#include <type_traits>

namespace OpenGlass::Projection
{
	enum class ModuleId : UCHAR
	{
		uDWM,
		DwmCore
	};

	enum class Requirement : UCHAR
	{
		Required,
		Optional
	};

	enum class ResolutionState : UCHAR
	{
		Missing,
		Unique,
		Ambiguous
	};

	enum class SymbolFlags : UCHAR
	{
		None = 0,
		DebugOnly = 1
	};

	struct Version
	{
		ULONG build;
		ULONG revision;
	};

	struct VersionRange
	{
		Version minInclusive;
		Version maxExclusive;
	};

	inline constexpr VersionRange all_versions{};

	constexpr bool VersionBefore(Version runtime, Version boundary) noexcept
	{
		if (!boundary.build)
		{
			return true;
		}
		if (!boundary.revision)
		{
			return runtime.build < boundary.build;
		}
		return runtime.build < boundary.build ||
			   (runtime.build == boundary.build && runtime.revision < boundary.revision);
	}

	constexpr bool IsVersionInRange(Version version, VersionRange range) noexcept
	{
		const bool afterStart =
			!range.minInclusive.build || version.build > range.minInclusive.build ||
			(version.build == range.minInclusive.build && version.revision >= range.minInclusive.revision);
		return afterStart && VersionBefore(version, range.maxExclusive);
	}

	struct SymbolSpec
	{
		size_t symbolIndex;
		size_t idOffset;
		size_t firstNameIndex;
		size_t nameCount;
		size_t minVersionIndex;
		size_t maxVersionIndex;
		Requirement requirement;
		SymbolFlags flags;
	};

	struct BindingSpec
	{
		size_t symbolIndex;
		PVOID* storage;
		PVOID fallback;
	};

	struct LayoutSpec
	{
		size_t firstCase;
		size_t caseCount;
	};

	struct LayoutCase
	{
		LONG offset;
		size_t boundaryVersionIndex;
	};

	class ModuleRegistry
	{
		LPCSTR m_name;
		LPCSTR m_strings;
		const size_t* m_symbolNameOffsets;
		const Version* m_versions;
		const ULONG* m_knownBuilds;
		const SymbolSpec* m_symbolSpecs;
		PVOID* m_candidates;
		PVOID* m_resolved;
		ResolutionState* m_resolutionStates;
		const BindingSpec* m_bindings;
		const LayoutSpec* m_layoutSpecs;
		const LayoutCase* m_layoutCases;
		LONG* m_selectedOffsets;
		bool* m_layoutSupported;
		VersionRange m_supportedRange;
		Version m_version{};
		ULONG m_undecorationFailureCount{};
		size_t m_layoutCaseCount{};
		size_t m_symbolNameCount{};
		size_t m_versionCount{};
		size_t m_symbolSpecCount{};
		size_t m_symbolCount{};
		size_t m_knownBuildCount{};
		size_t m_bindingCount{};
		size_t m_layoutCount{};
		bool m_descriptorError{};

		constexpr LPCSTR String(size_t offset) const noexcept
		{
			return m_strings + offset;
		}

		constexpr Version VersionAt(size_t index) const noexcept
		{
			return index < m_versionCount ? m_versions[index] : Version{};
		}

		constexpr VersionRange RangeOf(const SymbolSpec& spec) const noexcept
		{
			return {VersionAt(spec.minVersionIndex), VersionAt(spec.maxVersionIndex)};
		}

		static constexpr bool IsEnabled([[maybe_unused]] const SymbolSpec& spec) noexcept
		{
#ifdef _DEBUG
			return true;
#else
			return spec.flags != SymbolFlags::DebugOnly;
#endif
		}

		static constexpr bool IsRequired(const SymbolSpec& spec) noexcept
		{
			return spec.requirement == Requirement::Required;
		}

		bool Matches(const SymbolSpec& spec, LPCSTR completeSymbolName) const noexcept;
		void PublishBindings() noexcept;

	public:
		constexpr ModuleRegistry(
			LPCSTR name,
			LPCSTR strings,
			std::span<const size_t> symbolNameOffsets,
			std::span<const Version> versions,
			std::span<const ULONG> knownBuilds,
			std::span<const SymbolSpec> symbolSpecs,
			std::span<PVOID> candidates,
			std::span<PVOID> resolved,
			std::span<ResolutionState> resolutionStates,
			std::span<const BindingSpec> bindings,
			std::span<const LayoutSpec> layoutSpecs,
			std::span<const LayoutCase> layoutCases,
			std::span<LONG> selectedOffsets,
			std::span<bool> layoutSupported,
			VersionRange supportedRange = all_versions
		) noexcept
			: m_name{name}, m_strings{strings}, m_symbolNameOffsets{symbolNameOffsets.data()}, m_versions{versions.data()},
			  m_knownBuilds{knownBuilds.data()}, m_symbolSpecs{symbolSpecs.data()}, m_candidates{candidates.data()}, m_resolved{resolved.data()},
			  m_resolutionStates{resolutionStates.data()}, m_bindings{bindings.data()}, m_layoutSpecs{layoutSpecs.data()},
			  m_layoutCases{layoutCases.data()}, m_selectedOffsets{selectedOffsets.data()}, m_layoutSupported{layoutSupported.data()},
			  m_supportedRange{supportedRange}, m_layoutCaseCount{layoutCases.size()}, m_symbolNameCount{symbolNameOffsets.size()},
			  m_versionCount{versions.size()}, m_symbolSpecCount{symbolSpecs.size()}, m_symbolCount{candidates.size()},
			  m_knownBuildCount{knownBuilds.size()}, m_bindingCount{bindings.size()}, m_layoutCount{layoutSpecs.size()}
		{
		}

		bool Freeze(Version version) noexcept;
		void ResetSymbols() noexcept;
		void Collect(LPCSTR completeSymbolName, PVOID address) noexcept;
		void RecordUndecorationFailure() noexcept;
		bool ValidateSymbols() const noexcept;
		void CommitSymbols() noexcept;
		void ReportUnresolved(std::string& output, std::string_view prefix) const;
		__declspec(noinline) PVOID SymbolAddress(size_t index, bool checked) const;
		__declspec(noinline) VersionRange SymbolRange(size_t index) const;
		__declspec(noinline) LONG LayoutOffset(size_t index) const;

		bool LayoutSupported(size_t index) const noexcept
		{
			return index < m_layoutCount && m_layoutSupported[index];
		}
		LPCSTR name() const noexcept
		{
			return m_name;
		}
		Version version() const noexcept
		{
			return m_version;
		}
		bool SupportsVersion(Version version) const noexcept
		{
			return IsVersionInRange(version, m_supportedRange);
		}
		bool RecognizesBuild(ULONG build) const noexcept
		{
			return std::ranges::binary_search(std::span<const ULONG>{m_knownBuilds, m_knownBuildCount}, build);
		}
		bool descriptor_error() const noexcept
		{
			return m_descriptorError;
		}
		size_t descriptor_count() const noexcept
		{
			return m_symbolCount + m_layoutCount;
		}
	};

	bool CommitModules(ModuleRegistry& first, ModuleRegistry& second) noexcept;

	template <typename ModuleTag> ModuleRegistry& RegistryFor() noexcept;
	template <typename ModuleTag> struct LayoutState;

	template <typename T>
	concept symbol_pointer = std::is_pointer_v<T> || std::is_member_pointer_v<T>;

	template <typename ModuleTag, size_t Index, symbol_pointer T> class SymbolHandle
	{
	public:
		using value_type = T;
		using module_tag = ModuleTag;
		inline static constexpr size_t index{Index};

		__forceinline T get() const
		{
			static_assert(sizeof(T) == sizeof(PVOID), "projected pointer representation must be pointer-sized");
			return Util::force_cast_to<T>(RegistryFor<ModuleTag>().SymbolAddress(Index, true));
		}

		__forceinline T try_get() const noexcept
		{
			static_assert(sizeof(T) == sizeof(PVOID), "projected pointer representation must be pointer-sized");
			return Util::force_cast_to<T>(RegistryFor<ModuleTag>().SymbolAddress(Index, false));
		}

		__forceinline PVOID get_address() const
		{
			return RegistryFor<ModuleTag>().SymbolAddress(Index, true);
		}

		__forceinline PVOID try_address() const noexcept
		{
			return RegistryFor<ModuleTag>().SymbolAddress(Index, false);
		}

		__forceinline VersionRange range() const
		{
			return RegistryFor<ModuleTag>().SymbolRange(Index);
		}

		__forceinline explicit operator bool() const noexcept
		{
			return try_address() != nullptr;
		}

		template <typename... Args>
			requires std::invocable<T, Args...>
		__forceinline decltype(auto) operator()(Args&&... args) const
		{
			return std::invoke(get(), std::forward<Args>(args)...);
		}
	};

	template <typename ModuleTag, size_t Index, typename T> class FieldHandle
	{
		__forceinline LONGLONG SelectedOffset() const noexcept
		{
			return LayoutState<ModuleTag>::Offset(Index);
		}

	public:
		inline static constexpr size_t index{Index};

		template <typename BaseT> __forceinline T* address(BaseT* base) const
		{
			return reinterpret_cast<T*>(reinterpret_cast<ULONG_PTR>(base) + SelectedOffset());
		}

		template <typename BaseT> __forceinline std::add_const_t<T>* address(const BaseT* base) const
		{
			return reinterpret_cast<std::add_const_t<T>*>(reinterpret_cast<ULONG_PTR>(base) + SelectedOffset());
		}

		template <typename BaseT> __forceinline T& ref(BaseT* base) const
		{
			return *address(base);
		}

		template <typename BaseT> __forceinline std::add_const_t<T>& ref(const BaseT* base) const
		{
			return *address(base);
		}

		template <typename BaseT> __forceinline T* mutable_address(const BaseT* base) const
		{
			return const_cast<T*>(address(base));
		}

		template <typename BaseT> __forceinline T& mutable_ref(const BaseT* base) const
		{
			return *mutable_address(base);
		}

		template <typename BaseT> __forceinline T read(const BaseT* base) const
		{
			return ref(base);
		}

		__forceinline LONGLONG offset() const
		{
			return RegistryFor<ModuleTag>().LayoutOffset(Index);
		}

		__forceinline bool is_supported() const noexcept
		{
			return LayoutState<ModuleTag>::IsSupported(Index);
		}
	};

	template <typename ModuleTag, size_t Index, typename T>
	class VtableSlotHandle final : public FieldHandle<ModuleTag, Index, T>
	{
	};

	template <typename T> struct ProjectedAbi;
	[[noreturn]] void ProjectedFailFastCommon() noexcept;

#define OPENGLASS_DEFINE_MEMBER_ABI(QUALIFIERS, THIS_TYPE)                                                           \
	template <typename R, typename C, typename... Args> struct ProjectedAbi<R (C::*)(Args...) QUALIFIERS>            \
	{                                                                                                                \
		using type = R (*)(THIS_TYPE C*, Args...);                                                                     \
		[[noreturn]] static R FailFast(THIS_TYPE C*, Args...)                                                          \
		{                                                                                                              \
			ProjectedFailFastCommon();                                                                                   \
		}                                                                                                              \
	};

	OPENGLASS_DEFINE_MEMBER_ABI(, )
	OPENGLASS_DEFINE_MEMBER_ABI(const, const)
	OPENGLASS_DEFINE_MEMBER_ABI(noexcept, )
	OPENGLASS_DEFINE_MEMBER_ABI(const noexcept, const)
#undef OPENGLASS_DEFINE_MEMBER_ABI

	template <typename R, typename... Args> struct ProjectedAbi<R (*)(Args...)>
	{
		using type = R (*)(Args...);
		[[noreturn]] static R FailFast(Args...)
		{
			ProjectedFailFastCommon();
		}
	};

	template <typename R, typename... Args> struct ProjectedAbi<R (*)(Args...) noexcept>
	{
		using type = R (*)(Args...) noexcept;
		[[noreturn]] static R FailFast(Args...) noexcept
		{
			ProjectedFailFastCommon();
		}
	};

	template <auto Target> using projected_abi_t = typename ProjectedAbi<decltype(Target)>::type;

	template <typename T> struct FunctionPointerTraits;
	template <typename R, typename... Args> struct FunctionPointerTraits<R (*)(Args...)>
	{
		using result_type = R;
		using arguments = std::tuple<Args...>;
	};
	template <typename R, typename... Args> struct FunctionPointerTraits<R (*)(Args...) noexcept>
	{
		using result_type = R;
		using arguments = std::tuple<Args...>;
	};

	template <typename Source, typename Target, typename = void> struct IsDiscardReturnCompatible : std::false_type
	{
	};
	template <typename Source, typename Target>
	struct IsDiscardReturnCompatible<
		Source,
		Target,
		std::void_t<
			typename FunctionPointerTraits<Source>::result_type,
			typename FunctionPointerTraits<Source>::arguments,
			typename FunctionPointerTraits<Target>::result_type,
			typename FunctionPointerTraits<Target>::arguments
		>
	> : std::bool_constant<
		!std::is_void_v<typename FunctionPointerTraits<Source>::result_type> &&
		std::is_void_v<typename FunctionPointerTraits<Target>::result_type> &&
		std::same_as<
			typename FunctionPointerTraits<Source>::arguments,
			typename FunctionPointerTraits<Target>::arguments
		>
	>
	{
	};

	template <typename SourceTuple, typename TargetTuple, size_t... Indices>
	consteval bool IsTuplePrefix(std::index_sequence<Indices...>)
	{
		return (std::same_as<
			std::tuple_element_t<Indices, SourceTuple>,
			std::tuple_element_t<Indices, TargetTuple>
		> && ...);
	}

	template <typename Source, typename Target, typename = void> struct IsExtraTrailingArgumentCompatible : std::false_type
	{
	};
	template <typename Source, typename Target>
	struct IsExtraTrailingArgumentCompatible<
		Source,
		Target,
		std::void_t<
			typename FunctionPointerTraits<Source>::result_type,
			typename FunctionPointerTraits<Source>::arguments,
			typename FunctionPointerTraits<Target>::result_type,
			typename FunctionPointerTraits<Target>::arguments
		>
	> : std::bool_constant<
		std::same_as<
			typename FunctionPointerTraits<Source>::result_type,
			typename FunctionPointerTraits<Target>::result_type
		> &&
		(std::tuple_size_v<typename FunctionPointerTraits<Target>::arguments> ==
		 std::tuple_size_v<typename FunctionPointerTraits<Source>::arguments> + 1) &&
		IsTuplePrefix<
			typename FunctionPointerTraits<Source>::arguments,
			typename FunctionPointerTraits<Target>::arguments
		>(std::make_index_sequence<std::tuple_size_v<typename FunctionPointerTraits<Source>::arguments>>{})
	>
	{
	};

	template <typename Source, typename Target>
	inline constexpr bool is_discard_return_compatible_v = IsDiscardReturnCompatible<Source, Target>::value;
	template <typename Source, typename Target>
	inline constexpr bool is_extra_trailing_argument_compatible_v =
		IsExtraTrailingArgumentCompatible<Source, Target>::value;

	template <auto Target> inline projected_abi_t<Target> g_projectedSlot{};

	template <auto Target> inline constexpr projected_abi_t<Target> ProjectedFailFast =
		&ProjectedAbi<decltype(Target)>::FailFast;

	template <auto Target> PVOID* ProjectedSlotStorage() noexcept
	{
		static_assert(sizeof(g_projectedSlot<Target>) == sizeof(PVOID));
		return reinterpret_cast<PVOID*>(&g_projectedSlot<Target>);
	}

	template <auto Target> PVOID* ProjectedVariableStorage() noexcept
	{
		static_assert(std::is_pointer_v<decltype(Target)>);
		static_assert(sizeof(*Target) == sizeof(PVOID));
		return reinterpret_cast<PVOID*>(reinterpret_cast<ULONG_PTR>(Target));
	}

	template <typename T> PVOID ProjectedAddress(T pointer) noexcept
	{
		static_assert(sizeof(pointer) == sizeof(PVOID));
		return Util::force_cast_from(pointer);
	}

	template <auto Target> inline projected_abi_t<Target>& Invoke = g_projectedSlot<Target>;

#ifdef _DEBUG
#define OPENGLASS_MUSTTAIL
#else
#define OPENGLASS_MUSTTAIL [[msvc::musttail]]
#endif

	template <typename T>
	concept function_pointer = std::is_pointer_v<T> && std::is_function_v<std::remove_pointer_t<T>>;
	PVOID* PrepareDetourStorage(PVOID* storage, ModuleRegistry& registry, size_t index);

	template <typename T, bool = std::is_member_pointer_v<T>> struct DetourSymbolAbi
	{
		using type = T;
	};

	template <typename R, typename C, typename... Args>
	struct DetourSymbolAbi<R (C::*)(Args...), true>
	{
		using type = R (*)(C*, Args...);
	};

#define OPENGLASS_DEFINE_DETOUR_MEMBER_ABI(QUALIFIERS)                                                              \
	template <typename R, typename C, typename... Args>                                                               \
	struct DetourSymbolAbi<R (C::*)(Args...) QUALIFIERS, true>                                                        \
	{                                                                                                                  \
		using type = R (*)(C*, Args...);                                                                                 \
	};

	OPENGLASS_DEFINE_DETOUR_MEMBER_ABI(const)
	OPENGLASS_DEFINE_DETOUR_MEMBER_ABI(noexcept)
	OPENGLASS_DEFINE_DETOUR_MEMBER_ABI(const noexcept)
#undef OPENGLASS_DEFINE_DETOUR_MEMBER_ABI

	template <typename Owner, typename T> struct DetourDispatchThunk;

	template <typename Owner, typename R, typename... Args>
	struct DetourDispatchThunk<Owner, R (*)(Args...)>
	{
		static R Invoke(Args... args)
		{
			auto& rundown = HookHelper::GetHookRundown();
			if (!rundown.TryAcquire())
			{
				return std::invoke(Owner::EntryOriginal(), std::forward<Args>(args)...);
			}

			const auto release = wil::scope_exit([&rundown]
			{
				rundown.Release();
			});
			return Owner::InvokeReplacement(std::forward<Args>(args)...);
		}
	};

	template <auto Symbol> struct DetourSymbolState
	{
		using SymbolHandleType = std::remove_cvref_t<decltype(Symbol)>;
		using SymbolPointerType = typename SymbolHandleType::value_type;
		using T = typename DetourSymbolAbi<SymbolPointerType>::type;

		inline static T s_entryOriginal{};
		inline static T s_dispatcher{};
	};

	// Projection hooks are exclusive by default. The replacement is part of the
	// type so an exclusive detour cannot be rebound at runtime. Opt in to
	// ChainDetour only when multiple independent components intentionally replace
	// the same symbol.
	template <auto Symbol, auto Replacement>
	requires function_pointer<decltype(Replacement)>
	class Detour final
	{
		using SymbolHandleType = std::remove_cvref_t<decltype(Symbol)>;
		using SymbolPointerType = typename SymbolHandleType::value_type;
		using T = typename DetourSymbolAbi<SymbolPointerType>::type;
		using State = DetourSymbolState<Symbol>;
		static_assert(
			std::same_as<T, decltype(Replacement)>,
			"detour replacement must match the typed symbol ABI"
		);

	public:
		static T EntryOriginal() noexcept
		{
			return State::s_entryOriginal;
		}

		static decltype(auto) InvokeReplacement(auto&&... args)
		{
			return std::invoke(Replacement, std::forward<decltype(args)>(args)...);
		}

		T prepare_detour() noexcept
		{
			constexpr T dispatcher = &DetourDispatchThunk<Detour, T>::Invoke;
			FAIL_FAST_IF_FAILED_MSG(
				!State::s_dispatcher || State::s_dispatcher == dispatcher ? S_OK : E_UNEXPECTED,
				"A projection symbol cannot have multiple exclusive detours"
			);

			State::s_dispatcher = dispatcher;
			return dispatcher;
		}

		__forceinline T* detour_storage()
		{
			return reinterpret_cast<T*>(PrepareDetourStorage(
				reinterpret_cast<PVOID*>(&State::s_entryOriginal),
				RegistryFor<typename SymbolHandleType::module_tag>(),
				SymbolHandleType::index
			));
		}

		explicit operator bool() const noexcept
		{
			return State::s_entryOriginal || static_cast<bool>(Symbol);
		}

		__forceinline decltype(auto) operator()(auto&&... args) const
		{
			return std::invoke(State::s_entryOriginal, std::forward<decltype(args)>(args)...);
		}
	};

	template <auto Value> struct NttpValueTag
	{
	};
	template <auto... Values> struct UniqueNttpValues : std::true_type
	{
	};
	template <auto First, auto... Rest>
	struct UniqueNttpValues<First, Rest...> :
		std::bool_constant<
			((!std::same_as<NttpValueTag<First>, NttpValueTag<Rest>>) && ...) &&
			UniqueNttpValues<Rest...>::value
		>
	{
	};

	// ChainDetour fixes chain membership and call order in the type while each
	// component's Node records whether that replacement is active in this process.
	// All nodes share one physical dispatcher and original trampoline per symbol.
	template <auto Symbol, auto... Replacements>
	requires (function_pointer<decltype(Replacements)> && ...)
	class ChainDetour final
	{
		using SymbolHandleType = std::remove_cvref_t<decltype(Symbol)>;
		using SymbolPointerType = typename SymbolHandleType::value_type;
		using T = typename DetourSymbolAbi<SymbolPointerType>::type;
		using State = DetourSymbolState<Symbol>;
		static constexpr size_t c_replacementCount = sizeof...(Replacements);
		static constexpr auto c_replacements = std::tuple{ Replacements... };
		static_assert(c_replacementCount != 0, "a projection detour chain cannot be empty");
		static_assert(
			(std::same_as<T, decltype(Replacements)> && ...),
			"every chain replacement must match the typed symbol ABI"
		);
		static_assert(
			UniqueNttpValues<Replacements...>::value,
			"a replacement may appear only once in a projection detour chain"
		);

		inline static std::array<bool, c_replacementCount> s_activeNodes{};

		template <size_t Index>
		static decltype(auto) InvokeFrom(auto&&... args)
		{
			if constexpr (Index == c_replacementCount)
			{
				return std::invoke(State::s_entryOriginal, std::forward<decltype(args)>(args)...);
			}
			else
			{
				if (s_activeNodes[Index])
				{
					constexpr auto replacement = std::get<Index>(c_replacements);
					return std::invoke(replacement, std::forward<decltype(args)>(args)...);
				}
				return InvokeFrom<Index + 1>(std::forward<decltype(args)>(args)...);
			}
		}

	public:
		static T EntryOriginal() noexcept
		{
			return State::s_entryOriginal;
		}

		static decltype(auto) InvokeReplacement(auto&&... args)
		{
			return InvokeFrom<0>(std::forward<decltype(args)>(args)...);
		}

		template <size_t Index>
		class Node final
		{
			static_assert(Index < c_replacementCount, "chain node index is out of range");

		public:
			T prepare_detour() noexcept
			{
				constexpr T dispatcher = &DetourDispatchThunk<ChainDetour, T>::Invoke;
				FAIL_FAST_IF_FAILED_MSG(
					!State::s_dispatcher || State::s_dispatcher == dispatcher ? S_OK : E_UNEXPECTED,
					"A projection symbol cannot mix chain and exclusive dispatchers"
				);

				State::s_dispatcher = dispatcher;
				s_activeNodes[Index] = true;
				return dispatcher;
			}

			__forceinline T* detour_storage()
			{
				return reinterpret_cast<T*>(PrepareDetourStorage(
					reinterpret_cast<PVOID*>(&State::s_entryOriginal),
					RegistryFor<typename SymbolHandleType::module_tag>(),
					SymbolHandleType::index
				));
			}

			explicit operator bool() const noexcept
			{
				return State::s_entryOriginal || static_cast<bool>(Symbol);
			}

			__forceinline decltype(auto) operator()(auto&&... args) const
			{
				return InvokeFrom<Index + 1>(std::forward<decltype(args)>(args)...);
			}
		};
	};

	// Some private optimized functions preserve otherwise volatile caller state and
	// therefore cannot pass through the normal C++ dispatcher. The physical entry is
	// part of the type and must bracket Dispatch with the narrow register preservation
	// required by the audited caller contract.
	template <auto Symbol, auto DispatchEntry>
	requires function_pointer<decltype(DispatchEntry)>
	class CustomDispatchDetour final
	{
		using SymbolHandleType = std::remove_cvref_t<decltype(Symbol)>;
		using SymbolPointerType = typename SymbolHandleType::value_type;
		using T = typename DetourSymbolAbi<SymbolPointerType>::type;
		using State = DetourSymbolState<Symbol>;
		static_assert(
			std::same_as<T, decltype(DispatchEntry)>,
			"custom dispatch entry must match the typed symbol ABI"
		);

	public:
		T prepare_detour() noexcept
		{
			FAIL_FAST_IF_FAILED_MSG(
				!State::s_dispatcher || State::s_dispatcher == DispatchEntry ? S_OK : E_UNEXPECTED,
				"A projection symbol cannot have multiple exclusive dispatchers"
			);

			State::s_dispatcher = DispatchEntry;
			return DispatchEntry;
		}

		__forceinline T* detour_storage()
		{
			return reinterpret_cast<T*>(PrepareDetourStorage(
				reinterpret_cast<PVOID*>(&State::s_entryOriginal),
				RegistryFor<typename SymbolHandleType::module_tag>(),
				SymbolHandleType::index
			));
		}

		template <typename Replacement, typename... Args>
		requires std::invocable<Replacement, Args...>
		__forceinline decltype(auto) Dispatch(Replacement&& replacement, Args&&... args)
		{
			auto& rundown = HookHelper::GetHookRundown();
			if (!rundown.TryAcquire())
			{
				return std::invoke(State::s_entryOriginal, std::forward<decltype(args)>(args)...);
			}

			const auto release = wil::scope_exit([&rundown]
			{
				rundown.Release();
			});
			return std::invoke(std::forward<Replacement>(replacement), std::forward<Args>(args)...);
		}

		__forceinline decltype(auto) operator()(auto&&... args) const
		{
			return std::invoke(State::s_entryOriginal, std::forward<decltype(args)>(args)...);
		}

		explicit operator bool() const noexcept
		{
			return State::s_entryOriginal || static_cast<bool>(Symbol);
		}
	};
} // namespace OpenGlass::Projection
