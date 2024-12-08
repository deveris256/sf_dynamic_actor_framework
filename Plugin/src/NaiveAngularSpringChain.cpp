#include "NaiveAngularSpringChain.h"
#include "LogWrapper.h"

void physics::AngularSpring::apply(double dt, const Eigen::Matrix4d& cur_parent, Eigen::Matrix4d& cur_joint_io)
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

void physics::AngularSpringChain::applyConstraints(double dt_sec)
{
	for (size_t i = 0; i < springs.size(); ++i) {
		springs[i].apply(dt_sec, joints[i], joints[i + 1]);
	}
}