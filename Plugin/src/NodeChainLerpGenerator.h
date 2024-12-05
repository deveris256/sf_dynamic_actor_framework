#pragma once
#include "NiAVObject.h"
#include "LogWrapper.h"
#include "NaiveAngularSpringChain.h"
#include "MutexUtils.h"
#include "Evaluatable.h"

namespace utils
{
	inline constexpr double M_PI = 3.14159265358979323846;

	inline void setMatrix3dPlain(const RE::NiMatrix3& matrix, Eigen::Matrix3d& matrix_out)
	{
		matrix_out(0, 0) = matrix.entry[0][0];
		matrix_out(0, 1) = matrix.entry[1][0];
		matrix_out(0, 2) = matrix.entry[2][0];
		matrix_out(1, 0) = matrix.entry[0][1];
		matrix_out(1, 1) = matrix.entry[1][1];
		matrix_out(1, 2) = matrix.entry[2][1];
		matrix_out(2, 0) = matrix.entry[0][2];
		matrix_out(2, 1) = matrix.entry[1][2];
		matrix_out(2, 2) = matrix.entry[2][2];
	}

	inline void setMatrix4dPlain(const RE::NiMatrix3& matrix, Eigen::Matrix4d& matrix_out)
	{
		matrix_out(0, 0) = matrix.entry[0][0];
		matrix_out(0, 1) = matrix.entry[1][0];
		matrix_out(0, 2) = matrix.entry[2][0];
		matrix_out(1, 0) = matrix.entry[0][1];
		matrix_out(1, 1) = matrix.entry[1][1];
		matrix_out(1, 2) = matrix.entry[2][1];
		matrix_out(2, 0) = matrix.entry[0][2];
		matrix_out(2, 1) = matrix.entry[1][2];
		matrix_out(2, 2) = matrix.entry[2][2];
	}

	inline void setMatrix4d(const RE::NiTransform& transform, Eigen::Matrix4d& matrix_out)
	{
		setMatrix4dPlain(transform.rotate, matrix_out);

		matrix_out(0, 3) = transform.translate.x;
		matrix_out(1, 3) = transform.translate.y;
		matrix_out(2, 3) = transform.translate.z;
		matrix_out(3, 0) = 0;
		matrix_out(3, 1) = 0;
		matrix_out(3, 2) = 0;
		matrix_out(3, 3) = 1;
	}

	inline void setNiMatrixPlain(const Eigen::Matrix3d& matrix, RE::NiMatrix3& matrix_out)
	{
		matrix_out.entry[0][0] = matrix(0, 0);
		matrix_out.entry[0][1] = matrix(1, 0);
		matrix_out.entry[0][2] = matrix(2, 0);
		matrix_out.entry[1][0] = matrix(0, 1);
		matrix_out.entry[1][1] = matrix(1, 1);
		matrix_out.entry[1][2] = matrix(2, 1);
		matrix_out.entry[2][0] = matrix(0, 2);
		matrix_out.entry[2][1] = matrix(1, 2);
		matrix_out.entry[2][2] = matrix(2, 2);
	}

	inline void setNiTransform(const Eigen::Matrix4d& matrix, RE::NiTransform& transform_out)
	{
		setNiMatrixPlain(matrix.block<3, 3>(0, 0), transform_out.rotate);
		transform_out.translate.x = matrix(0, 3);
		transform_out.translate.y = matrix(1, 3);
		transform_out.translate.z = matrix(2, 3);
	}

	inline std::string formatToNumpy(const RE::NiTransform& transform)
	{
		std::stringstream ss;
		ss << "np.array([[" << transform.rotate.entry[0][0] << ", " << transform.rotate.entry[0][1] << ", " << transform.rotate.entry[0][2] << "], ";
		ss << "[" << transform.rotate.entry[1][0] << ", " << transform.rotate.entry[1][1] << ", " << transform.rotate.entry[1][2] << "], ";
		ss << "[" << transform.rotate.entry[2][0] << ", " << transform.rotate.entry[2][1] << ", " << transform.rotate.entry[2][2] << "], ";
		ss << "[" << transform.translate.x << ", " << transform.translate.y << ", " << transform.translate.z << "]], dtype=np.float32)";
		return ss.str();
	}
	
}

namespace daf
{
	class Node
	{
	public:
		RE::NiAVObject* node;
		RE::NiAVObject* naturalParent;
		RE::NiTransform originalLocalTransform;
		RE::NiTransform transformOverlay;

