#include "MorphEvaluationRuleSet.h"

bool daf::MorphEvaluationRuleSet::ParseScript(std::string a_filename, bool a_clearExisting, CollisionBehavior a_behavior)
{
	std::ifstream file(a_filename);
	if (!file.is_open()) {
		logger::error("Failed to open file: {}", a_filename);
		return false;
	}

	// Read json file
	nlohmann::json j;
	try {
		file >> j;
	} catch (const nlohmann::json::parse_error& e) {
		logger::error("Failed to parse json format: {}", e.what());
		file.close();
		return false;
	}
	file.close();

	if (a_clearExisting) {
		Clear();
	}

	bool success = true;

	// Parse aliases
	if (j.contains("Aliases")) {
		auto& aliases = j["Aliases"];
		for (auto& [alias, alias_obj] : aliases.items()) {
			std::string editorID;
			Alias::Type alias_type = Alias::Type::kAny;
			float       default_to = 0.f;
			if (alias_obj.contains("EditorID")) {
				editorID = alias_obj["EditorID"].get<std::string>();
			} else {
				logger::error("When parsing Alias '{}': No EditorID found in definition.", alias);
			}
			if (alias_obj.contains("Type")) {
				auto type_str = alias_obj["Type"].get<std::string>();
				if (type_str == "actorValue") {
					alias_type = Alias::Type::kActorValue;
				} else if (type_str == "wornKeyword") {
					alias_type = Alias::Type::kWornKeyword;
				} else if (type_str == "visibleWornKeyword") {
					alias_type = Alias::Type::kVisibleWornKeyword;
				} else if (type_str == "npcKeyword") {
					alias_type = Alias::Type::kNPCKeyword;
				} else if (type_str == "morph") {
					alias_type = Alias::Type::kMorph;
				} else if (type_str == "headpart") {
					alias_type = Alias::Type::kHeadpart;
				} else {
					logger::warn("When parsing Alias '{}': Unknown Alias type: '{}', using automatic type.", alias, type_str);
				}
			}
			if (alias_obj.contains("Default")) {
				default_to = alias_obj["Default"].get<float>();
			}
			if (!ParseAlias(alias, editorID, alias_type, default_to)) {
				logger::error("When parsing Alias '{}': {}", alias, last_error);
				success = false;
			}
		}
	}

	// Parse rules
	if (j.contains("Rules")) {
		auto& rules = j["Rules"];
		if (rules.contains("Adders")) {
			auto& adder = rules["Adders"];
			for (auto& [morph_name, expr_str] : adder.items()) {
				if (!ParseRule(morph_name, expr_str, false, a_behavior)) {
					logger::error("When parsing Rule for '{}': {}", morph_name, last_error);
					success = false;
				}
			}
		}
		if (rules.contains("Setters")) {
			auto& setter = rules["Setters"];
			for (auto& [morph_name, expr_str] : setter.items()) {
				if (!ParseRule(morph_name, expr_str, true, CollisionBehavior::kOverwrite)) {
					logger::error("When parsing Rule for '{}': {}", morph_name, last_error);
					success = false;
				}
			}
		}
	}

	return success;
}

