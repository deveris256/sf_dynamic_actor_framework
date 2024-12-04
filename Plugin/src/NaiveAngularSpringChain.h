#pragma once

namespace physics
{
	class AngularSpring
	{
	public:
		AngularSpring(const Eigen::Matrix4d& joint, double a_mass, const Eigen::Matrix4d& parent,
			double a_stiffness, double a_damping, double a_drag, const Eigen::Vector3d& a_gravity = Eigen::Vector3d(0, 0, -9.81)) :
			joint_mass(a_mass),
			damping(a_damping), drag(a_drag), gravity(a_gravity)
		{
			rest_transform = parent.inverse() * joint;
			angular_velocity.setZero();
			prev_joint_rot = joint.block<3, 3>(0, 0);
			cur_parent_rot = parent.block<3, 3>(0, 0);
			prev_parent_rot = parent.block<3, 3>(0, 0);
			setStiffness(a_stiffness);
		}

		// Takes global transforms but calculation happens in local space
		void apply(double dt, const Eigen::Matrix4d& cur_parent, Eigen::Matrix4d& cur_joint_io)
		{
			Eigen::Matrix3d& cur_joint_rot = prev_joint_rot;
			cur_parent_rot = cur_parent.block<3, 3>(0, 0);
			const Eigen::Matrix3d& rest_rot = rest_transform.block<3, 3>(0, 0);

			// Compute rotation deviation from the rest position
			Eigen::Matrix3d delta_rot = rest_rot.transpose() * cur_parent_rot.transpose() * cur_joint_rot;

			Eigen::AngleAxisd      delta_angle_aa(delta_rot);
			const Eigen::Vector3d& axis = delta_angle_aa.axis();
			double                 angle = delta_angle_aa.angle();

			if (parent_blending_factor > 0.0) {
				Eigen::AngleAxisd delta_parent_rot(angle * parent_blending_factor, -axis);
				angle *= (1 - parent_blending_factor);
				cur_joint_rot.noalias() = cur_joint_rot * delta_parent_rot;
			}

			// Limit the rotation within the maximum cone angle
			if (angle > max_rotation_angle) {
				angle = max_rotation_angle;  // Clamp angle
				Eigen::AngleAxisd rel_rot(angle, axis);
				prev_joint_rot.noalias() = cur_parent_rot * rest_rot * rel_rot;  // Update joint rotation

				// Reduce angular velocity along the rotation axis
				angular_velocity = prev_joint_rot * angular_velocity;
			} else if (angle < 1E-4) {
				angle = 0.0;
				angular_velocity.setZero();
			}

			linear_velocity = (cur_joint_io.block<3, 1>(0, 3) - prev_joint_pos) / dt;

			auto drag_force = -drag * linear_velocity;

			// Compute torques
			Eigen::Vector3d spring_torque = -stiffness * angle * axis;
			Eigen::Vector3d damping_torque = -damping * angular_velocity;
			Eigen::Vector3d gravity_torque = cur_joint_rot.transpose() * ((cur_joint_io.block<3, 1>(0, 3) - cur_parent.block<3, 1>(0, 3)).cross(drag_force + gravity * joint_mass));

			spring_torque(0) *= twist_multipier;
			damping_torque(0) *= twist_multipier;

			Eigen::Vector3d total_torque = spring_torque + damping_torque + gravity_torque;

			// Update angular velocity
			angular_velocity += total_torque / joint_mass * dt;
			angular_velocity *= global_velocity_damping_factor;  // Apply damping

			double angular_magnitude = angular_velocity.norm();
			if (angular_magnitude > angular_speed_limit) {
				angular_velocity = angular_velocity.normalized() * angular_speed_limit;
				angular_magnitude = angular_speed_limit;
			} else if (angular_magnitude < 1E-4) {
				angular_velocity.setZero();
			}

			// Integrate angular velocity
			Eigen::AngleAxisd rotation_update(angular_magnitude * dt, angular_velocity.normalized());

			prev_joint_rot.noalias() = prev_joint_rot * rotation_update;
			prev_joint_pos = cur_joint_io.block<3, 1>(0, 3);
			prev_parent_rot = cur_parent_rot;

			// Compute the next joint position
			Eigen::Matrix4d joint = Eigen::Matrix4d::Identity();
			joint.block<3, 3>(0, 0) = prev_joint_rot * rest_rot.transpose();
			joint.block<3, 1>(0, 3) = cur_parent.block<3, 1>(0, 3);
			cur_joint_io = joint * rest_transform;
		}

		void normalizeRotation()
		{
			Eigen::AngleAxisd aa(prev_joint_rot);
			prev_joint_rot = aa.toRotationMatrix();
		}