		Node() :
			node(nullptr) {}
		explicit Node(RE::NiAVObject* node) :
			node(node), naturalParent(node->parent), originalLocalTransform(node->local) {}
		Node(RE::NiAVObject* node, const RE::NiTransform& originalLocalTransform):
			node(node), naturalParent(node->parent), originalLocalTransform(originalLocalTransform) {}

		void bind(RE::NiAVObject* node)
		{
			this->node = node;
			naturalParent = node->parent;
			originalLocalTransform = node->local;
			transformOverlay = RE::NiTransform();

			//auto assert = (node->parent->world * node->local) / node->world;
		}
	};

	class NodeChainBase
	{
	public:
		Node              chainRoot;  // Shouldn't be updated
		std::vector<Node> chainNodes;

		virtual ~NodeChainBase() = default;
		virtual void build(const RE::NiAVObject* a_chainRoot, const std::vector<RE::NiAVObject*>& a_chainNodes, const std::vector<RE::NiTransform>& a_originalLocalTransforms) = 0;
		virtual void update(time_t lastTime, time_t currentTime) = 0;
		virtual void setOverlayTransform(const std::vector<RE::NiTransform>& transform_overlay) = 0;
	};

	class DirectNodeChain : public NodeChainBase
	{
	public:
		void build(const RE::NiAVObject* a_chainRoot, const std::vector<RE::NiAVObject*>& a_chainNodes, const std::vector<RE::NiTransform>& a_originalLocalTransforms) override
		{
			chainRoot.bind(const_cast<RE::NiAVObject*>(a_chainRoot));
			chainNodes.reserve(a_chainNodes.size());
			for (size_t i = 0; i < a_chainNodes.size(); ++i) {
				chainNodes.emplace_back(a_chainNodes[i], a_originalLocalTransforms[i]);
			}
		}

		void update(time_t lastTime, time_t currentTime) override
		{
			for (auto& node : chainNodes) {
				node.node->local = node.originalLocalTransform * node.transformOverlay;
			}
		}

		void setOverlayTransform(const std::vector<RE::NiTransform>& transform_overlay) override
		{
			for (size_t i = 0; i < chainNodes.size(); ++i) {
				auto& node = chainNodes[i];
				node.transformOverlay = transform_overlay[i];
			}
		}
	};

	class PhysicsNodeChain : public DirectNodeChain
	{
	public:
		physics::AngularSpringChain chain;

		time_t dt = 8;  // 8 ms per Euler integration step
		double speed_multiplier = 1.0;
		time_t residual = 0;
		time_t max_trace_time_per_update = 100;  // 100 ms max time per update

		double physics_mass = 2.0;
		double physics_stiffness = 1000.0;
		double physics_angularDamping = 2.0;
		double physics_linearDrag = 2.0;

		Eigen::Vector3d physics_gravity = Eigen::Vector3d(0.0, 0.0, -9.81);

		double multipier = 30.0;

		bool physics_enabled = true;

		PhysicsNodeChain(double a_physics_mass, double a_physics_stiffness, double a_physics_angularDamping, double a_physics_linearDrag) :
			DirectNodeChain(),
			physics_mass(a_physics_mass),
			physics_stiffness(a_physics_stiffness),
			physics_angularDamping(a_physics_angularDamping),
			physics_linearDrag(a_physics_linearDrag) {}

		void build(const RE::NiAVObject* a_chainRoot, const std::vector<RE::NiAVObject*>& a_chainNodes, const std::vector<RE::NiTransform>& a_originalLocalTransforms) override;

		void update(time_t lastTime, time_t currentTime) override;

		void setOverlayTransform(const std::vector<RE::NiTransform>& transform_overlay) override;

		void setStiffness(double stiffness)
		{
			stiffness = std::max(0.1, stiffness);
			physics_stiffness = stiffness;
			chain.setStiffness(stiffness);
		}

		void setAngularDamping(double angularDamping)
		{
			angularDamping = std::max(0.1, angularDamping);
			physics_angularDamping = angularDamping;
			chain.setAngularDamping(angularDamping);
		}

		void setLinearDrag(double linearDrag)
		{
			linearDrag = std::max(0.0, linearDrag);
			physics_linearDrag = linearDrag;
			chain.setLinearDrag(linearDrag);
		}
	};

	class NodeChainLerpGenerator
	{
	public:
		class TransformTarget
		{
		public:
			TransformTarget() :
				scale(1.0)
			{
				xyz_euler_rotation.setZero();
				position.setZero();
			}

