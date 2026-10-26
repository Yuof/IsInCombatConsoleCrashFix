// Fixes a vanilla crash while a console command runs (SkyrimSE.exe+0CC90CF on 1.6.1170: BSReadWriteLock::LockForRead
// on null + 0x88 under TESObjectREFR::GetDisplayFullName). In console mode condition functions print their result, and
// IsInCombat prints "%s is not in combat" with its subject's name without checking that the subject is an actor - a
// reference that is not one arrives as null. That happens with IsInCombat typed in the console on a non-actor, and when
// a console command (coc, cow, a mod's ConsoleUtil call) runs quest conditions that test IsInCombat on non-actors.
// The plugin wraps IsInCombat's console handler and condition function in the script command table: in console mode a
// reference that is not an actor gets 0 (the game's own answer), the console handler prints its name; everything else
// goes to the game's functions unchanged. The command is found by its name, so no code offsets are involved.

namespace
{
	RE::SCRIPT_FUNCTION::Execute_t*   Execute = nullptr;
	RE::SCRIPT_FUNCTION::Condition_t* Condition = nullptr;
	std::atomic<std::uint64_t>        catches{ 0 };

	bool Milestone(std::uint64_t a_n)  // 1, 10, 100, ...
	{
		while (a_n % 10 == 0) {
			a_n /= 10;
		}
		return a_n == 1;
	}

	// the case the game's code crashes on: console mode and a subject that is not an actor
	bool Caught(RE::TESObjectREFR* a_ref, double& a_result)
	{
		if (!RE::ConsoleLog::IsConsoleMode() || (a_ref && a_ref->GetFormType() == RE::FormType::ActorCharacter)) {
			return false;
		}
		a_result = 0.0;
		// ponytail: a log line at the 1st, 10th, 100th ... catch - one console coc can run a quest fill over thousands of references
		if (const auto n = ++catches; Milestone(n)) {
			logger::info("IsInCombat in console mode on {:08X} \"{}\", not an actor: answered 0 ({} so far)"sv,
				a_ref ? a_ref->GetFormID() : 0, a_ref ? a_ref->GetDisplayFullName() : "", n);
		}
		return true;
	}

	bool ExecuteThunk(const RE::SCRIPT_PARAMETER* a_paramInfo, RE::SCRIPT_FUNCTION::ScriptData* a_scriptData,
		RE::TESObjectREFR* a_thisObj, RE::TESObjectREFR* a_containingObj, RE::Script* a_scriptObj, RE::ScriptLocals* a_locals,
		double& a_result, std::uint32_t& a_opcodeOffsetPtr)
	{
		if (Caught(a_thisObj, a_result)) {
			RE::ConsoleLog::GetSingleton()->Print("%s is not in combat", a_thisObj ? a_thisObj->GetDisplayFullName() : "");
			return true;
		}
		return Execute(a_paramInfo, a_scriptData, a_thisObj, a_containingObj, a_scriptObj, a_locals, a_result, a_opcodeOffsetPtr);
	}

	bool ConditionThunk(RE::TESObjectREFR* a_thisObj, void* a_param1, void* a_param2, double& a_result)
	{
		return Caught(a_thisObj, a_result) || Condition(a_thisObj, a_param1, a_param2, a_result);
	}
}

SKSEPluginInfo(
	.Version = Plugin::VERSION,
	.Name = Plugin::NAME,
	.Author = "Yuof"sv,
	.RuntimeCompatibility = SKSE::VersionIndependence::AddressLibrary)

SKSE_PLUGIN_LOAD(const SKSE::LoadInterface* a_skse)
{
	SKSE::Init(a_skse, { .log = true });
	logger::info("{} v{} on {}"sv, Plugin::NAME, Plugin::VERSION.string(), REL::Module::get().version().string());
	auto* command = RE::SCRIPT_FUNCTION::LocateScriptCommand("IsInCombat"sv);
	if (!command || !command->executeFunction || !command->conditionFunction || !command->referenceFunction || command->numParams != 1) {
		logger::error("self-check failed (IsInCombat not found as expected in the script command table): inactive"sv);
		return true;
	}
	Execute = command->executeFunction;
	Condition = command->conditionFunction;
	command->executeFunction = ExecuteThunk;
	command->conditionFunction = ConditionThunk;
	logger::info("self-check passed, wrapped IsInCombat's console handler and condition function"sv);
	return true;
}