bool daf::MorphEvaluationRuleSet::ParseAlias(std::string a_alias, std::string a_editorID, Alias::Type a_aliasType, float defaultTo)
{
	auto alias = _get_string_view(a_alias);
	auto editorID = _get_string_view(a_editorID);

	if (auto same_alias = FindSameAlias(editorID, a_aliasType); same_alias) {  // Collapse aliases referencing same actorValue/keword/morph
		same_alias->equivalent_symbols.emplace_back(alias);
		if (!m_symbol_table.add_variable(alias.data(), m_value_snapshot[same_alias->symbol])) {
			last_error = std::format("Symbol redefinition or illegal symbol name: '{}'.", alias);
			return false;
		}
		return true;
	}

	m_value_snapshot[alias] = defaultTo;
	if (!m_symbol_table.add_variable(alias.data(), m_value_snapshot[alias])) {
		m_value_snapshot.erase(alias);
		last_error = std::format("Symbol redefinition or illegal symbol name: '{}'.", alias);
		//logger::error(last_error.c_str());
		return false;
	}

	if (auto avi = ParseSymbolAsActorValue(editorID); (std::to_underlying(a_aliasType) & std::to_underlying(Alias::Type::kActorValue)) && avi) {
		m_aliases[alias] = {
			alias,
			editorID,
			Alias::Type::kActorValue,
			[avi](RE::Actor* actor, RE::TESNPC* npc, ActorArmorVisableLayer) -> float {
				return AcquireActorValue(actor, npc, avi);
			}
		};
		m_loaded = true;
		return true;
	} else if (auto keyword = ParseSymbolAsKeyword(editorID); (std::to_underlying(a_aliasType) & std::to_underlying(Alias::Type::kWornKeyword)) && keyword) {
		m_aliases[alias] = {
			alias,
			editorID,
			Alias::Type::kWornKeyword,
			[keyword](RE::Actor* actor, RE::TESNPC* npc, ActorArmorVisableLayer) -> float {
				return AcquireWornKeywordValue(actor, npc, keyword, ActorArmorVisableLayer::kAny);
			}
		};
		m_loaded = true;
		return true;
	} else if (auto keyword = ParseSymbolAsKeyword(editorID); (std::to_underlying(a_aliasType) & std::to_underlying(Alias::Type::kVisibleWornKeyword)) && keyword) {
		m_aliases[alias] = {
			alias,
			editorID,
			Alias::Type::kWornKeyword,
			[keyword](RE::Actor* actor, RE::TESNPC* npc, ActorArmorVisableLayer visLayer) -> float {
				return AcquireWornKeywordValue(actor, npc, keyword, visLayer);
			}
		};
		m_loaded = true;
		return true;
	} else if (auto keyword = ParseSymbolAsKeyword(editorID); (std::to_underlying(a_aliasType) & std::to_underlying(Alias::Type::kNPCKeyword)) && keyword) {
		m_aliases[alias] = {
			alias,
			editorID,
			Alias::Type::kNPCKeyword,
			[keyword](RE::Actor* actor, RE::TESNPC* npc, ActorArmorVisableLayer) -> float {
				return AcquireNPCKeywordValue(actor, npc, keyword);
			}
		};
		m_loaded = true;
		return true;
	} else if (auto headpart = ParseSymbolAsHeadPart(editorID); (std::to_underlying(a_aliasType) & std::to_underlying(Alias::Type::kHeadpart)) && headpart) {
		m_aliases[alias] = {
			alias,
			editorID,
			Alias::Type::kHeadpart,
			[headpart](RE::Actor* actor, RE::TESNPC* npc, ActorArmorVisableLayer) -> float {
				return AcquireHeadPartValue(actor, npc, headpart);
			}
		};
		m_loaded = true;
		return true;
	} else if (std::to_underlying(a_aliasType) & std::to_underlying(Alias::Type::kMorph)) {
		m_aliases[alias] = {
			alias,
			editorID,
			Alias::Type::kMorph,
			[editorID](RE::Actor* actor, RE::TESNPC* npc, ActorArmorVisableLayer) -> float {
				return AcquireMorphValue(actor, npc, editorID);
			}
		};
		m_loaded = true;
		return true;
	}

	m_symbol_table.remove_variable(alias.data());
	m_value_snapshot.erase(alias);
	if (std::to_underlying(a_aliasType) & std::to_underlying(Alias::Type::kActorValue)) {
		last_error = std::format("Cannot parse as ActorValue: Alias: '{}' EditorID: '{}'", alias, editorID);
	} else if (std::to_underlying(a_aliasType) & std::to_underlying(Alias::Type::kKeyword)) {
		last_error = std::format("Cannot parse as Keyword: Alias: '{}' EditorID: '{}'", alias, editorID);
	}
	return false;
}

