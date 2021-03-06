/** @file modules/trajectory_manager/holonomic/trajectory_manager.h
 * @author CVRA
 * @brief High-level trajectory generation for an holonomic robot
 * 
 * A trajectory consist of two things : a moving part and a turning part
 * Avaible Moving Part :
 *  - A Straight line
 *  - An arc of circle
 * Avaible Turning Part :
 *  - Fixed cap
 *  - Face a point
 *  - Fixed offset from the speed vector of the robot
 * 
 * The module output to robot_system the speed, the angle of the speed
 * and the angular speed of the robot
 */
 

#ifndef _TRAJECTORY_MANAGER_HOLONOMIC_H_
#define _TRAJECTORY_MANAGER_HOLONOMIC_H_

#include <aversive.h>
#include <aversive/error.h>
#include <holonomic/robot_system.h>
#include <holonomic/position_manager.h>
#include <vect2.h>



/* Moving trajectories */
enum h_trajectory_moving_state {
    MOVING_STRAIGHT,
    MOVING_CIRCLE,
    MOVING_IDLE,
};
/* Turning trajectories */
enum h_trajectory_turning_state {
    TURNING_CAP,
    TURNING_SPEEDOFFSET,
    TURNING_FACEPOINT,
    TURNING_IDLE,
};

/** A complete instance of the trajectory manager. */
struct h_trajectory {
    enum h_trajectory_moving_state moving_state;
    enum h_trajectory_turning_state turning_state;
    
                  
    vect2_cart xy_target; /**< Target for the moving part */
    double a_target;      /**< Target for the turning part */
    
    vect2_cart circle_center; /**< Center of the circle for MOVING_CIRCLE */
    double arc_angle;         /**< For MOVING_CIRCLE : PI/2 -> a quarter of a cirlce  */
    double radius;          /**< For MOVING_CIRCLE : radius of the circle  */
    vect2_cart point2face;    /**< Point to face for TURNING_FACEPOINT */
    
    /** Output to robot_system @todo : leur init à 0 
     * NOT USED YET*/
    double speed;
    double direction;
    double omega;
    
    /** Windows for arrival */
    double d_win;
    double a_win;

    /** Flag end of trajectory */
    int8_t end_of_traj;
    
    /** Position of the robot */
    struct holonomic_robot_position *position;
    
    /** Binded robot system */
    struct robot_system_holonomic *robot;
    
    /** Associated ramps */
    struct ramp_filter *speed_r;
    struct quadramp_filter *angle_qr;
    struct ramp_filter *omega_r;
    
    double cs_hz;   /**< The frequency of the control system associated with this manager. */

    int8_t scheduler_task;    /**<< id of current task (-1 if no running task) */
};

/** @brief Structure initialization.
 *
 * @param [in] traj The trajectory manager to initialize.
 * @param [in] cs_hz The frequency of the control systems, in Hz. */
void holonomic_trajectory_init(struct h_trajectory *traj, double cs_hz);

/** @brief Sets the ramps.
 *
 * This function tells the trajectory manager which ramps to use for
 * angle/speed/omega(andgular speed) control.
 *
 * @param [in] traj The trajectory manager instance.
 * @param [in] speed_r, angle_qr, omega_r   The ramp to use.
 */
void holonomic_trajectory_set_ramps(struct h_trajectory *traj, struct ramp_filter *speed_r,
               struct quadramp_filter *angle_qr, struct ramp_filter *omega_r);
               
/** @brief Sets related robot params.
 *
 * Sets the robot_pos and robot_system used for the computation of the trajectory.
 * @param [in] traj The trajectory manager instance.
 * @param [in] rs   The related robot system.
 * @param [in] pos  The position manager instance of the robot. 
 */
void holonomic_trajectory_set_robot_params(struct h_trajectory *traj,
                 struct robot_system_holonomic *rs,
                 struct holonomic_robot_position *pos);

/** @brief Sets the windows for arrival
 *
 * If the robot is in this windows, the the trajectory is finished
 * @param [in] traj The trajectory manager instance.
 * @param [in] d_win The max distance to be in the windows
 * @param [in] a_win  The max angle difference to be in the windows
 */
void holonomic_trajectory_set_windows(struct h_trajectory *traj,
                 double d_win, double a_win);
                      
/** @brief Manually set the consign.
 *
 * @param [in] traj The trajectory manager instance.
 * @param [in] speed The speed consign @todo : units
 * @param [in] direction The direction consign @todo : units
 * @param [in] omega The angular speed consign @todo : units
 */
void holonomic_trajectory_set_var(struct h_trajectory *traj, int32_t speed, int32_t direction, int32_t omega);
                 
/** @brief Go to a point.
 *
 * This function makes the holonomic robot go to a point. Once the function is called, the
 * trajectory manager schedules its own event in the scheduler to regulate on
 * the point. This event is automatically deleted once the point is reached.
 *
 * @param [in] traj The trajectory manager instance.
 * @param [in] x_abs_mm, y_abs_mm The coordinates of the point in mm.
 *
 * @note This moving command is mixed with the current turning command 
 */
void holonomic_trajectory_moving_straight_goto_xy_abs(struct h_trajectory *traj, double x_abs_mm, double y_abs_mm);

/** @brief Go make a circle arpund the specified center. The robot start from his position.
 *
 * This function makes the holonomic robot do a circle. Once the function is called, the
 * trajectory manager schedules its own event in the scheduler to regulate on
 * the point. This event is automatically deleted once the point is reached.
 *
 * @param [in] traj The trajectory manager instance.
 * @param [in] x_center_abs,  y_center_abs The coordinates of the center in mm.
 * @param [in] arc_angle the portion of circle in rad (2*pi = full circle; pi = 1/2 of a circle)
 *
 * @note This moving command is mixed with the current turning command 
 */
void holonomic_trajectory_moving_circle(struct h_trajectory *traj, double x_center_abs, double y_center_abs, double arc_angle);

/** @brief Make the heading of the robot a constant cap.
 *
 * @param [in] traj The trajectory manager instance.
 * @param [in] cap The cap to follow, in radian
 *
 * @note This moving command is mixed with the current moving command 
 */
 void holonomic_trajectory_turning_cap(struct h_trajectory *traj, double cap);

/** True when trajectory is finished */
int8_t holonomic_end_of_traj(struct h_trajectory *traj);


/** True if the robot is within the distance d_win of the trajectory's target */
uint8_t holonomic_robot_in_xy_window(struct h_trajectory *traj, double d_win);


void holonomic_delete_event(struct h_trajectory *traj);



#endif

