#include "pch.h"
#include "TestHarness.hpp"
#include "../OpenGlassGUI/PreviewJournal.hpp"
#include "../OpenGlassGUI/EditorScope.hpp"

using namespace OpenGlass;

namespace OpenGlassTests
{
	void TestPreviewJournal()
	{
		PreviewJournal<int, std::optional<int>> journal;
		std::map<int, int> registry{ { 1, 10 } };
		auto read = [&](int key) -> std::optional<int> { if (registry.contains(key)) return registry[key]; return {}; };
		auto write = [&](int key, std::optional<int> value) { if (value) registry[key] = *value; else registry.erase(key); return true; };
		auto apply = [&](int value) { journal.Begin(); journal.Touch(1, read); registry[1] = value; journal.CommitAttempt(); };
		apply(20); // A
		journal.Begin(); journal.Touch(1, read); registry[1] = 30; journal.Touch(2, read); registry[2] = 7;
		Check(journal.RollbackAttempt(write)); // B failed, return to A, including missing values.
		Check(registry[1] == 20 && !registry.contains(2) && journal.IsDirty());
		apply(40); // C
		Check(journal.Revert(write)); Check(registry[1] == 10 && !journal.IsDirty());
		apply(20); apply(30); apply(40); // Successful A -> B -> C keeps the initial checkpoint.
		Check(journal.Revert(write)); Check(registry[1] == 10 && !journal.IsDirty());
		apply(50); journal.Accept(); apply(60);
		Check(!journal.Revert([](int, auto) { return false; })); Check(journal.IsDirty());
		Check(journal.Revert(write)); Check(registry[1] == 50);
		journal.Begin(); journal.Touch(2, read); registry[2] = 99;
		Check(!journal.RollbackAttempt(write, false)); // Failed independent color recovery keeps baseline.
		Check(journal.IsDirty()); Check(journal.Revert(write)); Check(!registry.contains(2));
		apply(70);
		Check(journal.Revert(write, true, false) && journal.IsDirty()); // Retain until refresh succeeds.
		journal.Accept();
		apply(80);
		journal.Begin(); journal.Touch(1, read); registry[1] = 90;
		journal.DiscardBaseline(1); // Automatic takes over, but this attempt can still fail.
		Check(!journal.IsDirty());
		Check(journal.RollbackAttempt(write) && registry[1] == 80 && journal.IsDirty());
		Check(journal.Revert(write) && registry[1] == 50);
		journal.Begin(); journal.Touch(1, read); registry[1] = 100;
		journal.DiscardBaseline(1); journal.Reconcile(read); journal.CommitAttempt();
		Check(!journal.IsDirty());
		Check(journal.Revert(write) && registry[1] == 100);
		using ScopedKey = std::pair<Settings::Scope, int>;
		PreviewJournal<ScopedKey, int> scoped;
		std::map<ScopedKey, int> layers{ { { Settings::Scope::User, 1 }, 10 }, { { Settings::Scope::Machine, 1 }, 20 } };
		for (const auto scope : { Settings::Scope::User, Settings::Scope::Machine })
		{
			const auto original = layers;
			const ScopedKey key{ scope, 1 };
			scoped.Begin(); scoped.Touch(key, [&](const auto& k) { return layers.at(k); }); layers[key] = 90; scoped.CommitAttempt();
			Check(layers.at({ scope == Settings::Scope::User ? Settings::Scope::Machine : Settings::Scope::User, 1 })
				== original.at({ scope == Settings::Scope::User ? Settings::Scope::Machine : Settings::Scope::User, 1 }));
			Check(scoped.Revert([&](const auto& k, int value) { layers[k] = value; return true; })); Check(layers == original);
		}
	}

	void TestNetPreviewJournal()
	{
		PreviewJournal<int, int> journal;
		std::map<int, int> state{ {1, 10}, {2, 20}, {3, 30} };
		auto read = [&](int key) { return state.at(key); };
		auto write = [&](int key, int value) { state[key] = value; return true; };
		auto edit = [&](int key, int value)
		{
			journal.Begin(); journal.Touch(key, read); state[key] = value; journal.Reconcile(read); journal.CommitAttempt();
		};
		journal.Begin(); journal.Reconcile(read);
		Check(!journal.HasAttemptChanges() && !journal.IsDirty());
		Check(journal.RollbackAttempt([](int, int) { return false; })); // Nothing to restore.
		edit(1, 10); Check(!journal.IsDirty());
		edit(1, 11); Check(journal.IsDirty());
		journal.Begin(); journal.Touch(1, read); journal.Reconcile(read);
		Check(!journal.HasAttemptChanges() && journal.IsDirty());
		Check(journal.RollbackAttempt([](int, int) { return false; }) && journal.IsDirty());
		journal.Begin(); journal.Touch(1, read); state[1] = 10; journal.Reconcile(read);
		Check(journal.HasAttemptChanges() && !journal.IsDirty()); // Returning to baseline still needs a refresh.
		journal.CommitAttempt(); Check(!journal.HasAttemptChanges());
		edit(1, 12); state[2] = 200; state[1] = 999; // An external edit to an owned key does not discard its baseline.
		edit(3, 31); Check(journal.Revert(write)); Check(state[1] == 10 && state[2] == 200 && state[3] == 30);
		edit(1, 11);
		journal.Begin(); journal.Touch(1, read); state[1] = 10; journal.Reconcile(read);
		Check(!journal.RollbackAttempt([](int, int) { return false; }));
		Check(journal.IsDirty()); Check(journal.Revert(write)); Check(state[1] == 10);
		edit(1, 40); journal.Accept(); edit(1, 41); Check(journal.Revert(write)); Check(state[1] == 40);
	}
}