bool daf::MorphEvaluationRuleSet::ParseRule(std::string a_targetMorphName, std::string a_exprStr, bool a_isSetter, CollisionBehavior a_behavior)
{
	auto target_morph_name = _get_string_view(a_targetMorphName);

	Rule rule(this, a_isSetter);
	rule.target_morph_name = target_morph_name;
	if (!rule.Parse(a_exprStr, m_symbol_table)) {
		last_error = rule.GetCompilerError();
		//logger::error(last_error.c_str());
		return false;
	}
	if (auto external_symbol = IsMorphLoopRule(rule); external_symbol) {
		last_error = std::format("Circular reference detected in morph rule with symbol: '{}'", external_symbol->symbol);
		//logger::error(last_error.c_str());
		return false;
	}

	// If the rule is a Setter, then it always overwrites existing rules for the same morph
	// If an Adder is Appending to an existing Setter, it will be ignored and invalidated
	if (a_isSetter) {
		a_behavior = CollisionBehavior::kOverwrite;
	}

	if (!m_rules.contains(target_morph_name)) {  // If doesn't have any rules for the morph yet
		m_rules[target_morph_name] = std::move(std::vector<Rule>{ rule });
	} else if (a_behavior == CollisionBehavior::kOverwrite) {  // If overwriting existing rules
		m_rules[target_morph_name].clear();
		m_rules[target_morph_name].emplace_back(rule);
	} else if (!m_rules[target_morph_name].empty()) {       // If appending to existing rules and there are existing rules
		if (m_rules[target_morph_name].back().is_setter) {  // If the last rule is a Setter, then ignore the Adder
			/*m_rules[target_morph_name].clear();
					m_rules[target_morph_name].emplace_back(rule);*/
			last_error = std::format("Attempting to overwrite a Setter with an Adder: '{}'. The Adder will be ignored.", target_morph_name);
			return false;
		} else {  // If the last rule is an Adder, then append the new Adder
			m_rules[target_morph_name].emplace_back(rule);
		}
	} else {  // If appending to existing rules and there are no existing rules
		m_rules[target_morph_name].emplace_back(rule);
	}

	m_loaded = true;
	return true;
}

daf::MorphEvaluationRuleSet::Alias* daf::MorphEvaluationRuleSet::IsMorphLoopRule(const Rule& a_rule) const
{
	return IsMorphLoopRule_Impl(a_rule, a_rule.target_morph_name);
}

void daf::MorphEvaluationRuleSet::Snapshot(RE::Actor* a_actor)
{
	auto npc = a_actor->GetNPC();
	{
		std::lock_guard<std::mutex> lock(m_snapshot_mutex);

		auto visible_layer = ActorArmorVisableLayer::kAny;

		if (utils::ShouldActorShowSpacesuit(a_actor)) {
			visible_layer = ActorArmorVisableLayer::kSpaceSuit;
		} else {
			visible_layer = ActorArmorVisableLayer::kApparel;
		}

		for (auto& [symbol, alias] : m_aliases) {
			m_value_snapshot[symbol] = alias.acquisition_func(a_actor, npc, visible_layer);
		}
	}
}

void daf::MorphEvaluationRuleSet::Evaluate(std::unordered_map<std::string_view, Result>& evaluated_values)
{
	evaluated_values.clear();

	std::lock_guard<std::mutex> lock(m_snapshot_mutex);
	for (auto& [morph_name, rules] : m_rules) {
		bool  is_setter = false;
		float value = 0.f;
		for (auto& rule : rules) {
			if (!rule.use_expr) {
				continue;
			}

			if (rule.is_setter) {
				is_setter = true;
				value = rule.Evaluate();
				break;
			} else {
				value += rule.Evaluate();
			}
		}

		if (!is_setter && value == 0.f) {
			continue;
		}

		evaluated_values[morph_name] = { is_setter, value };
	}
}

daf::MorphEvaluationRuleSet::Alias* daf::MorphEvaluationRuleSet::IsMorphLoopRule_Impl(const Rule& a_rule, const std::string_view& target_morph_name) const
{
	auto& external_symbols = a_rule.external_symbols;
	for (auto alias : external_symbols) {
		if (std::to_underlying(alias->type) & std::to_underlying(Alias::Type::kMorph)) {
			auto& morph_name = alias->editorID;
			if (morph_name == target_morph_name) {
				return alias;
			} else {
				auto it = m_rules.find(morph_name);
				if (it != m_rules.end()) {
					for (auto& r : it->second) {
						if (IsMorphLoopRule_Impl(r, target_morph_name)) {
							return alias;
						}
					}
				}
			}
		}
	}
	return nullptr;
}

