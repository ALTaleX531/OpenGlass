#pragma once
#include <map>
#include <optional>
#include <stdexcept>
#include <set>

namespace OpenGlass
{
	// Registry-independent journal, shared by immediate edits and multi-step previews.
	template<class Key, class Value>
	class PreviewJournal
	{
		std::map<Key, Value> m_baseline;
		std::map<Key, Value> m_attempt;
		std::set<Key> m_pending;
		std::optional<std::map<Key, Value>> m_previousBaseline;
	public:
		bool IsDirty() const noexcept { return !m_baseline.empty(); }
		bool IsAttemptActive() const noexcept { return m_previousBaseline.has_value(); }
		// Reconcile removes unchanged writes; session dirtiness is independent.
		bool HasAttemptChanges() const noexcept { return !m_attempt.empty(); }
		void Begin()
		{
			if (IsAttemptActive()) throw std::logic_error("Nested preview attempt");
			m_previousBaseline = m_baseline;
			m_attempt.clear();
		}
		template<class Reader> void Touch(const Key& key, Reader&& read)
		{
			m_pending.insert(key);
			if (m_baseline.contains(key) && (!IsAttemptActive() || m_attempt.contains(key))) return;
			auto value = read(key);
			if (IsAttemptActive()) m_attempt.try_emplace(key, value);
			m_baseline.try_emplace(key, std::move(value));
		}
		// Only read keys written by this operation. External changes to other keys
		// must not erase an earlier GUI baseline.
		template<class Reader> void Reconcile(Reader&& read)
		{
			for (const auto& key : m_pending)
			{
				auto value = read(key);
				if (const auto attempt = m_attempt.find(key); attempt != m_attempt.end() && attempt->second == value) m_attempt.erase(attempt);
				if (const auto found = m_baseline.find(key); found != m_baseline.end() && found->second == value)
				{
					m_baseline.erase(found);
				}
			}
			m_pending.clear();
		}
		// Drop a session baseline when a semantic restore now owns this derived value.
		// Keep the attempt snapshot so a later failure can still undo this operation.
		void DiscardBaseline(const Key& key) { m_baseline.erase(key); }
		template<class Visitor> void VisitRestore(bool attempt, Visitor&& visit) const
		{
			for (const auto& [key, value] : attempt ? m_attempt : m_baseline) visit(key, value);
		}
		void CommitAttempt() noexcept { m_previousBaseline.reset(); m_attempt.clear(); m_pending.clear(); }
		template<class Writer> bool RollbackAttempt(Writer&& write, bool externalRestored = true)
		{
			bool success = externalRestored;
			for (const auto& [key, value] : m_attempt) if (!write(key, value)) success = false;
			if (success && m_previousBaseline) m_baseline = std::move(*m_previousBaseline);
			else if (!success)
			{
				if (m_previousBaseline) for (const auto& [key, value] : *m_previousBaseline) m_baseline.insert_or_assign(key, value);
				for (const auto& [key, value] : m_attempt) m_baseline.try_emplace(key, value);
			}
			CommitAttempt();
			return success;
		}
		template<class Writer> bool Revert(Writer&& write, bool externalRestored = true, bool acceptOnSuccess = true)
		{
			bool success = externalRestored;
			for (const auto& [key, value] : m_baseline) if (!write(key, value)) success = false;
			if (success && acceptOnSuccess) Accept();
			return success;
		}
		void Accept() noexcept { m_baseline.clear(); CommitAttempt(); }
	};
}
