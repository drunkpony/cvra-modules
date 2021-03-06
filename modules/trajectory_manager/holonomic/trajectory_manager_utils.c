#include <math.h>
#include <holonomic/trajectory_manager_utils.h>
#include <scheduler.h>
#include <quadramp.h>
#include <ramp.h>
#include <fast_math.h>

/* These are just placeholders to pass to some functions, they need to be replaced. */
#define RAD 10
//static vect2_cart FP = {.x = 150., .y = 0.};
#define ANG 1.5

#define SPEED_ROBOT 500

/** the angle between two keyframe */
#define ANGLE_INC 0.05

static int prev_speed = 0;
int32_t holonomic_do_ramp(struct h_trajectory *traj, int32_t consign);

void holonomic_trajectory_manager_event(void * param)
{
    ///@todo : probablement des fonctions de la lib math qui font ça
    struct h_trajectory *traj = (struct h_trajectory *) param;
    double x = holonomic_position_get_x_double(traj->position);
    double y = holonomic_position_get_y_double(traj->position);
    vect2_cart vector_pos;
    int32_t s_consign = 0;  /**< The speed consign */
    int32_t a_consign = 0;  /**< The angle consign */
    int32_t o_consign = 0;  /**< The angular speed (omega) consign */
    
    float target_norm =  sqrtf(pow(traj->xy_target.x, 2) + pow(traj->xy_target.y, 2));      // TODO : variable never used
    float position_norm = sqrtf(pow(x, 2) + pow(y, 2));                                     // TODO : variable never used
    int32_t distance2target = sqrtf(pow(x - traj->xy_target.x, 2) + pow(y - traj->xy_target.y, 2));
    
    vector_pos.x = x;
    vector_pos.y = y;
    
    /** @todo : move the following in the right siwtch-case */
    vect2_cart vec_target = {.x = traj->xy_target.x - x,
                             .y = traj->xy_target.y - y};
                             
    vect2_cart vec_to_center = {.x = traj->circle_center.x - x,     // TODO : variable never used
                                .y = traj->circle_center.y - y};
    
    static vect2_cart keyframe = {.x = -1,
                                   .y = -1};
    
                             
    /* step 1 : process new commands to quadramps */
    switch (traj->moving_state) 
    {
         case MOVING_STRAIGHT:
            /* Calcul de la consigne d'angle */
            a_consign = TO_DEG(vect2_angle_vec_x_rad_cart(&vec_target));
            /* Calcul de la consigne de vitesse */
            s_consign = SPEED_ROBOT;
            if (distance2target < 250)
                s_consign = 2*distance2target;
            break;
         case MOVING_CIRCLE:
            if (keyframe.x < 0) {
                keyframe.x = traj->circle_center.x + cos(atan2f(y - traj->circle_center.y, 
                   x - traj->circle_center.x) - ANGLE_INC)*traj->radius;
                keyframe.y = traj->circle_center.y + sin(atan2f(y - traj->circle_center.y, 
                    x - traj->circle_center.x)-ANGLE_INC)*traj->radius;
                    }
                traj->xy_target.x = keyframe.x;
            traj->xy_target.y = keyframe.y;
            /* Calcul de la consigne d'angle */
            a_consign = TO_DEG(vect2_angle_vec_x_rad_cart(&vec_target));
            printf("%d\n", a_consign);
            s_consign = SPEED_ROBOT/5;
            //if (distance2target < 250) ///@todo : faire un truc avec holonomic_length_arc_of_circle_pnt(traj, RAD)) plutôt.
                //s_consign = 2*distance2target;
            break;
        case MOVING_IDLE:
            break;
    }

    switch (traj->turning_state)
    {
        case TURNING_CAP:
            if(traj->a_target - holonomic_position_get_a_rad_double(traj->position) < 0
                    || traj->a_target - holonomic_position_get_a_rad_double(traj->position) > M_PI)
                o_consign = 50;
            else
                o_consign = -50;//holonomic_angle_2_x_rad(traj, traj->a_target);//cs_do_process(traj->csm_omega, holonomic_angle_2_x_rad(traj, ANG));
            break;
        case TURNING_SPEEDOFFSET:
            o_consign = 1;//cs_do_process(traj->csm_omega, holonomic_angle_2_speed_rad(traj, ANG));
            break;
        case TURNING_FACEPOINT:
            o_consign = 1;//cs_do_process(traj->csm_omega,  holonomic_angle_facepoint_rad(traj, &FP));
            break;
        case TURNING_IDLE:
            break;
    }
    /* step 3 : check the end of the move */
    if (traj->turning_state == TURNING_IDLE && holonomic_robot_in_xy_window(traj, traj->d_win)) //@todo : not only distance, angle
     {
        if (traj->moving_state == MOVING_CIRCLE)
        {
            keyframe.x = traj->circle_center.x + cos(atan2f(y - traj->circle_center.y, 
                   x - traj->circle_center.x) - ANGLE_INC) * traj->radius;
            keyframe.y = traj->circle_center.y + sin(atan2f(y - traj->circle_center.y, 
                    x - traj->circle_center.x) - ANGLE_INC) * traj->radius;
                    
            printf("x: %f      y: %f\n", keyframe.x, keyframe.y);
            
            // If we have finished the circular trajectory 
            /**@todo do not work beaucause the pos has changed -> take the depaeture of the pos of restart for scratch */ 
            vect2_cart arrival = {  .x = traj->circle_center.x + cos(atan2f(y - traj->circle_center.y, 
                   x - traj->circle_center.x) - traj->arc_angle) * traj->radius,
                                    .y = traj->circle_center.y + sin(atan2f(y - traj->circle_center.y, 
                   x - traj->circle_center.x) - traj->arc_angle) * traj->radius};

            printf("Arrival : x: %f      y: %f\n", arrival.x, arrival.y);
            if (vect2_dist_cart(&arrival, &vector_pos) < traj->d_win)
            {
                holonomic_delete_event(traj);
            }
            return;
        }
         
        if (prev_speed < 20)
        {
           traj->moving_state = MOVING_IDLE;
           holonomic_delete_event(traj);
           return;
       }
        else
            s_consign = 0;
        
    }
    if (traj->moving_state == MOVING_IDLE && holonomic_robot_in_angle_window(traj, traj->a_win))
    {

        traj->turning_state = TURNING_IDLE;
        holonomic_delete_event(traj);
        return;
        
    }
            
    /* step 2 : pass the consign to rsh */
    set_consigns_to_rsh(traj, s_consign, a_consign, o_consign);
    /** @todo : re-init le keyframe !!! */
}