			Eigen::Vector3f position;
			Eigen::Vector3f xyz_euler_rotation; // XYZ, degrees
			float           scale;

			inline Eigen::Matrix3f getRotationMatrix() const
			{
				Eigen::Matrix3f rotation;
				rotation = Eigen::AngleAxisf(xyz_euler_rotation[0] * utils::M_PI / 180.0f, Eigen::Vector3f::UnitX()) *
				           Eigen::AngleAxisf(xyz_euler_rotation[1] * utils::M_PI / 180.0f, Eigen::Vector3f::UnitY()) *
				           Eigen::AngleAxisf(xyz_euler_rotation[2] * utils::M_PI / 180.0f, Eigen::Vector3f::UnitZ());
				return rotation;
			}

			inline Eigen::Matrix4f getTransformMatrixNoScale() const
			{
				Eigen::Matrix4f transform = Eigen::Matrix4f::Identity();
				transform.block<3, 3>(0, 0) = getRotationMatrix();
				transform.block<3, 1>(0, 3) = position;
				return transform;
			}

			inline void setRotationMatrix(const Eigen::Matrix3f& rotation)
			{
				xyz_euler_rotation = rotation.eulerAngles(2, 1, 0);
			}

			inline void setTransformMatrixNoScale(const Eigen::Matrix4f& transform)
			{
				xyz_euler_rotation = transform.block<3, 3>(0, 0).eulerAngles(2, 1, 0);
				position = transform.block<3, 1>(0, 3);
			}

			inline RE::NiTransform toNiTransform() const
			{
				RE::NiTransform transform;
				auto            rotation = getRotationMatrix();
				transform.rotate.entry[0][0] = rotation(0, 0);
				transform.rotate.entry[0][1] = rotation(0, 1);
				transform.rotate.entry[0][2] = rotation(0, 2);
				transform.rotate.entry[1][0] = rotation(1, 0);
				transform.rotate.entry[1][1] = rotation(1, 1);
				transform.rotate.entry[1][2] = rotation(1, 2);
				transform.rotate.entry[2][0] = rotation(2, 0);
				transform.rotate.entry[2][1] = rotation(2, 1);
				transform.rotate.entry[2][2] = rotation(2, 2);
				std::memcpy(&transform.translate, &position, sizeof(position));
				transform.scale = scale;
				return transform;
			}

			// Overloaded operators
			TransformTarget operator+(const TransformTarget& other) const
			{
				TransformTarget result;
				result.xyz_euler_rotation = xyz_euler_rotation + other.xyz_euler_rotation;
				result.position = position + other.position;
				result.scale = scale + other.scale;
				return result;
			}

			TransformTarget operator-(const TransformTarget& other) const
			{
				TransformTarget result;
				result.xyz_euler_rotation = xyz_euler_rotation - other.xyz_euler_rotation;
				result.position = position - other.position;
				result.scale = scale - other.scale;
				return result;
			}

			TransformTarget operator*(double scalar) const
			{
				TransformTarget result;
				result.xyz_euler_rotation = xyz_euler_rotation * scalar;
				result.position = position * scalar;
				result.scale = scale * scalar;
				return result;
			}
		};

		struct ChainNodeData
		{
			std::string     nodeName;
			TransformTarget maxima;
			TransformTarget minima;
		};

		struct PhysicsData
		{
			bool   enabled{ false };
			float mass{ 2.0 };

			float       stiffness{ 1000.0 };
			std::string stiffnessExpression;

			float       angularDamping{ 2.0 };
			std::string angularDampingExpression;

			float       linearDrag{ 2.0 };
			std::string linearDragExpression;
		};

		class EvaluatablePhysicsParams
		{
		public:
			using SymbolTable = utils::Evaluatable<float>::_SymbolTable_T;

			mutex::NonReentrantSpinLock _this_lock;

			SymbolTable symbolTable;

			float t{ 0.f };  // 0 - 1

			utils::Evaluatable<float> mass;
			utils::Evaluatable<float> stiffness;
			utils::Evaluatable<float> angularDamping;
			utils::Evaluatable<float> linearDrag;

			EvaluatablePhysicsParams() :
				mass(2.0, symbolTable), stiffness(1000.0, symbolTable), angularDamping(2.0, symbolTable), linearDrag(2.0, symbolTable)
			{
				symbolTable.add_constants();
				symbolTable.add_variable("t", t);
			}
		};

