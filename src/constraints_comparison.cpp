#include <trajopt_utils/macros.h>
TRAJOPT_IGNORE_WARNINGS_PUSH
#include <ctime>
#include <ros/package.h>
#include <ros/ros.h>
TRAJOPT_IGNORE_WARNINGS_POP

#include <trajopt/trajectory_costs.hpp>
#include <trajopt/typedefs.hpp>  // should only need this one

#include <trajopt_sco/bpmpd_interface.hpp>
#include <Eigen/Dense>
#include <Eigen/Core>

// No idea which of these I need
#include <trajopt_utils/macros.h>
TRAJOPT_IGNORE_WARNINGS_PUSH
#include <jsoncpp/json/json.h>
#include <ros/ros.h>
#include <srdfdom/model.h>
#include <urdf_parser/urdf_parser.h>
#include <octomap_ros/conversions.h>
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>
#include <pcl_conversions/pcl_conversions.h>
#include <sensor_msgs/PointCloud2.h>
#include <sensor_msgs/point_cloud_conversion.h>
TRAJOPT_IGNORE_WARNINGS_POP

#include <tesseract_ros/kdl/kdl_chain_kin.h>
#include <tesseract_ros/kdl/kdl_env.h>
#include <tesseract_ros/ros_basic_plotting.h>
#include <trajopt/plot_callback.hpp>
#include <trajopt/problem_description.hpp>
#include <trajopt_utils/config.hpp>
#include <trajopt_utils/logging.hpp>

#include <trajopt/collision_terms.hpp>
#include <trajopt/common.hpp>
#include <trajopt/kinematic_terms.hpp>
#include <trajopt/plot_callback.hpp>
#include <trajopt/problem_description.hpp>
#include <trajopt/trajectory_costs.hpp>
#include <trajopt_sco/expr_op_overloads.hpp>
#include <trajopt_sco/expr_ops.hpp>
#include <trajopt_utils/eigen_conversions.hpp>
#include <trajopt_utils/eigen_slicing.hpp>
#include <trajopt_utils/logging.hpp>
#include <trajopt_utils/vector_ops.hpp>

#include <tesseract_core/basic_env.h>
#include <tesseract_core/basic_kin.h>
#include <trajopt/common.hpp>
#include <trajopt/json_marshal.hpp>
#include <trajopt_sco/optimizers.hpp>

using namespace trajopt;

class CostsComparisonUtils
{
public:
  EIGEN_MAKE_ALIGNED_OPERATOR_NEW

  ros::NodeHandle nh_;

  CostsComparisonUtils(int rows = 10, int cols = 7)
  {
    // Create all the inputs upfront
    num_rows = rows;
    num_cols = cols;
    coeffs = Eigen::VectorXd::Random(num_cols);
    targets = Eigen::VectorXd::Random(num_cols);
    upper_limits = Eigen::VectorXd::Random(num_cols);  // Random returns [-1:1]
    lower_limits = Eigen::VectorXd::Random(num_cols);
    first_step = 0;
    last_step = (num_rows)-1;
    dblvec = createRandomDblVec(num_rows * num_cols);
    model = std::make_shared<sco::BPMPDModel>();
    traj = createVarArray(model, num_rows, num_cols);
  }

  /**
   * @brief Creates a DblVec full of random numbers
   * @param size Length of the vector to create
   * @return
   */
  DblVec createRandomDblVec(const int size)
  {
    DblVec output;
    for (int ind; ind < size; ind++)
    {
      output.push_back(static_cast<double>(rand() / RAND_MAX));
    }
    return output;
  }

  /**
   * @brief Creates a VarArray based on the model passed in
   * @param model - Model that will store the varReps for the vars in the VarArray
   * @param num_rows - Number of rows in the VarArray
   * @param num_cols - Number of columns in the VarArray
   * @return
   */
  trajopt::VarArray createVarArray(std::shared_ptr<sco::BPMPDModel> model, const int& num_rows, const int& num_cols)
  {
    trajopt::VarArray output;

    for (int ind = 0; ind < num_rows; ind++)
    {
      for (int ind2 = 0; ind2 < num_cols; ind2++)
      {
        std::string name = std::to_string(ind) + "_" + std::to_string(ind2);
        model->addVar(name);

        // Copy in VarVector
        output.m_data = model->getVars();
        // Set rows and columns
        output.m_nRow = num_rows;
        output.m_nCol = num_cols;
      }
    }
    return output;
  }

