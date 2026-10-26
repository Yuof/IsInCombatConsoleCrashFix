// Fixes the crash at SkyrimSE+0CC90CF on 1.6.1170 (BSReadWriteLock::LockForRead on null + 0x88 under
// TESObjectREFR::GetDisplayFullName). While a console command runs, condition functions print their result to the
// console; IsInCombat's prints "%s is not in combat" with the subject's name but, unlike its other branch, without
// checking that the subject is an actor - a non-actor subject arrives as null. That happens for a non-actor ref
// selected in the console, and for quest alias candidates when a console coc / cow starts a quest (a quest that tests
// IsInCombat on refs in the loaded area). Every call from that function to GetDisplayFullName gets a null check.
// Addresses come from Address Library; the calls are found by their target at load, a build without them stays inactive.

namespace
{
	constexpr std::uint64_t kIsInCombat = 21576, kGetDisplayFullName = 19781;  // AE ids
	constexpr std::size_t   kSpan = 0xD0;                                     // the function's size on 1.6.1170 and 1.7.104

	struct GetDisplayFullName
	{
		static const char* thunk(RE::TESObjectREFR* a_ref)
		{
			if (!a_ref) {
				logger::info("IsInCombat printed for a subject that is not an actor: empty name"sv);
				return "";
			}
			return func(a_ref);
		}
		static inline REL::Relocation<decltype(thunk)> func;
	};

	std::vector<std::uintptr_t> CallSites()
	{
		std::vector<std::uintptr_t> sites;
		const auto begin = REL::ID(kIsInCombat).address();
		const auto target = REL::ID(kGetDisplayFullName).address();
		for (auto at = begin; at + 5 <= begin + kSpan; ++at) {
			const auto rel = *reinterpret_cast<const std::int32_t*>(at + 1);
			if (*reinterpret_cast<const std::uint8_t*>(at) == 0xE8 && at + 5 + static_cast<std::intptr_t>(rel) == target) {
				sites.push_back(at);
			}
		}
		return sites;
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
	// the ids above are AE ids: SE / VR number theirs differently, and a missing id stops the game in CommonLib's lookup
	if (!REL::Module::IsAE()) {
		logger::error("not an AE runtime: inactive"sv);
		return true;
	}
	const auto sites = CallSites();
	if (sites.empty()) {
		logger::error("self-check failed (no GetDisplayFullName call in IsInCombat's condition function): inactive"sv);
		return true;
	}
	SKSE::AllocTrampoline(14 * sites.size());
	for (const auto site : sites) {
		GetDisplayFullName::func = SKSE::GetTrampoline().write_call<5>(site, GetDisplayFullName::thunk);
	}
	logger::info("self-check passed, guarded {} GetDisplayFullName call(s) in IsInCombat"sv, sites.size());
	return true;
}
