#pragma once
#include "LogWrapper.h"
#include "SingletonBase.h"
#include "Evaluatable.h"
#include "MutexUtils.h"

#include "DynamicMorphSession.h"

namespace daf
{
	class MorphEvaluationRuleSet
	{
	public:
		using SymbolTable = exprtk::symbol_table<float>;
		using Symbol = std::string_view;

		enum class CollisionBehavior : uint8_t
		{
			kOverwrite,
			kAppend
		};

		enum class ActorArmorVisableLayer : uint8_t
		{
			kApparel = 1 << 0,
			kSpaceSuit = 1 << 1,
			kAny = kApparel | kSpaceSuit
		};

		using AcquisitionFunction = std::function<float(RE::Actor*, RE::TESNPC*, ActorArmorVisableLayer)>;

		class Alias
		{
		public:
			enum class Type : uint8_t
			{
				kNone = 0,
				kActorValue = 1 << 0,
				kWornKeyword = 1 << 1,
				kVisibleWornKeyword = 1 << 2,
				kNPCKeyword = 1 << 3,
				kMorph = 1 << 4,
				kHeadpart = 1 << 5,
				kEquipmentKeyword = kWornKeyword | kVisibleWornKeyword,
				kKeyword = kWornKeyword | kVisibleWornKeyword | kNPCKeyword,
				kAny = kActorValue | kWornKeyword | kVisibleWornKeyword | kNPCKeyword | kMorph
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

		class Rule : public utils::Evaluatable<float>
		{
		public:
			MorphEvaluationRuleSet* parent_rule_set{ nullptr };

			std::string_view target_morph_name;
			bool             is_setter{ false };

			std::vector<Alias*>      external_symbols;
			std::vector<std::string> internal_symbols;

			Rule(MorphEvaluationRuleSet* a_parentRuleSet, bool a_isSetter) :
				Evaluatable<float>(0.f),
				parent_rule_set(a_parentRuleSet), is_setter(a_isSetter) {}

			bool Parse(const std::string& expr_str, _SymbolTable_T& symbol_table) override
			{
				this->symbol_table = &symbol_table;
				expr.register_symbol_table(symbol_table);
				_Parser_T parser(_Parser_T::settings_t::e_collect_vars);
				if (!parser.compile(expr_str, expr)) {
					compiler_error = parser.error();
					use_expr = false;
					return false;
				}
				auto& all_symbols = parser.dec().symbol_list();
				for (auto& symbol : all_symbols) {
					if (symbol.second == _Parser_T::symbol_type::e_st_variable) {
						if (auto it = parent_rule_set->m_aliases.find(symbol.first); it != parent_rule_set->m_aliases.end()) {
							external_symbols.emplace_back(&it->second);
						} else {
							internal_symbols.emplace_back(symbol.first);
						}
					}
				}
				use_expr = true;
				return true;
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
		
		/*
		* JSON format:
		  {
			"Aliases": { 
				symbol: { "EditorID": editorID, ["Type": actorValue|wornKeyword|visibleWornKeyword|npcKeyword|morph, "Default": default_value] }, 
				...
			},
			"Rules": { 
				"Adders": {
					morphName0: "expression0", 
					...
				}, 
				"Setters": {
					morphName1: "expression1", 
					...
				}
			}
		*/
		bool ParseScript(std::string a_filename, bool a_clearExisting, CollisionBehavior a_behavior = CollisionBehavior::kOverwrite);

		bool ParseAlias(std::string a_alias, std::string a_editorID, Alias::Type a_aliasType = Alias::Type::kAny, float defaultTo = 0.f);

		bool ParseRule(std::string a_targetMorphName, std::string a_exprStr, bool a_isSetter, CollisionBehavior a_behavior = CollisionBehavior::kOverwrite);

		Alias* IsMorphLoopRule(const Rule& a_rule) const;

		const std::string& GetLastError() const {
			return last_error;
		}

		void Snapshot(RE::Actor* a_actor);

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

		void Evaluate(std::unordered_map<std::string_view, Result>& evaluated_values);

		bool IsLoaded() const
		{
			return m_loaded;
		}

		mutex::NonReentrantSpinLock m_ruleset_spinlock;

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

		Alias* IsMorphLoopRule_Impl(const Rule& a_rule, const std::string_view& target_morph_name) const;

		static inline bool IsSymbolConstant(std::string_view symbol)
		{
			return	symbol == "pi"sv || 
					symbol == "epsilon"sv || 
					symbol == "inf"sv;
		}

		static inline RE::BGSKeyword* ParseSymbolAsKeyword(std::string_view symbol)
		{
			return RE::TESObjectREFR::LookupByEditorID<RE::BGSKeyword>(symbol);
		}

		static inline RE::BGSHeadPart* ParseSymbolAsHeadPart(std::string_view symbol)
		{
			return RE::TESObjectREFR::LookupByEditorID<RE::BGSHeadPart>(symbol);
		}

		static inline RE::ActorValueInfo* ParseSymbolAsActorValue(std::string_view symbol)
		{
			return RE::TESObjectREFR::LookupByEditorID<RE::ActorValueInfo>(symbol);
		}

		static float AcquireNPCKeywordValue(RE::Actor* actor, RE::TESNPC* npc, RE::BGSKeyword* keyword);

		static float AcquireWornKeywordValue(RE::Actor* actor, RE::TESNPC* npc, RE::BGSKeyword* keyword, ActorArmorVisableLayer visible_layer);

		static inline float AcquireActorValue(RE::Actor* actor, RE::TESNPC* npc, RE::ActorValueInfo* actor_value_info)
		{
			return actor->GetActorValue(*actor_value_info);
		}

		static float AcquireMorphValue(RE::Actor* actor, RE::TESNPC* npc, std::string_view morph_name);

		static float AcquireHeadPartValue(RE::Actor* actor, RE::TESNPC* npc, RE::BGSHeadPart* headpart);

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

		void LoadRulesets(std::string a_rootFolder);

		// Loading order is always: master.json -> alphabetical order of other files
		bool LoadRaceSexRulesets(std::filesystem::path a_sexFolder, RE::TESRace* a_race, RE::SEX a_sex);

	private:
		MorphRuleSetManager() = default;

		// Always returns a valid ruleset, existing or new
		MorphEvaluationRuleSet* GetOrCreate(RE::TESRace* a_race, const RE::SEX a_sex, bool& is_new);
	};
}