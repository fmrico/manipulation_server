#ifndef MANIPULATION__MOVETOACTIONSERVER_HPP
#define MANIPULATION__MOVETOACTIONSERVER_HPP

#include <rclcpp/rclcpp.hpp>
#include <thread>
#include "rclcpp_lifecycle/lifecycle_node.hpp"
#include "lifecycle_msgs/msg/state.hpp"
#include "rclcpp_action/rclcpp_action.hpp"
#include "manipulation_interfaces/action/move_to_predefined.hpp"
#include <moveit/planning_scene/planning_scene.h>
#include <moveit/planning_scene_interface/planning_scene_interface.h>
#include <moveit/task_constructor/task.h>
#include <moveit/task_constructor/solvers.h>
#include <moveit/task_constructor/stages.h>

#include "manipulation/MTCNode.hpp"


namespace manipulation
{

class ManipulationServer : public rclcpp_lifecycle::LifecycleNode
{
public:
  using MoveToPredefined = manipulation_interfaces::action::MoveToPredefined;
  using GoalHandleMove = rclcpp_action::ServerGoalHandle<MoveToPredefined>;
  
  ManipulationServer(
      const rclcpp::NodeOptions & options = rclcpp::NodeOptions()
    );
  virtual ~ManipulationServer();

  rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn
  on_configure(const rclcpp_lifecycle::State & state) override;
  rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn
  on_activate(const rclcpp_lifecycle::State & state) override;
  rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn
  on_deactivate(const rclcpp_lifecycle::State & state) override;
  rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn
  on_cleanup(const rclcpp_lifecycle::State & state) override;
  rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn
  on_shutdown(const rclcpp_lifecycle::State & state) override;


private:

  rclcpp_action::Server<MoveToPredefined>::SharedPtr action_server_;

  rclcpp_action::GoalResponse handle_move_to_predefined_goal(
      const rclcpp_action::GoalUUID & uuid,
      std::shared_ptr<const MoveToPredefined::Goal> goal);

  rclcpp_action::CancelResponse handle_move_to_predefined_cancel(
      const std::shared_ptr<rclcpp_action::ServerGoalHandle<MoveToPredefined>> goal_handle);

  void handle_move_to_predefined_accepted(
      const std::shared_ptr<GoalHandleMove> goal_handle);

  void execute_move_to_predefined(const std::shared_ptr<GoalHandleMove> goal_handle);

  

  std::string group_, goal_;

  std::string arm_torso_group_name_{"arm_torso"};
  std::string hand_group_name_{"gripper"};

  std::string hand_frame_{"gripper_grasping_frame"};

  moveit::task_constructor::Task task_;
  std::unique_ptr<moveit::task_constructor::stages::CurrentState> stage_state_current_;
  std::shared_ptr<moveit::task_constructor::solvers::JointInterpolationPlanner> interpolation_planner_;
  moveit::planning_interface::PlanningSceneInterface planning_interface_;
  moveit::task_constructor::Stage* current_state_ptr_{nullptr};

  rclcpp::Node::SharedPtr node_;
  rclcpp::executors::MultiThreadedExecutor executor_;
  std::unique_ptr<std::thread> node_thread_;
  std::unique_ptr<std::thread> task_thread_;


};

} // end namespace manipulation

#endif // MANIPULATION__MOVETOACTIONSERVER_HPP