  // Create all the inputs upfront
  int num_rows;
  int num_cols;
  Eigen::VectorXd coeffs;
  Eigen::VectorXd targets;
  Eigen::VectorXd upper_limits;
  Eigen::VectorXd lower_limits;
  int first_step;
  int last_step;
  trajopt::DblVec dblvec;
  std::shared_ptr<sco::BPMPDModel> model;
  trajopt::VarArray traj;
};

//////

int main(int argc, char** argv)
{
  int min_row = 6;
  int max_row = 50;

  ROS_ERROR("Press enter to continue");
  std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

  ros::init(argc, argv, "trajopt_cnts_comparison_unit");
  ros::NodeHandle pnh("~");

  //---------------------------------------------
  //----------- Initialize ----------------------
  //---------------------------------------------
  std::ofstream ofs;
  std::string path = ros::package::getPath("trajopt_examples") + "/cnts_comparison_output.csv";
  ofs.open(path, std::ofstream::out | std::ofstream::trunc);
  ofs << "Cost/Cnt,"
      << "Construction Time,"
      << "Value Time,"
      << "Convex Time,"
      << "Violations Time,"
      << "Violation Time,"
      << "num_rows,"
      << "num_cols"
      << "\n";

  //---------------------------------------------
  //----------- JointPosEqCnt  -----------------
  //---------------------------------------------
  for (int ind = min_row; ind < max_row; ind++)
  {
    // Create cost comparison utility that contains all of the parameters
    CostsComparisonUtils util(ind, 7);

    // Define outputs
    double construct_time, value_time, convex_time, violation_time, violations_time;
    ros::Time tStart;

    // Time construction
    tStart = ros::Time::now();
    JointPosEqConstraint object(util.traj, util.coeffs, util.targets, util.first_step, util.last_step);
    construct_time = (ros::Time::now() - tStart).toSec();

    // Time the evaluation
    tStart = ros::Time::now();
    object.value(util.dblvec);
    value_time = (ros::Time::now() - tStart).toSec();

    // Time convex fnc
    tStart = ros::Time::now();
    object.convex(util.dblvec, util.model.get());
    convex_time = (ros::Time::now() - tStart).toSec();

    // Time violations
    tStart = ros::Time::now();
    object.violations(util.dblvec);
    violations_time = (ros::Time::now() - tStart).toSec();

    // Time violation
    tStart = ros::Time::now();
    object.violation(util.dblvec);
    violation_time = (ros::Time::now() - tStart).toSec();

    // Display to terminal and print to file
    std::string cost_name = "JointPosEqCnt";
    std::cout << cost_name << "   construct_time: " << construct_time << "    value_time: " << value_time
              << "    convex time: " << convex_time << "\n";
    ofs << cost_name << "," << construct_time << "," << value_time << "," << convex_time << "," << violations_time
        << "," << violation_time << "," << util.num_rows << "," << util.num_cols << "\n"
        << std::flush;
  }
  //---------------------------------------------
  //----------- JointVelEqCnt ------------------
  //---------------------------------------------
  for (int ind = min_row; ind < max_row; ind++)
  {
    // Create cost comparison utility that contains all of the parameters
    CostsComparisonUtils util(ind, 7);

    // Define outputs
    double construct_time, value_time, convex_time, violation_time, violations_time;
    ros::Time tStart;

    // Time construction
    tStart = ros::Time::now();
    JointVelEqConstraint object(util.traj, util.coeffs, util.targets, util.first_step, util.last_step);
    construct_time = (ros::Time::now() - tStart).toSec();

    // Time the evaluation
    tStart = ros::Time::now();
    object.value(util.dblvec);
    value_time = (ros::Time::now() - tStart).toSec();

    // Time convex fnc
    tStart = ros::Time::now();
    object.convex(util.dblvec, util.model.get());
    convex_time = (ros::Time::now() - tStart).toSec();

    // Time violations
    tStart = ros::Time::now();
    object.violations(util.dblvec);
    violations_time = (ros::Time::now() - tStart).toSec();

    // Time violation
    tStart = ros::Time::now();
    object.violation(util.dblvec);
    violation_time = (ros::Time::now() - tStart).toSec();

    // Display to terminal and print to file
    std::string cost_name = "JointVelEqCnt";
    std::cout << cost_name << "   construct_time: " << construct_time << "    value_time: " << value_time
              << "    convex time: " << convex_time << "\n";
    ofs << cost_name << "," << construct_time << "," << value_time << "," << convex_time << "," << violations_time
        << "," << violation_time << "," << util.num_rows << "," << util.num_cols << "\n"
        << std::flush;
  }
  //---------------------------------------------
  //-----------  JointAccEqCnt  ----------------
  //---------------------------------------------
  for (int ind = min_row; ind < max_row; ind++)
  {
    // Create cost comparison utility that contains all of the parameters
    CostsComparisonUtils util(ind, 7);

    // Define outputs
    double construct_time, value_time, convex_time, violation_time, violations_time;
    ros::Time tStart;

    // Time construction
    tStart = ros::Time::now();
    JointAccEqConstraint object(util.traj, util.coeffs, util.targets, util.first_step, util.last_step);
    construct_time = (ros::Time::now() - tStart).toSec();

    // Time the evaluation
    tStart = ros::Time::now();
    object.value(util.dblvec);
    value_time = (ros::Time::now() - tStart).toSec();

    // Time convex fnc
    tStart = ros::Time::now();
    object.convex(util.dblvec, util.model.get());
    convex_time = (ros::Time::now() - tStart).toSec();

    // Time violations
    tStart = ros::Time::now();
    object.violations(util.dblvec);
    violations_time = (ros::Time::now() - tStart).toSec();

    // Time violation
    tStart = ros::Time::now();
    object.violation(util.dblvec);
    violation_time = (ros::Time::now() - tStart).toSec();

    // Display to terminal and print to file
    std::string cost_name = "JointAccEqCnt";
    std::cout << cost_name << "   construct_time: " << construct_time << "    value_time: " << value_time
              << "    convex time: " << convex_time << "\n";
    ofs << cost_name << "," << construct_time << "," << value_time << "," << convex_time << "," << violations_time
        << "," << violation_time << "," << util.num_rows << "," << util.num_cols << "\n"
        << std::flush;
  }
  //---------------------------------------------
  //-----------  JointJerkEqCnt ----------------
  //---------------------------------------------
  for (int ind = min_row; ind < max_row; ind++)
  {
    // Create cost comparison utility that contains all of the parameters
    CostsComparisonUtils util(ind, 7);

    // Define outputs
    double construct_time, value_time, convex_time, violation_time, violations_time;
    ros::Time tStart;

    // Time construction
    tStart = ros::Time::now();
    JointJerkEqConstraint object(util.traj, util.coeffs, util.targets, util.first_step, util.last_step);
    construct_time = (ros::Time::now() - tStart).toSec();

    // Time the evaluation
    tStart = ros::Time::now();
    object.value(util.dblvec);
    value_time = (ros::Time::now() - tStart).toSec();

    // Time convex fnc
    tStart = ros::Time::now();
    object.convex(util.dblvec, util.model.get());
    convex_time = (ros::Time::now() - tStart).toSec();

    // Time violations
    tStart = ros::Time::now();
    object.violations(util.dblvec);
    violations_time = (ros::Time::now() - tStart).toSec();

    // Time violation
    tStart = ros::Time::now();
    object.violation(util.dblvec);
    violation_time = (ros::Time::now() - tStart).toSec();

    // Display to terminal and print to file
    std::string cost_name = "JointJerkEqCnt";
    std::cout << cost_name << "   construct_time: " << construct_time << "    value_time: " << value_time
              << "    convex time: " << convex_time << "\n";
    ofs << cost_name << "," << construct_time << "," << value_time << "," << convex_time << "," << violations_time
        << "," << violation_time << "," << util.num_rows << "," << util.num_cols << "\n"
        << std::flush;
  }

  //---------------------------------------------
  //----------- JointPosIneqCnt  -----------------
  //---------------------------------------------
  for (int ind = min_row; ind < max_row; ind++)
  {
    // Create cost comparison utility that contains all of the parameters
    CostsComparisonUtils util(ind, 7);

    // Define outputs
    double construct_time, value_time, convex_time, violation_time, violations_time;
    ros::Time tStart;

    // Time construction
    tStart = ros::Time::now();
    JointPosIneqConstraint object(
        util.traj, util.coeffs, util.targets, util.upper_limits, util.lower_limits, util.first_step, util.last_step);
    construct_time = (ros::Time::now() - tStart).toSec();

    // Time the evaluation
    tStart = ros::Time::now();
    object.value(util.dblvec);
    value_time = (ros::Time::now() - tStart).toSec();

    // Time convex fnc
    tStart = ros::Time::now();
    object.convex(util.dblvec, util.model.get());
    convex_time = (ros::Time::now() - tStart).toSec();

    // Time violations
    tStart = ros::Time::now();
    object.violations(util.dblvec);
    violations_time = (ros::Time::now() - tStart).toSec();

    // Time violation
    tStart = ros::Time::now();
    object.violation(util.dblvec);
    violation_time = (ros::Time::now() - tStart).toSec();

    // Display to terminal and print to file
    std::string cost_name = "JointPosIneqCnt";
    std::cout << cost_name << "   construct_time: " << construct_time << "    value_time: " << value_time
              << "    convex time: " << convex_time << "\n";
    ofs << cost_name << "," << construct_time << "," << value_time << "," << convex_time << "," << violations_time
        << "," << violation_time << "," << util.num_rows << "," << util.num_cols << "\n"
        << std::flush;
  }
  //---------------------------------------------
  //----------- JointVelIneqCnt  -----------------
  //---------------------------------------------
  for (int ind = min_row; ind < max_row; ind++)
  {
    // Create cost comparison utility that contains all of the parameters
    CostsComparisonUtils util(ind, 7);

    // Define outputs
    double construct_time, value_time, convex_time, violation_time, violations_time;
    ros::Time tStart;

    // Time construction
    tStart = ros::Time::now();
    JointVelIneqConstraint object(
        util.traj, util.coeffs, util.targets, util.upper_limits, util.lower_limits, util.first_step, util.last_step);
    construct_time = (ros::Time::now() - tStart).toSec();

    // Time the evaluation
    tStart = ros::Time::now();
    object.value(util.dblvec);
    value_time = (ros::Time::now() - tStart).toSec();

    // Time convex fnc
    tStart = ros::Time::now();
    object.convex(util.dblvec, util.model.get());
    convex_time = (ros::Time::now() - tStart).toSec();

    // Time violations
    tStart = ros::Time::now();
    object.violations(util.dblvec);
    violations_time = (ros::Time::now() - tStart).toSec();

    // Time violation
    tStart = ros::Time::now();
    object.violation(util.dblvec);
    violation_time = (ros::Time::now() - tStart).toSec();

    // Display to terminal and print to file
    std::string cost_name = "JointVelIneqCnt";
    std::cout << cost_name << "   construct_time: " << construct_time << "    value_time: " << value_time
              << "    convex time: " << convex_time << "\n";
    ofs << cost_name << "," << construct_time << "," << value_time << "," << convex_time << "," << violations_time
        << "," << violation_time << "," << util.num_rows << "," << util.num_cols << "\n"
        << std::flush;
  }
  //---------------------------------------------
  //----------- JointAccIneqCnt  -----------------
  //---------------------------------------------
  for (int ind = min_row; ind < max_row; ind++)
  {
    // Create cost comparison utility that contains all of the parameters
    CostsComparisonUtils util(ind, 7);

    // Define outputs
    double construct_time, value_time, convex_time, violation_time, violations_time;
    ros::Time tStart;

    // Time construction
    tStart = ros::Time::now();
    JointAccIneqConstraint object(
        util.traj, util.coeffs, util.targets, util.upper_limits, util.lower_limits, util.first_step, util.last_step);
    construct_time = (ros::Time::now() - tStart).toSec();

    // Time the evaluation
    tStart = ros::Time::now();
    object.value(util.dblvec);
    value_time = (ros::Time::now() - tStart).toSec();

    // Time convex fnc
    tStart = ros::Time::now();
    object.convex(util.dblvec, util.model.get());
    convex_time = (ros::Time::now() - tStart).toSec();

    // Time violations
    tStart = ros::Time::now();
    object.violations(util.dblvec);
    violations_time = (ros::Time::now() - tStart).toSec();

    // Time violation
    tStart = ros::Time::now();
    object.violation(util.dblvec);
    violation_time = (ros::Time::now() - tStart).toSec();

    // Display to terminal and print to file
    std::string cost_name = "JointAccIneqCnt";
    std::cout << cost_name << "   construct_time: " << construct_time << "    value_time: " << value_time
              << "    convex time: " << convex_time << "\n";
    ofs << cost_name << "," << construct_time << "," << value_time << "," << convex_time << "," << violations_time
        << "," << violation_time << "," << util.num_rows << "," << util.num_cols << "\n"
        << std::flush;
  }
  //---------------------------------------------
  //----------- JointJerkIneqCnt  -----------------
  //---------------------------------------------
  for (int ind = min_row; ind < max_row; ind++)
  {
    // Create cost comparison utility that contains all of the parameters
    CostsComparisonUtils util(ind, 7);

    // Define outputs
    double construct_time, value_time, convex_time, violation_time, violations_time;
    ros::Time tStart;

    // Time construction
    tStart = ros::Time::now();
    JointJerkIneqConstraint object(
        util.traj, util.coeffs, util.targets, util.upper_limits, util.lower_limits, util.first_step, util.last_step);
    construct_time = (ros::Time::now() - tStart).toSec();

    // Time the evaluation
    tStart = ros::Time::now();
    object.value(util.dblvec);
    value_time = (ros::Time::now() - tStart).toSec();

    // Time convex fnc
    tStart = ros::Time::now();
    object.convex(util.dblvec, util.model.get());
    convex_time = (ros::Time::now() - tStart).toSec();

    // Time violations
    tStart = ros::Time::now();
    object.violations(util.dblvec);
    violations_time = (ros::Time::now() - tStart).toSec();

    // Time violation
    tStart = ros::Time::now();
    object.violation(util.dblvec);
    violation_time = (ros::Time::now() - tStart).toSec();

    // Display to terminal and print to file
    std::string cost_name = "JointJerkIneqCnt";
    std::cout << cost_name << "   construct_time: " << construct_time << "    value_time: " << value_time
              << "    convex time: " << convex_time << "\n";
    ofs << cost_name << "," << construct_time << "," << value_time << "," << convex_time << "," << violations_time
        << "," << violation_time << "," << util.num_rows << "," << util.num_cols << "\n"
        << std::flush;
  }

  //---------------------------------------------
  //----------- JointVelEqCost_time  -----------------
  //---------------------------------------------
  for (int ind = min_row; ind < max_row; ind++)
  {
    // Create cost comparison utility that contains all of the parameters
    CostsComparisonUtils util(ind, 7);

    // Define outputs
    double construct_time, value_time, convex_time, violation_time, violations_time;
    ros::Time tStart;

    // Setup. This could be included in construction I suppose
    trajopt::VarArray vars = util.traj;
    trajopt::VarArray joint_vars = vars.block(0, 0, vars.rows(), util.num_cols - 1);
    unsigned num_vels = util.last_step - util.first_step;
    // These require multiple costs to apply to all joints
    std::vector<sco::ConstraintPtr> objects;

    // Time construction
    tStart = ros::Time::now();
    for (size_t j = 0; j < util.num_cols - 1; j++)
    {
      // Get a vector of a single column of vars
      sco::VarVector joint_vars_vec = joint_vars.cblock(util.first_step, j, util.last_step - util.first_step + 1);
      sco::VarVector time_vars_vec =
          vars.cblock(util.first_step, util.num_cols - 1, util.last_step - util.first_step + 1);

      DblVec single_jnt_coeffs = DblVec(num_vels * 2, util.coeffs[j]);
      sco::ConstraintPtr object = sco::ConstraintPtr(
          new TrajOptConstraintFromErrFunc(sco::VectorOfVectorPtr(new trajopt::JointVelErrCalculator(
                                               util.targets[j], util.upper_limits[j], util.lower_limits[j])),
                                           sco::MatrixOfVectorPtr(new trajopt::JointVelJacCalculator()),
                                           concat(joint_vars_vec, time_vars_vec),
                                           util::toVectorXd(single_jnt_coeffs),
                                           sco::EQ,
                                           "_j" + std::to_string(j)));
      objects.push_back(object);
    }
    construct_time = (ros::Time::now() - tStart).toSec();

    // Time the evaluation
    tStart = ros::Time::now();
    for (sco::ConstraintPtr object : objects)
    {
      object->value(util.dblvec);
    }
    value_time = (ros::Time::now() - tStart).toSec();

    // Time convex fnc
    tStart = ros::Time::now();
    for (sco::ConstraintPtr object : objects)
    {
      object->convex(util.dblvec, util.model.get());
    }
    convex_time = (ros::Time::now() - tStart).toSec();

    // Time violations
    tStart = ros::Time::now();
    for (sco::ConstraintPtr object : objects)
    {
      object->violations(util.dblvec);
    }
    violations_time = (ros::Time::now() - tStart).toSec();

    // Time violation
    tStart = ros::Time::now();
    for (sco::ConstraintPtr object : objects)
    {
      object->violation(util.dblvec);
    }
    violation_time = (ros::Time::now() - tStart).toSec();

    // Display to terminal and print to file
    std::string cost_name = "JointVelEqCnt_time";
    std::cout << cost_name << "   construct_time: " << construct_time << "    value_time: " << value_time
              << "    convex time: " << convex_time << "\n";
    ofs << cost_name << "," << construct_time << "," << value_time << "," << convex_time << "," << violations_time
        << "," << violation_time << "," << util.num_rows << "," << util.num_cols << "\n"
        << std::flush;
  }

  //---------------------------------------------
  //----------- JointVelIneqCost_time  -----------------
  //---------------------------------------------
  for (int ind = min_row; ind < max_row; ind++)
  {
    // Create cost comparison utility that contains all of the parameters
    CostsComparisonUtils util(ind, 7);

    // Define outputs
    double construct_time, value_time, convex_time, violation_time, violations_time;
    ros::Time tStart;

    // Setup. This could be included in construction I suppose
    trajopt::VarArray vars = util.traj;
    trajopt::VarArray joint_vars = vars.block(0, 0, vars.rows(), util.num_cols - 1);
    unsigned num_vels = util.last_step - util.first_step;
    // These require multiple costs to apply to all joints
    std::vector<sco::ConstraintPtr> objects;

    // Time construction
    tStart = ros::Time::now();
    for (size_t j = 0; j < util.num_cols - 1; j++)
    {
      // Get a vector of a single column of vars
      sco::VarVector joint_vars_vec = joint_vars.cblock(util.first_step, j, util.last_step - util.first_step + 1);
      sco::VarVector time_vars_vec =
          vars.cblock(util.first_step, util.num_cols - 1, util.last_step - util.first_step + 1);

      DblVec single_jnt_coeffs = DblVec(num_vels * 2, util.coeffs[j]);
      sco::ConstraintPtr object = sco::ConstraintPtr(
          new TrajOptConstraintFromErrFunc(sco::VectorOfVectorPtr(new trajopt::JointVelErrCalculator(
                                               util.targets[j], util.upper_limits[j], util.lower_limits[j])),
                                           sco::MatrixOfVectorPtr(new trajopt::JointVelJacCalculator()),
                                           concat(joint_vars_vec, time_vars_vec),
                                           util::toVectorXd(single_jnt_coeffs),
                                           sco::INEQ,
                                           "_j" + std::to_string(j)));
      objects.push_back(object);
    }
    construct_time = (ros::Time::now() - tStart).toSec();

    // Time the evaluation
    tStart = ros::Time::now();
    for (sco::ConstraintPtr object : objects)
    {
      object->value(util.dblvec);
    }
    value_time = (ros::Time::now() - tStart).toSec();

    // Time convex fnc
    tStart = ros::Time::now();
    for (sco::ConstraintPtr object : objects)
    {
      object->convex(util.dblvec, util.model.get());
    }
    convex_time = (ros::Time::now() - tStart).toSec();

    // Time violations
    tStart = ros::Time::now();
    for (sco::ConstraintPtr object : objects)
    {
      object->violations(util.dblvec);
    }
    violations_time = (ros::Time::now() - tStart).toSec();

    // Time violation
    tStart = ros::Time::now();
    for (sco::ConstraintPtr object : objects)
    {
      object->violation(util.dblvec);
    }
    violation_time = (ros::Time::now() - tStart).toSec();

    // Display to terminal and print to file
    std::string cost_name = "JointVelIneqCnt_time";
    std::cout << cost_name << "   construct_time: " << construct_time << "    value_time: " << value_time
              << "    convex time: " << convex_time << "\n";
    ofs << cost_name << "," << construct_time << "," << value_time << "," << convex_time << "," << violations_time
        << "," << violation_time << "," << util.num_rows << "," << util.num_cols << "\n"
        << std::flush;
  }

  //---------------------------------------------
  //----------- Close  ----------------------
  //---------------------------------------------
  ofs.close();

  return 0;
}
