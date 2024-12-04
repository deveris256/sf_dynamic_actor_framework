#include "NodeChainLerpGenerator.h"

bool daf::NodeChainLerpGenerator::build(RE::BGSFadeNode* a_actor3DRoot, const std::string& a_chainRootName, const std::vector<ChainNodeData>& a_chainNodeData, const PhysicsData& a_physicsData, bool a_noPhysics)
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

	auto numNodes = a_chainNodeData.size();
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
		mass = std::max(0.01, mass);
		stiffness = std::max(0.1, stiffness);
		angularDamping = std::max(0.1, angularDamping);
		linearDrag = std::max(0.0, linearDrag);
		chain = std::make_unique<PhysicsNodeChain>(mass, stiffness, angularDamping, linearDrag);
	} else {
		chain = std::make_unique<DirectNodeChain>();
	}

	chain->build(chainRoot, chainNodes);
	setNodeChainOverlayTransform(curTargets);
	this->isActive = true;

	lastLocalTime = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
	lastSystemTime = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
	return true;
}

void daf::NodeChainLerpGenerator::updateWithSystemTime(time_t systemTime, time_t delta_time_clamp)
{
	time_t deltaTime = systemTime - lastSystemTime;

	if (delta_time_clamp != 0 && deltaTime > delta_time_clamp) {
		deltaTime = delta_time_clamp;
	}

	updateWithDeltaTime(deltaTime);

	this->lastSystemTime = systemTime;
}

void daf::NodeChainLerpGenerator::updateWithDeltaTime(time_t deltaTime)
{
	if (!isActive) {
		return;
	}

	std::lock_guard _lock(_this_lock);

	time_t currentLocalTime = lastLocalTime + deltaTime;

	chain->update(lastLocalTime, currentLocalTime);

	if (etaLocal < lastLocalTime) {
		this->requestUpdate();
		this->lastLocalTime = currentLocalTime;
		return;
	}

	auto& target = isFreezed ? freezeTargets : curTargets;
	auto  paramTarget = isFreezed ? physicsParamFreezeTarget : physicsParamTarget;

	if (currentLocalTime >= etaLocal) {
		setNodeChainOverlayTransform(target);
		physicsParams.t = paramTarget;
		updatePhysicsData();
	} else {
		double t = (double)(deltaTime) / (double)(etaLocal - lastLocalTime);
		interpolate_Impl(t, curTransforms, target, curTransforms);
		physicsParams.t = interpolate_float_Impl(t, physicsParams.t, paramTarget);
		setNodeChainOverlayTransform(curTransforms);
		updatePhysicsData();
	}

	this->requestUpdate();
	this->lastLocalTime = currentLocalTime;
	return;
}

bool daf::NodeChainLerpGenerator::parsePhysicsData(const PhysicsData& data, double& mass_init_eval, double& stiffness_init_eval, double& angularDamping_init_eval, double& linearDrag_init_eval)
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
				data.stiffness);
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
				data.angularDamping);
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
				data.linearDrag);
			parsed = false;
		}
	}
	linearDrag_init_eval = physicsParams.linearDrag.Evaluate();

	return parsed;
}

void daf::PhysicsNodeChain::build(const RE::NiAVObject* a_chainRoot, const std::vector<RE::NiAVObject*>& a_chainNodes)
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

void daf::PhysicsNodeChain::update(time_t lastTime, time_t currentTime)
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
}

void daf::PhysicsNodeChain::setOverlayTransform(const std::vector<RE::NiTransform>& transform_overlay)
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