		NodeChainLerpGenerator() :
			lastLocalTime(0), etaLocal(0), lastSystemTime(0), fadeNode(nullptr) {}

		NodeChainLerpGenerator(RE::Actor* a_actor, const std::string& a_chainRootName, const std::vector<ChainNodeData>& a_chainNodeData, const PhysicsData a_physicsData = PhysicsData()) :
			lastLocalTime(0), etaLocal(0), lastSystemTime(0)
		{
			build(a_actor, a_chainRootName, a_chainNodeData, a_physicsData);
		}

		RE::BGSFadeNode*               fadeNode;
		std::unique_ptr<NodeChainBase> chain;
		std::atomic<bool>              hasPhysics{ false };
		EvaluatablePhysicsParams       physicsParams;

		std::vector<TransformTarget> maxima;  // t = 1
		std::vector<TransformTarget> minima;  // t = 0

		std::vector<TransformTarget> curTransforms;
		std::atomic<time_t>          lastLocalTime;
		std::atomic<time_t>		     lastSystemTime;

		std::vector<TransformTarget> curTargets;
		std::vector<TransformTarget> freezeTargets;
		float						 physicsParamTarget{ 0.0 };	
		float                        physicsParamFreezeTarget{ 0.0 };
		std::atomic<time_t>          etaLocal;

		time_t fullErectionTimeMs{ 5000 };

		std::atomic<bool> isActive{ false };
		std::atomic<bool> isFreezed{ false };

		void clear()
		{
			maxima.clear();
			minima.clear();
			curTransforms.clear();
			curTargets.clear();
			lastLocalTime = 0;
			etaLocal = 0;
			// Release chain
			chain.release();
		}

		bool build(RE::Actor* a_actor, const std::string& a_chainRootName, const std::vector<ChainNodeData>& a_chainNodeData, const PhysicsData& a_physicsData, bool a_noPhysics = false);

		// Update the node chain with system time. Limited with delta_time_clamp to avoid large delta time. 0 means no limit.
		void updateWithSystemTime(time_t systemTime, time_t delta_time_clamp = 0);

		// Update the node chain.
		void updateWithDeltaTime(time_t deltaTime);

		// True if the state changed.
		bool setActive(bool active)
		{
			if (active == isActive) {
				return false;
			}

			isActive = active;
			return true;
		}

		void setPhysicsEnabled(bool enabled)
		{
			std::lock_guard _lock(_this_lock);
			auto physicsChain = getPhysicsChain();
			if (physicsChain) {
				physicsChain->physics_enabled = enabled;
				setNodeChainOverlayTransform(curTransforms);
			}
		}

		// Set interpolated targets at 't'.
		void setAtT_Delayed(float t, time_t transition_time = 0)
		{
			// Clamp t
			t = std::clamp(t, 0.0f, 1.0f);

			std::lock_guard _lock(_this_lock);

			setInterpolatedTargets_Impl(t);

			etaLocal = lastLocalTime + transition_time;
		}

		// Set interpolated targets at 't'.
		void setAtT_Propotional(float t, float speed_multiplier = 1.f)
		{
			// Clamp t
			t = std::clamp(t, 0.0f, 1.0f);
			speed_multiplier = std::clamp(speed_multiplier, 0.01f, 10.f);

			std::lock_guard _lock(_this_lock);

			setInterpolatedTargets_Impl(t);

			float diff_t = std::abs(physicsParams.t - t);
			etaLocal = lastLocalTime + time_t(diff_t * float(fullErectionTimeMs) / speed_multiplier);
		}

		// Freeze at custom targets. Return false if already freezed.
		bool freezeAtTargets_Delayed(const std::vector<TransformTarget>& targets, float physicsParamTarget, time_t transition_time = 0, bool forced = false)
		{
			if (!forced && isFreezed) {
				return false;
			}

			std::lock_guard _lock(_this_lock);
			if (targets.size() != curTargets.size()) {
				logger::error("Invalid target size {}, expecting targets vector with {} TransformTargets.", targets.size(), curTargets.size());
				return false;
			}

			isFreezed = true;
			this->freezeTargets = targets;
			if (physicsParamTarget >= 0.f && physicsParamTarget <= 1.f) {
				this->physicsParamFreezeTarget = physicsParamTarget;
			}
			etaLocal = lastLocalTime + transition_time;

			return true;
		}

