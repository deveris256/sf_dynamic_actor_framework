#pragma once
#include "LogWrapper.h"
#include "SingletonBase.h"

#include "DynamicMorphSession.h"

namespace daf
{
	class MorphEvaluationRuleSet
	{
	public:
		using Parser = exprtk::parser<float>;
		using Expression = exprtk::expression<float>;
		using SymbolTable = exprtk::symbol_table<float>;
		using Symbol = std::string_view;

		using AcquisitionFunction = std::function<float(RE::Actor*, RE::TESNPC*)>;

		enum class CollisionBehavior : uint8_t
		{
			kOverwrite,
			kAppend
		};

		class Alias
		{
		public:
			enum class Type : uint8_t
			{
				kNone = 0,
				kActorValue = 1 << 0,
				kWornKeyword = 1 << 1,
				kNPCKeyword = 1 << 2,
				kMorph = 1 << 3,
				kAny = kActorValue | kWornKeyword | kNPCKeyword | kMorph
			};

			Alias() = default;
			Alias(std::string_view a_symbol, std::string_view a_editorID, Type a_type, AcquisitionFunction a_acquisition_func) :
				symbol(a_symbol), editorID(a_editorID), type(a_type), acquisition_func(a_acquisition_func) {}

			inline bool is_same_as(const Alias& a_rhs) const
			{
				return type == a_rhs.type && editorID == a_rhs.editorID;
			}

			inline bool is_same_reference(std::string_view a_editorID, Type a_type) const
			{
				return (std::to_underlying(type) & std::to_underlying(a_type)) && editorID == a_editorID;
			}

			Symbol             symbol;
			std::string_view   editorID;
			Type               type{ Type::kNone };
			AcquisitionFunction acquisition_func;

			std::vector<Symbol> equivalent_symbols;
		};

		class Rule
		{
		public:
			MorphEvaluationRuleSet* parent_rule_set{ nullptr };

			std::string_view target_morph_name;
			Expression       expr;
			bool             is_setter{ false };

			std::vector<Alias*>      external_symbols;
			std::vector<std::string> internal_symbols;

			std::string compiler_error;
			bool        is_valid{ false };

			Rule() = default;
			Rule(MorphEvaluationRuleSet* a_parentRuleSet, bool a_isSetter) :
				parent_rule_set(a_parentRuleSet), is_setter(a_isSetter) {}

			bool Parse(const std::string& expr_str, SymbolTable& symbol_table)
			{
				expr.register_symbol_table(symbol_table);
				Parser parser(Parser::settings_t::e_collect_vars);
				if (!parser.compile(expr_str, expr)) {
					compiler_error = parser.error();
					return false;
				}
				auto& all_symbols = parser.dec().symbol_list();
				for (auto& symbol : all_symbols) {
					if (symbol.second == Parser::symbol_type::e_st_variable) {
						if (auto it = parent_rule_set->m_aliases.find(symbol.first); it != parent_rule_set->m_aliases.end()) {
							external_symbols.emplace_back(&it->second);
						} else {
							internal_symbols.emplace_back(symbol.first);
						}
					}
				}
				is_valid = true;
				return true;
			}

			float Evaluate()
			{
				if (!is_valid) {
					return 0.f;
				}
				return expr.value();
			}

			const std::string& GetCompilerError() const
			{
				return compiler_error;
			}
		};

		struct Result
		{
			bool  is_setter{ false };
			float value{ 0.f };
		};

		using ResultTable = std::unordered_map<std::string_view, Result>;

		MorphEvaluationRuleSet()
		{
			m_symbol_table.add_constants();
		}

		~MorphEvaluationRuleSet() = default;

		void Clear()
		{
			//m_actor_value_symbols.clear();
			//m_keyword_symbols.clear();
			//m_morph_symbols.clear();
			m_aliases.clear();
			m_rules.clear();
			m_symbol_table.clear();
			m_value_snapshot.clear();
			m_loaded = false;
		}