/** near the target (dist in x,y) ? */
uint8_t holonomic_robot_in_xy_window(struct h_trajectory *traj, double d_win)
{
    vect2_cart vcp = {.x = holonomic_position_get_x_double(traj->position),
                      .y = holonomic_position_get_y_double(traj->position)};
    return (vect2_dist_cart(&vcp, &traj->xy_target) < d_win);
    
}

/** returns true if the robot is in an area enclosed by a certain angle */
uint8_t holonomic_robot_in_angle_window(struct h_trajectory *traj, double a_win_rad)
{
    double d_a =  traj->a_target - holonomic_position_get_a_rad_double(traj->position);

    if (ABS(d_a) < M_PI) {
        return (ABS(d_a) < (a_win_rad/2));
    } else {
        return ((2 * M_PI-ABS(d_a)) < (a_win_rad/2));
    }
}

/** remove event if any @todo */
void holonomic_delete_event(struct h_trajectory *traj)
{
    prev_speed = 0;
    traj->end_of_traj = 1;
    
    set_consigns_to_rsh(traj, 0, holonomic_position_get_theta_v_int(traj->position), 0);
    /** do not work without this : */
    rsh_set_speed(traj->robot,0);
    
    if ( traj->scheduler_task != -1) {
        DEBUG(E_TRAJECTORY, "Delete event");
        scheduler_del_event(traj->scheduler_task);
        traj->scheduler_task = -1;
    }
}