		// Freeze at interpolated targets at 't'. Return false if already freezed.
		bool freezeAtT_Delayed(float t, time_t transition_time = 0, bool forced = false)
		{
			if (!forced && isFreezed) {
				return false;
			}

			// Clamp t
			t = std::clamp(t, 0.0f, 1.0f);

			std::lock_guard _lock(_this_lock);

			freezeAt_Impl(t);

			etaLocal = lastLocalTime + transition_time;
		}

		// Freeze at interpolated targets at 't', with auto transition time. Return false if already freezed.
		bool freezeAtT_Propotional(float t, float speed_multiplier = 1.f, bool forced = false)
		{
			if (!forced && isFreezed) {
				return false;
			}

			// Clamp t
			t = std::clamp(t, 0.0f, 1.0f);
			speed_multiplier = std::clamp(speed_multiplier, 0.01f, 10.f);

			std::lock_guard _lock(_this_lock);

			freezeAt_Impl(t);

			float diff_t = std::abs(physicsParams.t - physicsParamFreezeTarget);
			etaLocal = lastLocalTime + time_t(diff_t * float(fullErectionTimeMs) / speed_multiplier);
		}

		// Resume and proceed to original targets.
		void unfreeze(time_t transition_time = 0)
		{
			if (!isFreezed) {
				return;
			}

			std::lock_guard _lock(_this_lock);

			unfreeze_Impl();

			etaLocal = lastLocalTime + transition_time;
		}

		// Resume and proceed to original targets, with auto transition time.
		void unfreeze_Propotional(float speed_multiplier = 1.f)
		{
			if (!isFreezed) {
				return;
			}

			speed_multiplier = std::clamp(speed_multiplier, 0.01f, 10.f);

			std::lock_guard _lock(_this_lock);

			unfreeze_Impl();
			
			float diff_t = std::abs(physicsParamTarget - physicsParamFreezeTarget);
			etaLocal = lastLocalTime + time_t(diff_t * float(fullErectionTimeMs) / speed_multiplier);
		}

		bool targetArrived() {
			return etaLocal < lastLocalTime;
		}

		PhysicsNodeChain* getPhysicsChain()
		{
			if (!hasPhysics) {
				return nullptr;
			}
			return dynamic_cast<PhysicsNodeChain*>(chain.get());
		}

	private:
		mutex::NonReentrantSpinLock _this_lock;

		inline void setInterpolatedTargets_Impl(float t) 
		{
			interpolateTargets(t, curTargets);
			physicsParamTarget = t;
		}

		inline void freezeAt_Impl(float t)
		{
			isFreezed = true;
			interpolateTargets(t, freezeTargets);
			physicsParamFreezeTarget = t;
		}

		inline void unfreeze_Impl()
		{
			isFreezed = false;
		}

		inline void updatePhysicsData() {
			auto physicsChain = getPhysicsChain();
			if (!physicsChain) {
				return;
			}

			physicsChain->setStiffness(physicsParams.stiffness.Evaluate());
			physicsChain->setAngularDamping(physicsParams.angularDamping.Evaluate());
			physicsChain->setLinearDrag(physicsParams.linearDrag.Evaluate());
		}

		bool parsePhysicsData(const PhysicsData& data, double& mass_init_eval, double& stiffness_init_eval, double& angularDamping_init_eval, double& linearDrag_init_eval);

		inline void interpolateTargets(double t, std::vector<TransformTarget>& targets)
		{
			// Interpolate between the minima and maxima
			interpolate_Impl(t, minima, maxima, targets);
		}

		inline static void interpolate_Impl(double t, const std::vector<TransformTarget>& minima, const std::vector<TransformTarget>& maxima, std::vector<TransformTarget>& out)
		{
			for (size_t i = 0; i < maxima.size(); ++i) {
				out[i] = minima[i] + (maxima[i] - minima[i]) * t;
			}
		}

		inline static float interpolate_float_Impl(float t, float min, float max)
		{
			return min + (max - min) * t;
		}

		inline void setNodeChainOverlayTransform(const std::vector<TransformTarget>& targets)
		{
			std::vector<RE::NiTransform> transforms;
			transforms.reserve(targets.size());
			for (const auto& target : targets) {
				transforms.emplace_back(target.toNiTransform());
			}
			chain->setOverlayTransform(transforms);
		}

		inline bool requestUpdate()
		{
			auto m = fadeNode->bgsModelNode;
			if (!m) {
				return false;
			}

			auto u = m->unk10;
			if (!u)
				return false;

			u->needsUpdate = true;

			return true;
		}
	};
}