float daf::MorphEvaluationRuleSet::AcquireNPCKeywordValue(RE::Actor* actor, RE::TESNPC* npc, RE::BGSKeyword* keyword)
{
	if (npc->HasKeyword(keyword->formEditorID)) {
		return 1.f;
	}
	return 0.f;
}

float daf::MorphEvaluationRuleSet::AcquireWornKeywordValue(RE::Actor* actor, RE::TESNPC* npc, RE::BGSKeyword* keyword, ActorArmorVisableLayer visible_layer)
{
	float found = 0.f;

	actor->ForEachEquippedItem([visible_layer, keyword, &found](const RE::BGSInventoryItem& item) -> RE::BSContainer::ForEachResult {
		auto armor = item.object->As<RE::TESObjectARMO>();
		if (!armor) {
			return RE::BSContainer::ForEachResult::kContinue;
		}
		auto instanceData = reinterpret_cast<RE::TESObjectARMOInstanceData*>(item.instanceData.get());

		switch (visible_layer) {
		case ActorArmorVisableLayer::kApparel:
			if (utils::IsARMOSpacesuit(armor, instanceData)) {
				return RE::BSContainer::ForEachResult::kContinue;
			}
			break;
		case ActorArmorVisableLayer::kSpaceSuit:
			if (!utils::IsARMOSpacesuit(armor, instanceData)) {
				return RE::BSContainer::ForEachResult::kContinue;
			}
			break;
		}

		if (utils::ARMOHasKeyword(armor, instanceData, keyword)) {
			found = 1.f;
			return RE::BSContainer::ForEachResult::kStop;
		}
		return RE::BSContainer::ForEachResult::kContinue;
	});

	return found;
}

float daf::MorphEvaluationRuleSet::AcquireMorphValue(RE::Actor* actor, RE::TESNPC* npc, std::string_view morph_name)
{
	if (morph_name == overweightMorphName) {
		return npc->morphWeight.fat;
	} else if (morph_name == strongMorphName) {
		return npc->morphWeight.muscular;
	} else if (morph_name == thinMorphName) {
		return npc->morphWeight.thin;
	} else {
		if (!npc->shapeBlendData) {
			return 0.f;
		}

		auto& morph_data = *npc->shapeBlendData;
		auto  it = morph_data.find(morph_name);
		if (it != morph_data.end()) {
			return it->value;
		}
		return 0.f;
	}
}

float daf::MorphEvaluationRuleSet::AcquireHeadPartValue(RE::Actor* actor, RE::TESNPC* npc, RE::BGSHeadPart* headpart)
{
	auto  acc = npc->headParts.lock();
	auto& headparts = *acc;

	for (auto it = headparts.begin(); it != headparts.end() && *it != nullptr; ++it) {
		if (*it == headpart) {
			return 1.f;
		}
	}

	return 0.f;
}

