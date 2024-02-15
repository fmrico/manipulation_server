#include "manipulation/ManipulationServer.hpp"
#include <algorithm>
#include <vector>

namespace manipulation
{

ManipulationServer::ManipulationServer(const rclcpp::NodeOptions & options)
: rclcpp_lifecycle::LifecycleNode("manipulation_action_server", "", options),
  node_{std::make_shared<rclcpp::Node>("manipulation_action_server_executor", options)}
{

  RCLCPP_INFO(this->get_logger(), "Manipulation server created");

}
ManipulationServer::~ManipulationServer()
{
}

rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn
ManipulationServer::on_configure(const rclcpp_lifecycle::State & state)
{
  // this is for GETTING parameters (todo)
  RCLCPP_INFO(get_logger(), "Configuring manipulation server");
  action_server_ = rclcpp_action::create_server<MoveToPredefined>(
  this,
  "move_robot_to_predefined",
  std::bind(&ManipulationServer::handle_move_to_predefined_goal, this, std::placeholders::_1, std::placeholders::_2),
  std::bind(&ManipulationServer::handle_move_to_predefined_cancel, this, std::placeholders::_1),
  std::bind(&ManipulationServer::handle_move_to_predefined_accepted, this, std::placeholders::_1)
  );
  interpolation_planner_ = std::make_shared<moveit::task_constructor::solvers::JointInterpolationPlanner>();
  // change the name to be a parameter
  task_.stages()->setName("demo task");
  task_.loadRobotModel(node_);
  task_.setProperty("ik_frame", "gripper_grasping_frame");

  node_thread_ = std::make_unique<std::thread>([this]() {
    executor_.add_node(node_);
    executor_.spin();
    executor_.remove_node(node_);
  });

  stage_state_current_ = std::make_unique<moveit::task_constructor::stages::CurrentState>("current");
  current_state_ptr_ = stage_state_current_.get();

  
  return rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn::SUCCESS;
}

rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn
ManipulationServer::on_activate(const rclcpp_lifecycle::State & state)
{
  RCLCPP_INFO(get_logger(), "Activating manipulation server");
  return rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn::SUCCESS;
}

rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn
ManipulationServer::on_deactivate(const rclcpp_lifecycle::State & state)
{
  RCLCPP_INFO(get_logger(), "Deactivating manipulation server");
  return rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn::SUCCESS;
}

rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn
ManipulationServer::on_cleanup(const rclcpp_lifecycle::State & state)
{
  RCLCPP_INFO(get_logger(), "Cleanning up manipulation server");
  return rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn::SUCCESS;
}

rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn
ManipulationServer::on_shutdown(const rclcpp_lifecycle::State & state)
{
  node_thread_->join();
  RCLCPP_INFO(get_logger(), "Shutting down manipulation server");
  return rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn::SUCCESS;
}

rclcpp_action::GoalResponse
ManipulationServer::handle_move_to_predefined_goal(
    const rclcpp_action::GoalUUID & uuid,
    std::shared_ptr<const manipulation_interfaces::action::MoveToPredefined::Goal> goal)
{
  
  RCLCPP_INFO(this->get_logger(), "Received goal to move robot predefined pose %s", goal_.c_str());
  (void)uuid;
  return rclcpp_action::GoalResponse::ACCEPT_AND_EXECUTE;
}

rclcpp_action::CancelResponse
ManipulationServer::handle_move_to_predefined_cancel(
    const std::shared_ptr<rclcpp_action::ServerGoalHandle<manipulation_interfaces::action::MoveToPredefined>> goal_handle)
{
  RCLCPP_INFO(this->get_logger(), "Goal cancelled");
  (void)goal_handle;
  return rclcpp_action::CancelResponse::ACCEPT;
}

void
ManipulationServer::handle_move_to_predefined_accepted(
    const std::shared_ptr<GoalHandleMove> goal_handle)
{
  RCLCPP_INFO(this->get_logger(), "Goal accepted");
  
  std::thread{std::bind(&ManipulationServer::execute_move_to_predefined, this, std::placeholders::_1),
    goal_handle}.detach();

}

void
ManipulationServer::execute_move_to_predefined(const std::shared_ptr<GoalHandleMove> goal_handle)
{
  RCLCPP_INFO(this->get_logger(), "Executing goal");
  // auto feedback = std::make_shared<MoveTo::Feedback>();
  auto result = std::make_shared<MoveToPredefined::Result>();

  result->success = false;

  auto goal = goal_handle->get_goal();

  task_.add(std::move(stage_state_current_));

  auto stage_predefined_position =
      std::make_unique<moveit::task_constructor::stages::MoveTo>("stage_predefined_position", interpolation_planner_);
  stage_predefined_position->setGroup(goal->group_name);
  stage_predefined_position->setGoal(goal->group_name);
  stage_predefined_position->setTimeout(8.0);
  task_.add(std::move(stage_predefined_position));
  RCLCPP_INFO(this->get_logger(), "Before init goal");
  try
  {
    task_.init();
  }
  catch (moveit::task_constructor::InitStageException& e)
  {
    RCLCPP_ERROR_STREAM(get_logger(), e);
    result->success = false;
  }
   if (!task_.plan(5))
  {
    RCLCPP_ERROR_STREAM(get_logger(), "Task planning failed");
    return;
  }
  RCLCPP_INFO_STREAM(get_logger(), "Task planning succeeded, sending plan to execute");
   
  task_.introspection().publishSolution(*task_.solutions().front());

  auto solutions = task_.solutions();
  std::vector<decltype(solutions)::value_type> solutionsVector(solutions.begin(), solutions.end());
  std::sort(solutionsVector.begin(), solutionsVector.end(), std::greater<>());
  if (!solutionsVector.empty()) {
    auto plan_result = task_.execute(*solutionsVector[0]);
    if (plan_result.val != moveit_msgs::msg::MoveItErrorCodes::SUCCESS)
    {
      result->success = false;
      RCLCPP_ERROR_STREAM(get_logger(), "Task execution failed");      
    }
    RCLCPP_INFO_STREAM(get_logger(), "Task execution succeeded");
    result->success = true;
    return;
    }
  else {
    RCLCPP_ERROR_STREAM(get_logger(), "No solutions found");
    result->success = false;
  }
  // auto mtc_node = std::make_shared<MTCNode>(options, group_, goal_);
  // RCLCPP_INFO(this->get_logger(), "MTCNode node created with (%s-%s)",
  //   group_.c_str(), goal_.c_str());
  // rclcpp::executors::MultiThreadedExecutor executor;
  
  // auto spin_thread = std::make_unique<std::thread>([&executor, &mtc_node]() {
  //   executor.add_node(mtc_node->get_node_base_interface());
  //   executor.spin();
  //   executor.remove_node(mtc_node->get_node_base_interface());
  // });


  // RCLCPP_INFO(this->get_logger(), "MTCNode added to executor and spinning...");
  // mtc_node->do_task();

  // spin_thread->join();
  
  
  RCLCPP_INFO(this->get_logger(), "Goal succeeded");
  goal_handle->succeed(result);
}

} // namespace manipulation

#include "rclcpp_components/register_node_macro.hpp"
RCLCPP_COMPONENTS_REGISTER_NODE(manipulation::ManipulationServer)