		void setStiffness(double a_stiffness)
		{
			this->stiffness = std::max(0.01, a_stiffness);
			parent_blending_factor = this->stiffness / (this->stiffness + 3000.0 * joint_mass);
		}

		double          joint_mass;
		double          stiffness;
		double          damping;
		double			drag{ 0.0 };
		double		    parent_blending_factor{ 0.0 };
		double          global_velocity_damping_factor{ 0.98 };  // simulates energy loss
		Eigen::Vector3d gravity;

		Eigen::Matrix4d rest_transform;
		
		Eigen::Vector3d angular_velocity;
		Eigen::Vector3d linear_velocity;

		Eigen::Matrix3d prev_joint_rot;
		Eigen::Vector3d prev_joint_pos;
		Eigen::Matrix3d prev_parent_rot;
		Eigen::Matrix3d cur_parent_rot;

		double twist_multipier{ 2.0 };
		double angular_speed_limit{ 30.0 };  // radians per second}
		double max_rotation_angle{ 0.6 };  // radians
	};

	class AngularSpringChain
	{
	public:
		AngularSpringChain() {}

		void clear()
		{
			joints.clear();
			springs.clear();
		}

		void build(const Eigen::Matrix4d& init_root, const std::vector<Eigen::Matrix4d>& init_joints, double mass, double stiffness, double damping, double drag, Eigen::Vector3d gravity)
		{
			clear();

			joints.reserve(init_joints.size() + 1);
			springs.reserve(init_joints.size());
			num_joints = init_joints.size() + 1;

			this->gravity = gravity;

			joints.emplace_back(init_root);
			for (size_t i = 1; i < num_joints; ++i) {
				joints.emplace_back(init_joints[i - 1]);
				springs.emplace_back(AngularSpring(joints[i], mass * double(num_joints - i), joints[i - 1], stiffness, damping, drag, gravity));
			}
		}

		void setStiffness(double stiffness)
		{
			for (auto& spring : springs) {
				spring.setStiffness(stiffness);
			}
		}

		void setAngularDamping(double damping)
		{
			for (auto& spring : springs) {
				spring.damping = damping;
			}
		}

		void setLinearDrag(double drag)
		{
			for (auto& spring : springs) {
				spring.drag = drag;
			}
		}

		void setGravity(const Eigen::Vector3d& gravity)
		{
			this->gravity = gravity;
			for (auto& spring : springs) {
				spring.gravity = gravity;
			}
		}

		bool forEachSpring(const std::function<bool(size_t, AngularSpring&)>& func)
		{
			size_t i = 0;
			for (auto& spring : springs) {
				if (!func(i, spring)) {
					return false;
				}
				++i;
			}
			return true;
		}

		bool forEachJoint(const std::function<bool(size_t, Eigen::Matrix4d&)>& func)
		{
			size_t i = 0;
			for (auto& joint : joints) {
				if (!func(i, joint)) {
					return false;
				}
				++i;
			}
			return true;
		}

		// Apply each spring constraint in sequence with Markov property.
		void applyConstraints(double dt_sec)
		{
			for (size_t i = 0; i < springs.size(); ++i) {
				springs[i].apply(dt_sec, joints[i], joints[i + 1]);
			}
		}

		bool reachedStasis() const
		{
			// Check if all springs have reached stasis
			for (const auto& spring : springs) {
				if (spring.angular_velocity.norm() > 1e-6) {
					return false;
				}
			}
			return true;
		}

		std::vector<Eigen::Vector3d> getJointPositions() const
		{
			// Extract and return the positions of all joints
			std::vector<Eigen::Vector3d> positions;
			positions.reserve(joints.size());
			for (const auto& joint : joints) {
				positions.push_back(joint.block<3, 1>(0, 3));
			}
			return positions;
		}

		std::vector<Eigen::Matrix3d> getJointAxes() const
		{
			// Extract and return the local axes of all joints
			std::vector<Eigen::Matrix3d> axes;
			axes.reserve(joints.size());
			for (const auto& joint : joints) {
				axes.push_back(joint.block<3, 3>(0, 0));
			}
			return axes;
		}

		Eigen::Matrix4d& getRootJoint()
		{
			return joints[0];
		}

		Eigen::Matrix3d& getSpringJointRotation(size_t i)
		{
			return springs[i].prev_joint_rot;
		}

		void normalizeSpringJointRotationAll()
		{
			for (auto& spring : springs) {
				spring.normalizeRotation();
			}
		}

		Eigen::Vector3d              gravity;
		std::vector<Eigen::Matrix4d> joints;
		std::vector<AngularSpring>   springs;
	private:
		size_t                       num_joints{ 0 };
	};
}