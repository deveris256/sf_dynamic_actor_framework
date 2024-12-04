#pragma once
#include "NiAVObject.h"
#include "LogWrapper.h"
#include "NaiveAngularSpringChain.h"
#include "MutexUtils.h"

namespace utils
{
	extern constexpr double M_PI = 3.14159265358979323846;

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

	std::string formatToNumpy(const RE::NiTransform& transform)
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
		virtual void build(const RE::NiAVObject* a_chainRoot, const std::vector<RE::NiAVObject*>& a_chainNodes) = 0;
		virtual void update(time_t lastTime, time_t currentTime) = 0;
		virtual void setOverlayTransform(const std::vector<RE::NiTransform>& transform_overlay) = 0;
	};

	class DirectNodeChain : public NodeChainBase
	{
	public:
		void build(const RE::NiAVObject* a_chainRoot, const std::vector<RE::NiAVObject*>& a_chainNodes) override
		{
			chainRoot.bind(const_cast<RE::NiAVObject*>(a_chainRoot));
			chainNodes.reserve(a_chainNodes.size());
			for (auto node : a_chainNodes) {
				chainNodes.emplace_back(node);
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

		void build(const RE::NiAVObject* a_chainRoot, const std::vector<RE::NiAVObject*>& a_chainNodes) override
		{
			DirectNodeChain::build(a_chainRoot, a_chainNodes);

			// Build the physics chain
			Eigen::Matrix4d root_transform;
			utils::setMatrix4d(chainRoot.node->world, root_transform);

			root_transform.block<3, 1>(0, 3) *= multipier;

			std::vector<Eigen::Matrix4d> joint_transforms;
			joint_transforms.reserve(chainNodes.size());
			for (auto& node : chainNodes) {
				Eigen::Matrix4d joint_transform;
				utils::setMatrix4d(node.node->world, joint_transform);

				joint_transform.block<3, 1>(0, 3) *= multipier;

				joint_transforms.push_back(joint_transform);
			}

			chain.build(root_transform, joint_transforms, physics_mass, physics_stiffness, physics_angularDamping, physics_linearDrag, physics_gravity);
		}

		void update(time_t lastTime, time_t currentTime) override
		{
			if (!physics_enabled) {
				DirectNodeChain::update(lastTime, currentTime);
				return;
			}

			// Get root node transform
			auto& root_world_transform = chainRoot.node->world;
			utils::setMatrix4d(root_world_transform, chain.getRootJoint());
			chain.getRootJoint().block<3, 1>(0, 3) *= multipier;

			if (currentTime - lastTime > max_trace_time_per_update) {
				lastTime = currentTime - max_trace_time_per_update;
			}

			// Step the physics simulation
			lastTime = lastTime - residual;
			size_t step = 0;
			while (lastTime < currentTime - dt) {
				chain.applyConstraints(double(dt) * speed_multiplier / 1000.0);
				lastTime += dt;
				++step;
				if (step % 3 == 0) {
					chain.normalizeSpringJointRotationAll();
				}
			}
			residual = currentTime - lastTime;
			chain.normalizeSpringJointRotationAll();

			chain.forEachSpring([this](size_t id, physics::AngularSpring& spring) {
				auto& node = this->chainNodes[id];
				auto& joint_rot = spring.cur_parent_rot.transpose() * spring.prev_joint_rot;
				utils::setNiMatrixPlain(joint_rot, node.node->local.rotate);
				node.node->local.translate.x = spring.rest_transform(0, 3) / multipier;
				node.node->local.translate.y = spring.rest_transform(1, 3) / multipier;
				node.node->local.translate.z = spring.rest_transform(2, 3) / multipier;
				return true;
			});

			/*chain.forEachJoint([this](size_t id, const Eigen::Matrix4d& joint_transform) {
				if (id == 0) {
					return true;
				}
				auto& node = this->chainNodes[id - 1];
				auto& prev_joint_transform = chain.joints[id - 1];
				Eigen::Matrix4d rel_transform = prev_joint_transform.inverse() * joint_transform;
				rel_transform.block<3, 1>(0, 3) /= multipier;
				utils::setNiTransform(rel_transform, node.node->local);
				return true;
			});*/
		}

		void setOverlayTransform(const std::vector<RE::NiTransform>& transform_overlay) override
		{
			if (!physics_enabled) {
				DirectNodeChain::setOverlayTransform(transform_overlay);
				return;
			}

			for (size_t i = 0; i < chainNodes.size(); ++i) {
				auto& node = chainNodes[i];
				node.transformOverlay = transform_overlay[i];
			}

			chain.forEachSpring([this](size_t id, physics::AngularSpring& spring) {
				const auto& joint = this->chainNodes[id];
				auto        new_local_transform = joint.originalLocalTransform * joint.transformOverlay;

				joint.node->local.scale = new_local_transform.scale;
				utils::setMatrix4d(new_local_transform, spring.rest_transform);
				spring.rest_transform.block<3, 1>(0, 3) *= multipier;

				return true;
			});
		}

		void setStiffness(double stiffness)
		{
			physics_stiffness = stiffness;
			chain.setStiffness(stiffness);
		}

		void setAngularDamping(double angularDamping)
		{
			physics_angularDamping = angularDamping;
			chain.setAngularDamping(angularDamping);
		}

		void setLinearDrag(double linearDrag)
		{
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
			lastTime(0), eta(0), fadeNode(nullptr) {}

		NodeChainLerpGenerator(RE::BGSFadeNode* a_fadeNode, const std::string& a_chainRootName, const std::vector<ChainNodeData>& a_chainNodeData, const PhysicsData a_physicsData = PhysicsData()) :
			fadeNode(a_fadeNode), lastTime(0), eta(0)
		{
			build(a_fadeNode, a_chainRootName, a_chainNodeData, a_physicsData);
		}

		RE::BGSFadeNode*               fadeNode;
		std::unique_ptr<NodeChainBase> chain;
		std::atomic<bool>              hasPhysics{ false };
		EvaluatablePhysicsParams       physicsParams;

		size_t                       numNodes{ 0 };
		std::vector<TransformTarget> maxima;  // t = 1
		std::vector<TransformTarget> minima;  // t = 0

		std::vector<TransformTarget> curTransforms;
		time_t                       lastTime;

		std::vector<TransformTarget> curTargets;
		std::vector<TransformTarget> freezeTargets;
		float						 physicsParamTarget{ 0.0 };	
		float                        physicsParamFreezeTarget{ 0.0 };
		time_t                       eta;

		time_t fullErectionTimeMs{ 5000 };

		std::atomic<bool> isActive{ false };
		std::atomic<bool> isFreezed{ false };

		void clear()
		{
			maxima.clear();
			minima.clear();
			curTransforms.clear();
			curTargets.clear();
			lastTime = 0;
			eta = 0;
			numNodes = 0;
			// Release chain
			chain.release();
		}

		bool build(RE::BGSFadeNode* a_actor3DRoot, const std::string& a_chainRootName, const std::vector<ChainNodeData>& a_chainNodeData, const PhysicsData& a_physicsData, bool a_noPhysics = false)
		{
			std::lock_guard _lock(_this_lock);

			if (!a_actor3DRoot) {
				logger::error("Invalid actor 3D root");
				return false;
			}

			clear();

			fadeNode = a_actor3DRoot;

			RE::NiNode* chainRoot = a_actor3DRoot->GetObjectByName(a_chainRootName);
			if (!chainRoot) {
				logger::error("Failed to find chain root node {}", a_chainRootName);
				return false;
			}

			numNodes = a_chainNodeData.size();
			maxima.reserve(numNodes);
			minima.reserve(numNodes);
			curTransforms.reserve(numNodes);
			curTargets.reserve(numNodes);
			freezeTargets.reserve(numNodes);

			std::vector<RE::NiAVObject*> chainNodes;
			for (size_t i = 0; i < a_chainNodeData.size(); ++i) {
				const auto& data = a_chainNodeData[i];

				RE::NiNode* n = a_actor3DRoot->GetObjectByName(data.nodeName);
				if (!n) {
					logger::error("Failed to find chain node {}", data.nodeName);
					return false;
				}

				if (i == 0) {
					if (n->parent != chainRoot) {
						logger::error("Chain root node {} is not the parent of the first chain node {}", a_chainRootName, data.nodeName);
						return false;
					}
				} else {
					if (n->parent != chainNodes.back()) {
						logger::error("Chain node {} is not the parent of the next chain node {}", a_chainNodeData[i - 1].nodeName, data.nodeName);
						return false;
					}
				}

				chainNodes.push_back(n);
				maxima.emplace_back(data.maxima);
				minima.emplace_back(data.minima);
				curTransforms.emplace_back(data.minima);
				curTargets.emplace_back(data.minima);
				freezeTargets.emplace_back(data.minima);
			}

			hasPhysics = a_physicsData.enabled && !a_noPhysics;

			if (hasPhysics) {
				double mass, stiffness, angularDamping, linearDrag;
				auto   parsed = parsePhysicsData(a_physicsData, mass, stiffness, angularDamping, linearDrag);
				chain = std::make_unique<PhysicsNodeChain>(mass, stiffness, angularDamping, linearDrag);
			} else {
				chain = std::make_unique<DirectNodeChain>();
			}



			chain->build(chainRoot, chainNodes);
			setNodeChainOverlayTransform(curTargets);
			this->isActive = true;

			lastTime = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
			return true;
		}

		// Update the node chain. Thread safe
		void update(time_t currentTime)
		{
			if (!isActive) {
				return;
			}

			std::lock_guard _lock(_this_lock);

			chain->update(lastTime, currentTime);

			if (eta < lastTime) {
				this->requestUpdate();
				this->lastTime = currentTime;
				return;
			}

			auto& target = isFreezed ? freezeTargets : curTargets;
			auto paramTarget = isFreezed ? physicsParamFreezeTarget : physicsParamTarget;

			if (currentTime >= eta) {
				setNodeChainOverlayTransform(target);
				physicsParams.t = paramTarget;
				updatePhysicsData();
			} else {
				double t = (double)(currentTime - lastTime) / (double)(eta - lastTime);
				interpolate_Impl(t, curTransforms, target, curTransforms);
				physicsParams.t = interpolate_float_Impl(t, physicsParams.t, paramTarget);
				setNodeChainOverlayTransform(curTransforms);
				updatePhysicsData();
			}

			this->requestUpdate();
			this->lastTime = currentTime;
			return;
		};

		// True if the state changed. Thread safe
		bool setActive(bool active)
		{
			if (active == isActive) {
				return false;
			}

			isActive = active;
			return true;
		}

		// Set the targets. Thread safe
		inline void setTargets(const std::vector<TransformTarget>& targets, time_t eta, float physicsParamTarget = -1.f)
		{
			std::lock_guard _lock(_this_lock);
			this->curTargets = targets;
			this->eta = eta;
			if (physicsParamTarget >= 0.f && physicsParamTarget <= 1.f) {
				this->physicsParamTarget = physicsParamTarget;
			}
		}

		// Set the interpolated targets. Thread safe
		inline void setInterpolatedTargets(float t, time_t eta)
		{
			// Clamp t
			t = std::clamp(t, 0.0f, 1.0f);

			std::lock_guard _lock(_this_lock);

			setInterpolatedTargets_Impl(t);

			this->eta = eta;
		}

		inline void setInterpolatedTargetsDelayed(float t, time_t delay)
		{
			// Clamp t
			t = std::clamp(t, 0.0f, 1.0f);

			std::lock_guard _lock(_this_lock);

			setInterpolatedTargets_Impl(t);

			time_t now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
			eta = now + delay;
		}

		inline void setInterpolatedTargetsPropotional(float t, float speed_multiplier = 1.f)
		{
			// Clamp t
			t = std::clamp(t, 0.0f, 1.0f);
			speed_multiplier = std::clamp(speed_multiplier, 0.01f, 10.f);

			std::lock_guard _lock(_this_lock);

			setInterpolatedTargets_Impl(t);

			float diff_t = std::abs(physicsParams.t - t);
			time_t now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
			eta = now + time_t(diff_t * float(fullErectionTimeMs) / speed_multiplier);
		}

		// Freeze at the current targets to 't'
		inline void freezeAt(float t, time_t transition_time = 0)
		{
			// Clamp t
			t = std::clamp(t, 0.0f, 1.0f);

			std::lock_guard _lock(_this_lock);

			freezeAt_Impl(t);

			time_t now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
			eta = now + transition_time;
		}

		// Freeze at the current targets to 't', with auto transition time
		inline void freezeAtAutoTransitionTime(float t, float speed_multiplier = 1.f)
		{
			// Clamp t
			t = std::clamp(t, 0.0f, 1.0f);
			speed_multiplier = std::clamp(speed_multiplier, 0.01f, 10.f);

			std::lock_guard _lock(_this_lock);

			freezeAt_Impl(t);

			float  diff_t = std::abs(physicsParams.t - physicsParamFreezeTarget);
			time_t now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
			eta = now + time_t(diff_t * float(fullErectionTimeMs) / speed_multiplier);
		}

		// Resume and proceed to original targets
		inline void unfreeze(time_t transition_time = 0)
		{
			if (!isFreezed) {
				return;
			}

			std::lock_guard _lock(_this_lock);

			unfreeze_Impl();

			eta = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count() + transition_time;
		}

		// Resume and proceed to original targets, with auto transition time
		inline void unfreezeAutoTransitionTime(float speed_multiplier = 1.f)
		{
			if (!isFreezed) {
				return;
			}

			speed_multiplier = std::clamp(speed_multiplier, 0.01f, 10.f);

			std::lock_guard _lock(_this_lock);

			unfreeze_Impl();
			
			float diff_t = std::abs(physicsParamTarget - physicsParamFreezeTarget);
			time_t now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
			eta = now + time_t(diff_t * float(fullErectionTimeMs) / speed_multiplier);
		}

		PhysicsNodeChain* getPhysicsChain()
		{
			if (!hasPhysics) {
				return nullptr;
			}
			return dynamic_cast<PhysicsNodeChain*>(chain.get());
		}

		void setPhysicsEnabled(bool enabled)
		{
			std::lock_guard _lock(_this_lock);
			auto physicsChain = getPhysicsChain();
			if (physicsChain) {
				if (enabled) {
					physicsChain->physics_enabled = true;
				} else {
					physicsChain->physics_enabled = false;
				}
				setNodeChainOverlayTransform(curTransforms);
			}
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

		inline bool parsePhysicsData(const PhysicsData& data, double& mass_init_eval, double& stiffness_init_eval, double& angularDamping_init_eval, double& linearDrag_init_eval)
		{
			std::lock_guard _lock(physicsParams._this_lock);

			physicsParams.t = 0;

			physicsParams.mass.Set(data.mass);

			bool parsed = true;

			physicsParams.stiffness.Set(data.stiffness);
			if (!data.stiffnessExpression.empty()) {
				if (!physicsParams.stiffness.Set(data.stiffnessExpression)) {
					logger::warn("Failed to parse stiffness expression: {}, error: {}. Using default value {}.", 
						data.stiffnessExpression, 
						physicsParams.stiffness.GetCompilerError(),
						data.stiffness
					);
					parsed = false;
				}
			}
			mass_init_eval = physicsParams.mass.Evaluate();

			physicsParams.angularDamping.Set(data.angularDamping);
			if (!data.angularDampingExpression.empty()) {
				if (!physicsParams.angularDamping.Set(data.angularDampingExpression)) {
					logger::warn("Failed to parse angular damping expression: {}, error: {}. Using default value {}.",
						data.angularDampingExpression,
						physicsParams.angularDamping.GetCompilerError(),
						data.angularDamping
					);
					parsed = false;
				}
			}
			angularDamping_init_eval = physicsParams.angularDamping.Evaluate();

			physicsParams.linearDrag.Set(data.linearDrag);
			if (!data.linearDragExpression.empty()) {
				if (!physicsParams.linearDrag.Set(data.linearDragExpression)) {
					logger::warn("Failed to parse linear drag expression: {}, error: {}. Using default value {}.",
						data.linearDragExpression,
						physicsParams.linearDrag.GetCompilerError(),
						data.linearDrag
					);
					parsed = false;
				}
			}
			linearDrag_init_eval = physicsParams.linearDrag.Evaluate();

			return parsed;
		}

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
				transforms.push_back(target.toNiTransform());
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