/** schedule the trajectory event */
void holonomic_schedule_event(struct h_trajectory *traj)
{
    if ( traj->scheduler_task != -1) {
        DEBUG(E_TRAJECTORY, "Schedule event, already scheduled");
    }
    else {
        traj->scheduler_task =
            scheduler_add_periodical_event_priority(&holonomic_trajectory_manager_event,
                                (void*)traj,
                                TRAJ_EVT_PERIOD, 30);
    }
}

/** do a modulo 2.pi -> [-Pi,+Pi], knowing that 'a' is in [-3Pi,+3Pi] */
double holonomic_simple_modulo_2pi(double a)
{
    if (a < -M_PI) {
        a += M_PI_2;
    }
    else if (a > M_PI) {
        a -= M_PI_2;
    }
    return a;
}

/** do a modulo 2.pi -> [-Pi,+Pi] */
double holonomic_modulo_2pi(double a)
{
    double res = a - (((int32_t) (a/M_PI_2)) * M_PI_2);
    return holonomic_simple_modulo_2pi(res);
}

/** calculates the lenght of an arc of a circle given an end point and a radius */
float holonomic_length_arc_of_circle_pnt(struct h_trajectory *traj, float rad)
{
    vect2_cart vcp = {.x = holonomic_position_get_x_double(traj->position),
                      .y = holonomic_position_get_y_double(traj->position)};
    float d_r = vect2_dist_cart(&vcp, &traj->xy_target) / rad;

    /* law of cosines */
    return (rad * fast_acosf(1 - 0.5 * d_r * d_r));
}

int32_t holonomic_do_ramp(struct h_trajectory *traj, int32_t consign)
{

    int32_t step = 20;
    
    //if (consign < step) step = consign;
    
    prev_speed = prev_speed + ((consign>=prev_speed) ? step : -step);
    
    return prev_speed;
}

void set_consigns_to_rsh(struct h_trajectory *traj, int32_t speed, int32_t direction, int32_t omega)
{

    /** @todo ramp don't work */
    /** A ramp, on donne l'objectif de vitesse (ex : 100 mm/s) et il fait avec la pente max défine dans cvra_cs.h */
    rsh_set_speed(traj->robot, holonomic_do_ramp(traj, speed));
    //rsh_set_rotation_speed(traj->robot, ramp_do_filter(traj->omega_r, omega));
    ///** A quadramp on donne l'angle désiré (ex : 90 deg) et il fait avec la vitesse et l'acc max défine dans cvra_cs.h */
    //rsh_set_direction_int(traj->robot, quadramp_do_filter(traj->angle_qr, direction));
    /** @todo @bug : ramp don't work, doing my own ramps*/

   // rsh_set_speed(traj->robot, speed);
    rsh_set_direction_int(traj->robot, direction);
    rsh_set_rotation_speed(traj->robot,omega);
}

/** Calculates the angle between the robot and a facing point. */
float holonomic_angle_facepoint_rad(struct h_trajectory *traj, vect2_cart *fpc)
{
    float a_fp = vect2_angle_vec_x_rad_cart(fpc);
    float d_a = a_fp - holonomic_position_get_a_rad_float(traj->position);

    return (holonomic_best_delta_angle_rad(d_a));
}

/** Calculates the difference of the robots angle to angle of the speedvector plus offset. */
float holonomic_angle_2_speed_rad(struct h_trajectory *traj, float ao)
{
    float d_a = ((holonomic_position_get_theta_v(traj->position) + ao)
                - holonomic_position_get_a_rad_float(traj->position));

    return (holonomic_best_delta_angle_rad(d_a));
}

/** Calculates the difference between the angle of the robot and a wished angle. */
float holonomic_angle_2_x_rad(struct h_trajectory *traj, float a)
{
    float d_a = (a - holonomic_position_get_a_rad_float(traj->position));

    return (holonomic_best_delta_angle_rad(d_a));
}

/** Calculates if the positiv or the negativ rotation is better. */
float holonomic_best_delta_angle_rad(float a)
{
    if (a > M_PI) {
        return (a - 2*M_PI);
    } else if (a < (-M_PI)){
        return (2*M_PI - a);
    } else {
        return a;
    }
}