void daf::MorphRuleSetManager::LoadRulesets(std::string a_rootFolder)
{
	std::filesystem::path root_path(a_rootFolder);
	if (!std::filesystem::exists(root_path)) {
		logger::error("Root folder does not exist: {}", a_rootFolder);
		return;
	}

	ClearAllRulesets();

	// For each subfolder in root folder, the folder name is the editorID of the race
	for (auto& entry : std::filesystem::directory_iterator(root_path)) {
		if (!entry.is_directory()) {
			continue;
		}

		auto folder_name = entry.path().filename().string();

		RE::TESRace* race = RE::TESObjectREFR::LookupByEditorID<RE::TESRace>(folder_name);

		if (!race) {
			logger::warn("Folder name '{}' doesn't resolve to any race editorID.", folder_name);
			continue;
		}

		std::string           race_master_file;
		std::filesystem::path male_entry;
		std::filesystem::path female_entry;

		// For each subfolder in race folder, find "male" or "female" folder
		for (auto& sex_entry : std::filesystem::directory_iterator(entry.path())) {
			if (!sex_entry.is_directory()) {
				auto master_name = sex_entry.path().filename().string();
				if (utils::caseInsensitiveCompare(master_name, "race_master.json")) {
					race_master_file = sex_entry.path().string();
				}
				continue;
			}

			auto sex_folder_name = sex_entry.path().filename().string();
			std::transform(sex_folder_name.begin(), sex_folder_name.end(), sex_folder_name.begin(), [](unsigned char c) { return std::tolower(c); });

			if (sex_folder_name == "male") {
				male_entry = sex_entry.path();
			} else if (sex_folder_name == "female") {
				female_entry = sex_entry.path();
			}
		}

		if (!race_master_file.empty()) {
			bool is_new;
			auto male_ruleset = GetOrCreate(race, RE::SEX::kMale, is_new);
			auto female_ruleset = GetOrCreate(race, RE::SEX::kFemale, is_new);
			logger::info("Loading race master for race '{}' for males: '{}'", folder_name, race_master_file);
			male_ruleset->ParseScript(race_master_file, true);
			logger::info("Loading race master for race '{}' for females: '{}'", folder_name, race_master_file);
			female_ruleset->ParseScript(race_master_file, true);
		} else {
			logger::warn("No race master ruleset found for race '{}'", folder_name);
		}

		if (!male_entry.empty()) {
			LoadRaceSexRulesets(male_entry, race, RE::SEX::kMale);
		} else {
			logger::warn("No male folder found for race '{}'", folder_name);
		}

		if (!female_entry.empty()) {
			LoadRaceSexRulesets(female_entry, race, RE::SEX::kFemale);
		} else {
			logger::warn("No female folder found for race '{}'", folder_name);
		}
	}
}

bool daf::MorphRuleSetManager::LoadRaceSexRulesets(std::filesystem::path a_sexFolder, RE::TESRace* a_race, RE::SEX a_sex)
{
	std::vector<std::string> ruleset_files;
	std::string              master_file;
	for (auto& ruleset_entry : std::filesystem::directory_iterator(a_sexFolder)) {
		if (ruleset_entry.is_regular_file() && ruleset_entry.path().extension() == ".json") {
			if (utils::caseInsensitiveCompare(ruleset_entry.path().filename().string(), "master.json")) {
				master_file = ruleset_entry.path().string();
			} else {
				ruleset_files.push_back(ruleset_entry.path().string());
			}
		}
	}

	// Sort ruleset_files alphabetically
	std::sort(ruleset_files.begin(), ruleset_files.end());

	// Load "master.json" first
	if (master_file.empty()) {
		logger::error("No master ruleset found in folder: '{}'", a_sexFolder.string());
		return false;
	}

	bool is_new{ false };
	logger::info("Loading master ruleset in: '{}'", master_file);
	GetOrCreate(a_race, a_sex, is_new)->ParseScript(master_file, is_new);

	for (auto& ruleset_file : ruleset_files) {
		bool is_new{ false };
		logger::info("Loading ruleset in: '{}'", ruleset_file);
		GetOrCreate(a_race, a_sex, is_new)->ParseScript(ruleset_file, false, MorphEvaluationRuleSet::CollisionBehavior::kAppend);
	}

	return true;
}

daf::MorphEvaluationRuleSet* daf::MorphRuleSetManager::GetOrCreate(RE::TESRace* a_race, const RE::SEX a_sex, bool& is_new)
{
	size_t sex_id = static_cast<std::size_t>(a_sex);
	is_new = false;

	RuleSetCollection_T::accessor acc;
	if (m_per_race_sex_ruleset.find(acc, a_race)) {
		if (!acc->second[sex_id]) {
			acc->second[sex_id].reset(new MorphEvaluationRuleSet());
			is_new = true;
		}

		return acc->second[sex_id].get();

	} else {
		m_per_race_sex_ruleset.insert(acc,
			std::make_pair(a_race,
				std::array<std::unique_ptr<MorphEvaluationRuleSet>, 2>{
					nullptr,
					nullptr }));
		acc->second[sex_id].reset(new MorphEvaluationRuleSet());
		is_new = true;
		return acc->second[sex_id].get();
	}
}
