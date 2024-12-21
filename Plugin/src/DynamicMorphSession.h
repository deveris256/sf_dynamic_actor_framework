#pragma once
#include "SFEventHandler.h"

namespace daf
{
	inline constexpr std::string_view overweightMorphName{ "Overweight" };
	inline constexpr std::string_view strongMorphName{ "Strong" };
	inline constexpr std::string_view thinMorphName{ "Thin" };

	namespace tokens
	{
		inline constexpr std::string general_offset{ "ECOffset_" };
	}

	// A mini git session for actor morphs
	class DynamicMorphSession
	{
	public:
		enum class DiffMode
		{
			Max_Norm,
			L1_Norm,
			L2_Norm
		};

		struct MorphValue
		{
			MorphValue() = default;
			MorphValue(float snapshot) :
				snapshot(snapshot), evaluated(snapshot) {}

			float snapshot{ 0.f };
			float evaluated{ 0.f };

			float Diff()
			{
				return evaluated - snapshot;
			}

			float DiffAbs()
			{
				return std::abs(Diff());
			}
		};

		DynamicMorphSession(std::string a_token, RE::Actor* a_actor, DiffMode a_diffMode = DiffMode::Max_Norm) :
			offsetPrefix(a_token),
			diffMode(a_diffMode)
		{
			Snapshot(a_actor);
		}

		~DynamicMorphSession()
		{
			for (auto& ptr : _new_strings) {
				delete[] ptr;
			}
		}

		inline std::string_view GetOffsetName(std::string_view morph_name)
		{
			auto offset_name = offsetPrefix + morph_name.data();
			if (auto it = m_morph_offset_names.find(offset_name); it != m_morph_offset_names.end()) {
				return it->first;
			}
			return _find_or_alloc(offset_name);
		}

		void MorphOffsetCommit(std::string morph_name, float offset)
		{
			auto offset_name = GetOffsetName(morph_name);
			auto morph_name_sv = _find_or_alloc(morph_name);

			m_morph_snapshot[offset_name].evaluated += offset;
			m_morph_snapshot[morph_name_sv].evaluated += offset;

			m_morph_offset_names[offset_name] = morph_name_sv;
		}

		void MorphTargetCommit(std::string morph_name, float target)
		{
			auto& entry = m_morph_snapshot[morph_name];

			entry.evaluated = target;

			if (float diff = entry.Diff(); diff != 0.f) {
				auto offset_name = GetOffsetName(morph_name);
				auto morph_name_sv = _find_or_alloc(morph_name);
				m_morph_snapshot[offset_name].evaluated += diff;
				m_morph_offset_names[offset_name] = morph_name_sv;
			}
		}

		void RevertCommits()
		{
			for (auto& [morph_name, target] : m_morph_snapshot) {
				target.evaluated = target.snapshot;
			}
		}

		// Revert all morph offsets
		void RestoreMorph()
		{
			for (auto& [offset_name, morph_name] : m_morph_offset_names) {
				auto& entry = m_morph_snapshot[offset_name];
				m_morph_snapshot[morph_name].evaluated -= entry.evaluated;
				entry.evaluated = 0;
			}
		}

		float Diff()
		{
			float diff = 0.f;
			switch (diffMode) {
			case DiffMode::Max_Norm:
				for (auto& [morph_name, target] : m_morph_snapshot) {
					diff = std::max(diff, target.DiffAbs());
				}
				return diff;
			case DiffMode::L1_Norm:
				for (auto& [morph_name, target] : m_morph_snapshot) {
					diff += target.DiffAbs();
				}
				return diff;
			case DiffMode::L2_Norm:
				for (auto& [morph_name, target] : m_morph_snapshot) {
					diff += target.Diff() * target.Diff();
				}
				return std::sqrt(diff);
			}
		}

		// Reduce resource occupation time and avoid race condition
		float PushCommits(float a_minDiffToCommit = 0.f)
		{
			float diff = Diff();

			if (diff < a_minDiffToCommit) {
				return diff;
			}

			std::unordered_map<std::string_view, float> commit_batch;

			for (auto& [morph_name, morph_value] : m_morph_snapshot) {
				if (morph_value.Diff() != 0.f) {
					commit_batch[morph_name] = morph_value.evaluated;
					morph_value.snapshot = morph_value.evaluated;
				}
			}

			
			{ // Critical section
				auto npc = m_actor->GetNPC();
				for (auto& [morph_name, target] : commit_batch) {
					if (morph_name == overweightMorphName) {
						npc->morphWeight.fat = target;
					} else if (morph_name == strongMorphName) {
						npc->morphWeight.muscular = target;
					} else if (morph_name == thinMorphName) {
						npc->morphWeight.thin = target;
					} else {
						if (!npc->shapeBlendData) {
							npc->shapeBlendData = new RE::BSTHashMap<RE::BSFixedStringCS, float>();
						}

						(*npc->shapeBlendData)[morph_name] = target;
					}
				}
			} // End of critical section
			

			return diff;
		}

		DiffMode          diffMode = DiffMode::Max_Norm;
		const std::string offsetPrefix;

	private:
		RE::Actor*                                             m_actor;
		std::unordered_map<std::string_view, MorphValue>       m_morph_snapshot;
		std::unordered_map<std::string_view, std::string_view> m_morph_offset_names;
		std::vector<char*>                                     _new_strings;

		std::unordered_set<std::string> m_string_pool;

		// Reduce resource occupation time and avoid race condition
		bool Snapshot(RE::Actor* a_actor)
		{
			m_actor = a_actor;

			{ // Critical section
				auto npc = m_actor->GetNPC();
				if (!npc) {
					return false;
				}

				m_morph_snapshot[overweightMorphName] = npc->morphWeight.fat;
				m_morph_snapshot[strongMorphName] = npc->morphWeight.muscular;
				m_morph_snapshot[thinMorphName] = npc->morphWeight.thin;

				if (npc->shapeBlendData) {
					auto& morph_data = *npc->shapeBlendData;
					for (auto& [morph_name, offset] : morph_data) {
						m_morph_snapshot[morph_name] = offset;
					}
				}
			} // End of critical section

			for (auto& [morph_name, target] : m_morph_snapshot) {
				if (morph_name.starts_with(offsetPrefix)) {
					m_morph_offset_names[morph_name] = morph_name.data() + offsetPrefix.size();
				}
			}

			return true;
		}

		std::string_view _find_or_alloc(std::string& _str)
		{
			auto it = m_string_pool.find(_str);
			if (it != m_string_pool.end()) {
				return *it;
			}
			return *m_string_pool.insert(_str).first;
		}
	};
}