		bool ParseScript(std::string a_filename, bool a_clearExisting, CollisionBehavior a_behavior = CollisionBehavior::kOverwrite)
		{
			/*
			* JSON format:
			* {"Aliases": { symbol: { "EditorID": editorID, ["Type": actorValue|wornKeyword|npcKeyword|morph, "Default": default_value] }, ...},
			*  "Rules": { "Adders": {morphName0: "expression0", ...}, "Setters": {morphName1: "expression1", ...}}
			*/
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
					float default_to = 0.f;
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
						} else if (type_str == "npcKeyword") {
							alias_type = Alias::Type::kNPCKeyword;
						} else if (type_str == "morph") {
							alias_type = Alias::Type::kMorph;
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
				if (rules.contains("Adders"))
				{
					auto& adder = rules["Adders"];
					for (auto& [morph_name, expr_str] : adder.items()) {
						if (!ParseRule(morph_name, expr_str, false, a_behavior)) {
							logger::error("When parsing Rule for '{}': {}", morph_name, last_error);
							success = false;
						}
					}
				}
				if (rules.contains("Setters"))
				{
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

		bool ParseAlias(std::string a_alias, std::string a_editorID, Alias::Type a_aliasType = Alias::Type::kAny, float defaultTo = 0.f)
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
				//m_actor_value_symbols[alias] = avi;
				m_aliases[alias] = { 
					alias, 
					editorID, 
					Alias::Type::kActorValue, 
					[avi](RE::Actor* actor, RE::TESNPC* npc) -> float {
						return AcquireActorValue(actor, npc, avi);
					}
				};
				m_loaded = true;
				return true;
			} else if (auto keyword = ParseSymbolAsKeyword(editorID); (std::to_underlying(a_aliasType) & std::to_underlying(Alias::Type::kWornKeyword)) && keyword) {
				//m_keyword_symbols[alias] = keyword;
				m_aliases[alias] = {
					alias,
					editorID,
					Alias::Type::kWornKeyword,
					[keyword](RE::Actor* actor, RE::TESNPC* npc) -> float {
						return AcquireWornKeywordValue(actor, npc, keyword);
					}
				};
				m_loaded = true;
				return true;
			} else if (auto keyword = ParseSymbolAsKeyword(editorID); (std::to_underlying(a_aliasType) & std::to_underlying(Alias::Type::kNPCKeyword)) && keyword) {
				//m_keyword_symbols[alias] = keyword;
				m_aliases[alias] = {
					alias,
					editorID,
					Alias::Type::kNPCKeyword,
					[keyword](RE::Actor* actor, RE::TESNPC* npc) -> float {
						return AcquireNPCKeywordValue(actor, npc, keyword);
					}
				};
				m_loaded = true;
				return true;
			} else if (std::to_underlying(a_aliasType) & std::to_underlying(Alias::Type::kMorph)) {
				//m_morph_symbols[alias] = editorID;
				m_aliases[alias] = {
					alias,
					editorID,
					Alias::Type::kMorph,
					[editorID](RE::Actor* actor, RE::TESNPC* npc) -> float {
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
			} else if (std::to_underlying(a_aliasType) & std::to_underlying(Alias::Type::kNPCKeyword) || 
				std::to_underlying(a_aliasType) & std::to_underlying(Alias::Type::kWornKeyword)) {
				last_error = std::format("Cannot parse as Keyword: Alias: '{}' EditorID: '{}'", alias, editorID);
			}
			//logger::error(last_error.c_str());
			return false;
		}

		bool ParseRule(std::string a_targetMorphName, std::string a_exprStr, bool a_isSetter, CollisionBehavior a_behavior = CollisionBehavior::kOverwrite)
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

			if (!m_rules.contains(target_morph_name)) { // If doesn't have any rules for the morph yet
				m_rules[target_morph_name] = std::vector<Rule>{ rule };
			} else if (a_behavior == CollisionBehavior::kOverwrite) { // If overwriting existing rules
				m_rules[target_morph_name].clear();
				m_rules[target_morph_name].emplace_back(rule);
			} else if (!m_rules[target_morph_name].empty()) { // If appending to existing rules and there are existing rules
				if (m_rules[target_morph_name].back().is_setter) { // If the last rule is a Setter, then ignore the Adder
					/*m_rules[target_morph_name].clear();
					m_rules[target_morph_name].emplace_back(rule);*/
					last_error = std::format("Attempting to overwrite a Setter with an Adder: '{}'. The Adder will be ignored.", target_morph_name);
					return false;
				} else { // If the last rule is an Adder, then append the new Adder
					m_rules[target_morph_name].emplace_back(rule);
				}
			} else { // If appending to existing rules and there are no existing rules
				m_rules[target_morph_name].emplace_back(rule);
			}

			m_loaded = true;
			return true;
		}

		inline Alias* IsMorphLoopRule(const Rule& a_rule) const {
			return IsMorphLoopRule_Impl(a_rule, a_rule.target_morph_name);
		}

		const std::string& GetLastError() const {
			return last_error;
		}

		void Snapshot(RE::Actor* a_actor) {
			auto npc = a_actor->GetNPC();
			{
				std::lock_guard<std::mutex> lock(m_snapshot_mutex);
				for (auto& [symbol, alias] : m_aliases) {
					m_value_snapshot[symbol] = alias.acquisition_func(a_actor, npc);
				}
			}
		}

		template <typename _arithmetic_t>
		requires std::is_arithmetic_v<_arithmetic_t>
		bool SetSymbolSnapshot(Symbol a_symbol, _arithmetic_t a_value)
		{
			std::lock_guard<std::mutex> lock(m_snapshot_mutex);
			if (!m_value_snapshot.contains(a_symbol)) {
				return false;
			}
			m_value_snapshot[a_symbol] = static_cast<float>(a_value);
			return true;
		}

		std::unordered_map<Symbol, float> GetSnapshot()
		{
			std::lock_guard<std::mutex> lock(m_snapshot_mutex);
			return m_value_snapshot;
		}

		bool HasSymbol(Symbol a_symbol)
		{
			std::lock_guard<std::mutex> lock(m_snapshot_mutex);
			return m_value_snapshot.contains(a_symbol);
		}

		void Evaluate(std::unordered_map<std::string_view, Result>& evaluated_values)
		{
			evaluated_values.clear();

			std::lock_guard<std::mutex> lock(m_snapshot_mutex);
			for (auto& [morph_name, rules] : m_rules) {
				bool is_setter = false;
				float value = 0.f;
				for (auto& rule : rules) {
					if (!rule.is_valid) {
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

		bool IsLoaded() const
		{
			return m_loaded;
		}

		std::mutex m_ruleset_mutex;

	private:
		// Per actor
		std::mutex                        m_snapshot_mutex;
		std::unordered_map<Symbol, float> m_value_snapshot;

		// Shared
		std::unordered_map<std::string_view, std::vector<Rule>> m_rules;
		std::unordered_map<Symbol, Alias>          m_aliases;

		std::string last_error;
		SymbolTable m_symbol_table;

		bool m_loaded{ false };

		std::unordered_set<std::string> m_string_pool;

		std::string_view _get_string_view(const std::string& str)
		{
			auto it = m_string_pool.find(str);
			if (it != m_string_pool.end()) {
				return *it;
			}
			return *m_string_pool.insert(str).first;
		}

		Alias* IsMorphLoopRule_Impl(const Rule& a_rule, const std::string_view& target_morph_name) const
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

		static bool IsSymbolConstant(std::string_view symbol)
		{
			return	symbol == "pi"sv || 
					symbol == "epsilon"sv || 
					symbol == "inf"sv;
		}

		static inline RE::BGSKeyword* ParseSymbolAsKeyword(std::string_view symbol)
		{
			return RE::TESObjectREFR::LookupByEditorID<RE::BGSKeyword>(symbol);
		}

		static inline RE::ActorValueInfo* ParseSymbolAsActorValue(std::string_view symbol)
		{
			return RE::TESObjectREFR::LookupByEditorID<RE::ActorValueInfo>(symbol);
		}

		static inline float AcquireNPCKeywordValue(RE::Actor* actor, RE::TESNPC* npc, RE::BGSKeyword* keyword)
		{
			if (npc->HasKeyword(keyword->formEditorID)) {
				return 1.f;
			}
			return 0.f;
		}

		static inline float AcquireWornKeywordValue(RE::Actor* actor, RE::TESNPC* npc, RE::BGSKeyword* keyword)
		{
			if (actor->WornHasKeyword(keyword)) {
				return 1.f;
			}

			return 0.f;
		}

		static inline float AcquireActorValue(RE::Actor* actor, RE::TESNPC* npc, RE::ActorValueInfo* actor_value_info)
		{
			return actor->GetActorValue(*actor_value_info);
		}

		static inline float AcquireMorphValue(RE::Actor* actor, RE::TESNPC* npc, std::string_view morph_name)
		{
			if (morph_name == overweightMorphName) {
				return npc->morphWeight.fat;
			}
			else if (morph_name == strongMorphName) {
				return npc->morphWeight.muscular;
			}
			else if (morph_name == thinMorphName) {
				return npc->morphWeight.thin;
			}
			else {
				if (!npc->shapeBlendData) {
					return 0.f;
				}

				auto& morph_data = *npc->shapeBlendData;
				auto it = morph_data.find(morph_name);
				if (it != morph_data.end()) {
					return it->value;
				}
				return 0.f;
			}
		}

		Alias* FindSameAlias(const Alias& a_alias)
		{
			for (auto& [symbol, alias] : m_aliases) {
				if (alias.is_same_as(a_alias)) {
					return &alias;
				}
			}
			return nullptr;
		}

		Alias* FindSameAlias(std::string_view a_editorID, Alias::Type a_type)
		{
			for (auto& [symbol, alias] : m_aliases) {
				if (alias.is_same_reference(a_editorID, a_type)) {
					return &alias;
				}
			}
			return nullptr;
		}
	};

	class MorphRuleSetManager :
		public utils::SingletonBase<MorphRuleSetManager>
	{
		friend class utils::SingletonBase<MorphRuleSetManager>;
	public:
		using RuleSetCollection_T = tbb::concurrent_hash_map<RE::TESRace*, std::array<std::unique_ptr<MorphEvaluationRuleSet>, 2>>;

		RuleSetCollection_T m_per_race_sex_ruleset;
 
		MorphEvaluationRuleSet* Get(RE::TESRace* a_race, const RE::SEX a_sex)
		{
			RuleSetCollection_T::accessor acc;
			if (m_per_race_sex_ruleset.find(acc, a_race)) {
				return acc->second[static_cast<std::size_t>(a_sex)].get();
			}
			return nullptr;
		}

		MorphEvaluationRuleSet* GetForActor(RE::Actor* a_actor)
		{
			auto npc = a_actor->GetNPC();
			return Get(npc->formRace, npc->GetSex());
		}

		void ClearAllRulesets()
		{
			m_per_race_sex_ruleset.clear();
		}

		void LoadRulesets(std::string a_rootFolder)
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

		// Loading order is always: master.json -> alphabetical order of other files
		bool LoadRaceSexRulesets(std::filesystem::path a_sexFolder, RE::TESRace* a_race, RE::SEX a_sex)
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

	private:
		MorphRuleSetManager() = default;

		// Always returns a valid ruleset, existing or new
		MorphEvaluationRuleSet* GetOrCreate(RE::TESRace* a_race, const RE::SEX a_sex, bool& is_new)
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
						std::array < std::unique_ptr<MorphEvaluationRuleSet>, 2>{
							nullptr,
							nullptr }));
				acc->second[sex_id].reset(new MorphEvaluationRuleSet());
				is_new = true;
				return acc->second[sex_id].get();
			}
		}
	};
}