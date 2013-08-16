// This file is part of the MatrixPilotQuad firmware.
//
//    http://code.google.com/p/gentlenav/
//
// Copyright 2009-2011 MatrixPilot Team
// See the AUTHORS.TXT file for a list of authors of MatrixPilot.
//
// MatrixPilot is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// MatrixPilot is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with MatrixPilot.  If not, see <http://www.gnu.org/licenses/>.

#include <libq.h>
#include "libDCM.h"
#include "motorCntrl.h"

#define MAXIMUM_ERROR_INTEGRAL ((long int) 32768000 )
#define YAW_DEADBAND 50 // prevent Tx pulse variation from causing yaw drift

extern int theta[3];
extern boolean didCalibrate;
extern int flight_mode;
int current_flight_mode = 0;
int motorsArmed = 0;

extern union longww IMUcmx, IMUvx;
extern union longww IMUcmy, IMUvy;
extern union longww IMUcmz, IMUvz;

// these are the current KP, KD and KDD loop gains in 2.14 fractional format
// valid range [0,3.99]
unsigned int pid_gains[PID_GAINS_N];
int16_t tilt_ki;
int16_t roll_kp;
int16_t roll_kd;
int16_t pitch_kp;
int16_t pitch_kd;
int16_t rrate_kp;
int16_t rrate_kd;
int16_t prate_kp;
int16_t prate_kd;
int16_t acro_kp;
int16_t rrate_kd;
int16_t prate_kd;
int16_t rate_ki;
int16_t yaw_ki;
int16_t yaw_kp;
int16_t yaw_kd;

int roll_control;
int pitch_control;
int throttle_control;
int rolladvanced, pitchadvanced;
signed char lagBC, precessBC;
int yaw_control;
int pitch_step;
struct relative2D matrix_accum;
extern boolean udb_throttle_enable;
unsigned int earth_yaw; // yaw with respect to earth frame
unsigned int desired_heading = 0;
int accel_feedback;
int rate_error_prev[2] = {0, 0};
int rate_error_dot[2] = {0, 0};

int pwManual[5]; // channels 1-4 are control inputs from RX

int roll_error, pitch_error, yaw_error;
int rate_error[3];
int rate_desired_delta[3], rate_desired_prev[3], rate_desired[3], rate_des_damping[3];

int yaw_error_previous = 0;

union longww roll_error_integral = {0};
union longww pitch_error_integral = {0};
union longww rrate_error_integral = {0};
union longww prate_error_integral = {0};
union longww yaw_error_integral = {0};

int poscmd_north, poscmd_east;
int pos_error[3], pos_setpoint[3];
union longww pos_prev[3], pos_delta[3];
int pos_perr[3], pos_derr[3];

struct int_RPY cmd_RPY, prev_RPY, adv_RPY;

// init constant portions of 1D rotation matrices
fractional Rx[9] = {RMAX, 0, 0, 0, 0, 0, 0, 0, 0};
fractional Ry[9] = {0, 0, 0, 0, RMAX, 0, 0, 0, 0};
fractional Rz[9] = {0, 0, 0, 0, 0, 0, 0, 0, RMAX};

// init Rc and Rd to identity matrix
fractional Rc[9] = {RMAX, 0, 0, 0, RMAX, 0, 0, 0, RMAX};
fractional Rd[9] = {RMAX, 0, 0, 0, RMAX, 0, 0, 0, RMAX};
fractional Rtmp[9], Rtrans[9];

#if (NUM_ROTORS == 3)
// rotate copter clockwise 30 degrees
#define X_SIN ((const fractional)(0   * 32768))
#define X_COS ((const fractional)(1 * 32768))

// these are the rotation angles for "plus" configuration, with motors:
// A right, angle 60 degrees,
// B rear, angle 180 degrees
// C left, angle 300 degrees
const fractional motor_cos[NUM_ROTORS] = {0.866 * 32768,    0,      -0.866 * 32768};
const fractional motor_sin[NUM_ROTORS] = {0.5 * 32768,      -32768, 0.5 * 32768};

int motorMap[NUM_ROTORS] = {MOTOR_A_OUTPUT_CHANNEL, MOTOR_B_OUTPUT_CHANNEL,
    MOTOR_C_OUTPUT_CHANNEL};

#elif (NUM_ROTORS == 4)

// rotate copter CCW 45 degrees
#define X_SIN ((int)(-0.707 * 32768))
#define X_COS ((int)(0.707 * 32768))

// these are the rotation angles for "plus" configuration, with motors:
// A front, angle 0 degrees,
// B right, angle 90 degrees
// C rear, angle 180 degrees
// D left, angle 270 degrees
const fractional motor_cos[NUM_ROTORS] = {0, 32767, 0, -32768};
const fractional motor_sin[NUM_ROTORS] = {32767, 0, -32768, 0};

int motorMap[NUM_ROTORS] = {MOTOR_A_OUTPUT_CHANNEL, MOTOR_B_OUTPUT_CHANNEL,
    MOTOR_C_OUTPUT_CHANNEL, MOTOR_D_OUTPUT_CHANNEL};

#elif (NUM_ROTORS == 6)

// rotate copter clockwise 30 degrees
#define X_SIN ((const fractional)(0.5   * 32768))
#define X_COS ((const fractional)(0.866 * 32768))

// these are the rotation angles for "plus" configuration, with motors:
// A front,         angle 0 degrees,
// B front right,   angle 60 degrees
// C rear right,    angle 120 degrees
// D rear,          angle 180 degrees
// E rear left,     angle 240 degrees
// F front left,    angle 300 degrees
const fractional motor_cos[NUM_ROTORS] = {0, 0.866 * 32768, 0.866 * 32768, 0, -0.866 * 32768, -0.866 * 32768};
const fractional motor_sin[NUM_ROTORS] = {32767, 0.5 * 32768, -0.5 * 32768, -32768, -0.5 * 32768, 0.5 * 32768};

int motorMap[NUM_ROTORS] = {MOTOR_A_OUTPUT_CHANNEL, MOTOR_B_OUTPUT_CHANNEL,
    MOTOR_C_OUTPUT_CHANNEL, MOTOR_D_OUTPUT_CHANNEL,
    MOTOR_E_OUTPUT_CHANNEL, MOTOR_F_OUTPUT_CHANNEL};

#else
#error ("unsupported value for NUM_ROTORS")
#endif

void rotateRP(struct int_RPY* command, signed char angle) {
    struct relative2D xy;
    xy.x = command->roll;
    xy.y = command->pitch;
    rotate(&xy, angle);
    command->roll = xy.x;
    command->pitch = xy.y;
}

void rotate2D(int *x, int *y, signed char angle) {
    struct relative2D xy;
    xy.x = *x;
    xy.y = *y;
    rotate(&xy, angle);
    *x = xy.x;
    *y = xy.y;
}

void deadBandRP(struct int_RPY* command, int bandRP) {
    if (command->roll >= bandRP) {
        command->roll -= bandRP;
    } else if (command->roll <= -bandRP) {
        command->roll += bandRP;
    } else {
        command->roll = 0;
    }

    if (command->pitch >= bandRP) {
        command->pitch -= bandRP;
    } else if (command->pitch <= -bandRP) {
        command->pitch += bandRP;
    } else {
        command->pitch = 0;
    }
}

void deadBand(int *input, int band) {
    if (*input >= band)
        *input -= band;
    else if (*input <= -band)
        *input += band;
    else
        *input = 0;
}

void magClamp(int *in, int mag) {
    if (*in < -mag)
        *in = -mag;
    else if (*in > mag)
        *in = mag;
}

void magClampRP(struct int_RPY* command, int mag) {
    if (command->roll < -mag) {
        command->roll = -mag;
    } else if (command->roll > mag) {
        command->roll = mag;
    }

    if (command->pitch < -mag) {
        command->pitch = -mag;
    } else if (command->pitch > mag) {
        command->pitch = mag;
    }
}

void magClampRPY(struct int_RPY* command, int mag) {
    if (command->roll < -mag) {
        command->roll = -mag;
    } else if (command->roll > mag) {
        command->roll = mag;
    }

    if (command->pitch < -mag) {
        command->pitch = -mag;
    } else if (command->pitch > mag) {
        command->pitch = mag;
    }

    if (command->yaw < -mag) {
        command->yaw = -mag;
    } else if (command->yaw > mag) {
        command->yaw = mag;
    }
}

void magClamp32(long *in, long mag) {
    if (*in < -mag)
        *in = -mag;
    else if (*in > mag)
        *in = mag;
}

void transposeR(fractional *t, fractional *r) {
    t[0] = r[0];
    t[1] = r[3];
    t[2] = r[6];

    t[3] = r[1];
    t[4] = r[4];
    t[5] = r[7];

    t[6] = r[2];
    t[7] = r[5];
    t[8] = r[8];
}

void motorOut(int throttle, struct int_RPY *command) {
    union longww long_accum;
    int index;

    for (index = 0; index < NUM_ROTORS; index++) {
        long_accum.WW = __builtin_mulss(motor_cos[index], command->roll);
        long_accum.WW += __builtin_mulss(motor_sin[index], command->pitch);
        long_accum.WW <<= 1; // drop extra sign bit
        int mval = throttle - long_accum._.W1;

        #if (NUM_ROTORS != 3)
        if (index & 0b1) {
            // CW rotation
            mval -= command->yaw;
        } else {
            // CCW rotation
            mval += command->yaw;
        }
        #endif
        udb_pwOut[motorMap[index]] = udb_servo_pulsesat(mval);
    }
    #if (NUM_ROTORS == 3)
    udb_pwOut[SERVO_TAIL_CHANNEL] = udb_pwTrim[RUDDER_INPUT_CHANNEL] + udb_servo_pulsesat(command->yaw);
    #endif
}

// in X configuration, rotate roll/pitch commands the amount specified by X_SIN,COS

void x_rotate(struct int_RPY* command) {
#if (CONFIG_X != 0)
    union longww long_accum;

    // for quad X config: theta = -pi/4, sin(theta) = -.707
    // x' = x cos(theta) - y sin(theta)
    long_accum.WW = __builtin_mulss(command->roll, X_COS);
    long_accum.WW -= __builtin_mulss(command->pitch, X_SIN);
    int roll_body_frame = 2 * long_accum._.W1;

    // y' = x sin(theta) + y cos(theta)
    long_accum.WW = __builtin_mulss(command->roll, X_SIN);
    long_accum.WW += __builtin_mulss(command->pitch, X_COS);
    command->pitch = 2 * long_accum._.W1;
    command->roll = roll_body_frame;
#else
    return;
#endif
}

// 1092 * 50Hz * pi/32K * 180/pi = 300 deg/sec
#define MAX_SLEW_RATE 1092

// yaw is always a rate command

void updateYaw(struct int_RPY* command) {
    int yawRate = (pwManual[RUDDER_INPUT_CHANNEL]
            - udb_pwTrim[RUDDER_INPUT_CHANNEL]);
    // apply deadband and slew rate limiter
    deadBand(&yawRate, 10);
    magClamp(&yawRate, MAX_SLEW_RATE);
    command->yaw += yawRate;
}

// perform rpy command updates at PID_HZ/8
static int rateCounter = 0;

void get_rateMode_commands(struct int_RPY* command) {
    if (rateCounter++ > 7) {
        rateCounter = 0;
        // manual rate flight mode
        // range +/- 1250 corresponds to max of 50Hz * 1250 * pi/32K * (180/pi) = 343 deg/sec
        struct int_RPY cmdRate;
        cmdRate.roll = (pwManual[AILERON_INPUT_CHANNEL] - udb_pwTrim[AILERON_INPUT_CHANNEL]);
        cmdRate.pitch = (pwManual[ELEVATOR_INPUT_CHANNEL] - udb_pwTrim[ELEVATOR_INPUT_CHANNEL]);

        // apply deadband and slew rate limit to roll and pitch
        deadBandRP(&cmdRate, 4);
        magClampRP(&cmdRate, MAX_SLEW_RATE);

        command->roll += cmdRate.roll;
        command->pitch += cmdRate.pitch;

        // rotate commanded roll/pitch into X body frame, if required
        x_rotate(command);

        // yaw is always rate mode
        updateYaw(command);
    }
}

void get_angleMode_commands(struct int_RPY* command, int tiltGain) {
    if (rateCounter++ > 7) {
        rateCounter = 0;
        // manual angle flight mode: +/-XX degrees of roll/pitch
        command->roll = (pwManual[AILERON_INPUT_CHANNEL] - udb_pwTrim[AILERON_INPUT_CHANNEL]) * tiltGain;
        command->pitch = (pwManual[ELEVATOR_INPUT_CHANNEL] - udb_pwTrim[ELEVATOR_INPUT_CHANNEL]) * tiltGain;

        // apply slew rate limit to roll and pitch
        struct int_RPY delta;
        delta.roll = command->roll - prev_RPY.roll;
        delta.pitch = command->pitch - prev_RPY.pitch;
        magClampRP(&delta, MAX_SLEW_RATE);

        command->roll = prev_RPY.roll + delta.roll;
        command->pitch = prev_RPY.pitch + delta.pitch;

        prev_RPY.roll = command->roll;
        prev_RPY.pitch = command->pitch;

        // rotate commanded roll/pitch into X body frame, if required
        x_rotate(command);

        // yaw is always rate mode
        updateYaw(command);
    }
}

void set_Rd(struct int_RPY* command) {
    // use commanded roll, pitch and yaw to generate desired orientation matrix Rd
    // constrain pitch to range [-90, 90] -> [-16384, 16384] -> [-64, 64] byte circular
    // R_d = R_z R_y R_x

    // R_x =
    // 1        0           0
    // 0    cos(pitch)  -sin(pitch)
    // 0    sin(pitch)   cos(pitch)

    // commanded_pitch is angle / pi as Q15 fractional
    fractional q15_angle = command->pitch;
    // clamp to +/-90 degrees
    magClamp(&q15_angle, 16384);
    fractional q14sin = _Q15sinPI(q15_angle) / 2;
    fractional q14cos = _Q15cosPI(q15_angle) / 2;
    Rx[4] = q14cos;
    Rx[5] = -q14sin;
    Rx[7] = q14sin;
    Rx[8] = q14cos;

    // R_y =
    // cos(roll)   0    sin(roll)
    //     0       1       0
    //-sin(roll)   0    cos(roll)

    // commanded_roll is angle / pi as Q15 fractional
    q15_angle = command->roll;
    q14sin = _Q15sinPI(q15_angle) / 2;
    q14cos = _Q15cosPI(q15_angle) / 2;
    Ry[0] = q14cos;
    Ry[2] = q14sin;
    Ry[6] = -q14sin;
    Ry[8] = q14cos;

    // R_z =
    // cos(yaw)   -sin(yaw)    0
    // sin(yaw)    cos(yaw)    0
    //    0           0        1

    // angle / pi as Q15 fractional
    q15_angle = command->yaw;
    q14sin = _Q15sinPI(q15_angle) / 2;
    q14cos = _Q15cosPI(q15_angle) / 2;
    Rz[0] = q14cos;
    Rz[1] = -q14sin;
    Rz[3] = q14sin;
    Rz[4] = q14cos;

    // construct Rd = Rz Ry Rx
    // Rd = Rz * Ry
    MatrixMultiply(3, 3, 3, Rtmp, Rz, Ry);
    //	multiply by 2 and copy back from Rtmp to Rd:
    MatrixAdd(3, 3, Rd, Rtmp, Rtmp);
    // Rd = Rd * Rx
    MatrixMultiply(3, 3, 3, Rtmp, Rd, Rx);
    //	multiply by 2 and copy back
    MatrixAdd(3, 3, Rd, Rtmp, Rtmp);
}

extern union longww primary_voltage;
extern unsigned int lowVoltageWarning;

unsigned int throttle_limit = (unsigned int) (65536 * THROTTLE_LIMIT);
extern fractional omega[];

void motorCntrl(void) {
#if DUAL_IMU == 1
    // pointer to rotation matrix
    fractional* prmat = &(mpuState.rmat[0]);
    // pointer to omegagyro vector
    //    fractional* pomegagyro = &(mpuState).omegagyro[0];
    fractional* pomega = &(mpuState).omega[0];
#else
    fractional* prmat = &rmat[0];
    //    fractional* pomega = &omegagyro[0];
    fractional* pomega = &omega[0];
#endif

    int temp;
    union longww long_accum;

    //    int posKP =0;
    int posKD = 0;

    for (temp = 0; temp <= 4; temp++) {
        if (udb_flags._.radio_on) {
            if (temp == THROTTLE_INPUT_CHANNEL) {
                // limit throttle to leave some control headroom
                long_accum.WW = __builtin_mulus(throttle_limit, (udb_pwIn[temp] - udb_pwTrim[temp]));
                pwManual[temp] = long_accum._.W1 + udb_pwTrim[temp];
            } else
                pwManual[temp] = udb_pwIn[temp];
        } else
            // If radio is off, use udb_pwTrim values instead of the udb_pwIn values
            pwManual[temp] = udb_pwTrim[temp];

    }

    if (!didCalibrate) {
        motorsArmed = 0;
    } else {
        switch (motorsArmed) {
            case 0:
                // wait for high throttle
                if ((pwManual[THROTTLE_INPUT_CHANNEL] - udb_pwTrim[THROTTLE_INPUT_CHANNEL]) > (SERVORANGE / 2))
                    motorsArmed = 1;
                break;
            case 1:
                // wait for low throttle and > half right rudder
                if (((pwManual[THROTTLE_INPUT_CHANNEL] - udb_pwTrim[THROTTLE_INPUT_CHANNEL]) < THROTTLE_DEADBAND) &&
                        (udb_pwIn[RUDDER_INPUT_CHANNEL] - udb_pwTrim[RUDDER_INPUT_CHANNEL]) > (SERVORANGE / 2)
                        ) {
                    motorsArmed = 2;
                }
                break;
            case 2:
                // wait for low throttle and neutral rudder
                if (((pwManual[THROTTLE_INPUT_CHANNEL] - udb_pwTrim[THROTTLE_INPUT_CHANNEL]) < THROTTLE_DEADBAND) &&
                        (udb_pwIn[RUDDER_INPUT_CHANNEL] - udb_pwTrim[RUDDER_INPUT_CHANNEL]) < THROTTLE_DEADBAND
                        ) {
                    motorsArmed = 3;
                    LED_RED = LED_ON;
                }
                break;
            case 3:
                // wait for low throttle and > half left rudder
                if (((pwManual[THROTTLE_INPUT_CHANNEL] - udb_pwTrim[THROTTLE_INPUT_CHANNEL]) < THROTTLE_DEADBAND) &&
                        (udb_pwIn[RUDDER_INPUT_CHANNEL] - udb_pwTrim[RUDDER_INPUT_CHANNEL]) < -(SERVORANGE / 2)
                        ) {
                    motorsArmed = 1;
                    LED_RED = LED_OFF;
                }
                break;
        }
    }

    if (motorsArmed < 3) {
        // not armed yet; set ESCs to idle
        cmd_RPY.roll = 0;
        cmd_RPY.pitch = 0;
        cmd_RPY.yaw = 0;
        motorOut(udb_pwTrim[THROTTLE_INPUT_CHANNEL], &cmd_RPY);
    } else if ((pwManual[THROTTLE_INPUT_CHANNEL] - udb_pwTrim[THROTTLE_INPUT_CHANNEL]) < THROTTLE_DEADBAND) {
        // test motor responses
        // command motors to spin at rates proportional to command
        get_angleMode_commands(&cmd_RPY, 1);
        cmd_RPY.yaw = YAW_SIGN * (pwManual[RUDDER_INPUT_CHANNEL] - udb_pwTrim[RUDDER_INPUT_CHANNEL]);
        magClampRPY(&cmd_RPY, 500);
        motorOut(udb_pwTrim[THROTTLE_INPUT_CHANNEL], &cmd_RPY);

        // init desired heading to current IMU heading
        // rotation about z is alpha = atan2(r10, r00)
        matrix_accum.y = prmat[3];
        matrix_accum.x = prmat[0];
        cmd_RPY.yaw = rect_to_polar16(&matrix_accum); // binary angle (0 - 65536 = 360 degrees)

    } else {
        // fly!
        // check flight mode
        if (flight_mode != current_flight_mode) {
            // on change of flight mode, record current IMU position
            current_flight_mode = flight_mode;
            pos_setpoint[0] = IMUcmx._.W1;
            pos_setpoint[1] = IMUcmy._.W1;
        }
        // if flight mode is POS_MODE, command roll and pitch to reduce position error
        // Positive commanded_roll rolls right and positive commanded_pitch pitches forward.

        // Now the GPS (GPSloc_cm and IMUcm) x axis points East and the y axis points North.
        // Rotate position error (dx,dy) = (desired_position - IMUcm) from the GPS frame to the body frame:
        // If vehicle heading is North, rotation is zero, if East (-90 degrees) rotation is +90 degrees.
        // For an arbitrary vehicle heading theta, rotation is then (-theta).
        // position hold flight mode: expected error mag [0, 512] cm
        pos_error[0] = pos_setpoint[0] - IMUcmx._.W1;
        pos_error[1] = pos_setpoint[1] - IMUcmy._.W1;

        // calculate 32 bit position delta
        pos_delta[0].WW = IMUcmx.WW - pos_prev[0].WW;
        pos_delta[1].WW = IMUcmy.WW - pos_prev[1].WW;
        pos_prev[0].WW = IMUcmx.WW;
        pos_prev[1].WW = IMUcmy.WW;

        // limit velocity to 50cm/sec at 5m error 0 = KP*500 - KD*50; KD=10*KP
        pos_perr[0] = POS_HOLD_KP * pos_error[0];
        pos_derr[0] = (posKD * PID_HZ * pos_delta[0].WW) >> 16;
        poscmd_east = pos_perr[0] - pos_derr[0];

        pos_perr[1] = POS_HOLD_KP * pos_error[1];
        pos_derr[1] = (posKD * PID_HZ * pos_delta[1].WW) >> 16;

        poscmd_north = pos_perr[1] - pos_derr[1];

        // clamp position hold control inputs
        magClamp(&poscmd_east, 4000);
        magClamp(&poscmd_north, 4000);

        // get heading in earth frame from rmat
        matrix_accum.x = prmat[4];
        matrix_accum.y = prmat[1];
        earth_yaw = rect_to_polar16(&matrix_accum); // binary angle (0 - 65536 = 360 degrees)

        if (flight_mode == POS_MODE) {
            // add manual and position control inputs
            get_angleMode_commands(&cmd_RPY, CMD_TILT_GAIN);
            cmd_RPY.roll += poscmd_east;
            cmd_RPY.pitch += poscmd_north;
            // rotate forward stick North (angle -heading)
            rotateRP(&cmd_RPY, (-earth_yaw) >> 8);
        } else if (flight_mode == COMPASS_MODE) {
            // manual mode: forward cyclic is North
            get_angleMode_commands(&cmd_RPY, CMD_TILT_GAIN);
            // rotate forward stick North (angle -heading)
            rotateRP(&cmd_RPY, (-earth_yaw) >> 8);
        } else if (flight_mode == RATE_MODE) {
            // manual (rate) flight mode
            get_rateMode_commands(&cmd_RPY);
        } else if (flight_mode == TILT_MODE) {
            // manual (tilt) flight mode
            get_angleMode_commands(&cmd_RPY, CMD_TILT_GAIN);
        }
        set_Rd(&cmd_RPY);

        // construct Rc = Rimu^T * Rd
        transposeR(Rtrans, prmat);
        MatrixMultiply(3, 3, 3, Rtmp, Rtrans, Rd);

        // This should be more efficient than the above two lines (from rmat.c)
        // *** Note: this accomplishes multiplication rmat transpose times Rd!!
//        MatrixMultiply(1, 3, 3, Rtmp, Rd, rmat);

        // multiply by 2 and copy back
        MatrixAdd(3, 3, Rc, Rtmp, Rtmp);

        // error Rc = Rimu^T * Rd and the error angles are Rc as xyz fixed angles

        // rotation about x axis is pitch
        // gamma = atan2(r21, r22)
        matrix_accum.y = Rc[7];
        matrix_accum.x = Rc[8];
        pitch_error = rect_to_polar16(&matrix_accum); // binary angle (0 - 65536 = 360 degrees)

        // rotation about y is roll
        // beta = atan2(-r20, sqrt(r00^2 + r10^2))
        matrix_accum.y = -Rc[6];
        matrix_accum.x = sqrt(pow(Rc[0], 2) + pow(Rc[3], 2));
        roll_error = rect_to_polar16(&matrix_accum); // binary angle (0 - 65536 = 360 degrees)

        // rotation about z is yaw
        // alpha = atan2(r10, r00)
        matrix_accum.y = Rc[3];
        matrix_accum.x = Rc[0];
        yaw_error = rect_to_polar16(&matrix_accum); // binary angle (0 - 65536 = 360 degrees)

        //        // light taillight whenever heading is within 5 degrees of North
        //        if (flight_mode == COMPASS_MODE) {
        //            if (abs((int) earth_yaw) < 910)
        //                TAIL_LIGHT = LED_ON;
        //            else
        //                TAIL_LIGHT = LED_OFF;
        //        }

        // in all flight modes, control orientation
        {
            // Compute the tilt error integrals
            roll_error_integral.WW += ((__builtin_mulus(pid_gains[TILT_KI_INDEX], roll_error)) >> 8);
            magClamp32(&roll_error_integral.WW, MAXIMUM_ERROR_INTEGRAL);

            pitch_error_integral.WW += ((__builtin_mulus(pid_gains[TILT_KI_INDEX], pitch_error)) >> 8);
            magClamp32(&pitch_error_integral.WW, MAXIMUM_ERROR_INTEGRAL);

            // Use tilt error as desired rate, with gain TILT_KP
            long_accum.WW = __builtin_mulus(pid_gains[ROLL_KP_INDEX], roll_error);
            rate_desired[0] = long_accum._.W1;

            // damp the desired roll rate
            // compute backward first difference of rate_desired
            rate_desired_delta[0] = PID_HZ / 400 * (rate_desired[0] - rate_desired_prev[0]);
            rate_desired_prev[0] = rate_desired[0];
            long_accum.WW = __builtin_mulus((unsigned int) (RMAX * ROLL_KD), rate_desired_delta[0]) << 2;
            rate_des_damping[0] = long_accum._.W1;

            rate_error[0] = rate_des_damping[0] + rate_desired[0] - pomega[1];
            rate_error[0] += roll_error_integral._.W1;

            long_accum.WW = __builtin_mulus(pid_gains[PITCH_KP_INDEX], pitch_error);
            rate_desired[1] = long_accum._.W1;

            // damp the desired pitch rate
            // compute backward first difference of rate_desired
            rate_desired_delta[1] = PID_HZ / 400 * (rate_desired[1] - rate_desired_prev[1]);
            rate_desired_prev[1] = rate_desired[1];
            long_accum.WW = __builtin_mulus((unsigned int) (RMAX * PITCH_KD), rate_desired_delta[1]) << 2;
            rate_des_damping[1] = long_accum._.W1;

            rate_error[1] = rate_des_damping[1] + rate_desired[1] - pomega[0];
            rate_error[1] += pitch_error_integral._.W1;
        }

        // Compute the rate error integrals
        rrate_error_integral.WW += ((__builtin_mulus(pid_gains[RATE_KI_INDEX], rate_error[0])) >> 8);
        magClamp32(&rrate_error_integral.WW, MAXIMUM_ERROR_INTEGRAL);
        rate_error[0] += rrate_error_integral._.W1;

        prate_error_integral.WW += ((__builtin_mulus(pid_gains[RATE_KI_INDEX], rate_error[1])) >> 8);
        magClamp32(&prate_error_integral.WW, MAXIMUM_ERROR_INTEGRAL);
        rate_error[1] += prate_error_integral._.W1;

        // compute backward first difference of rate_error for D term in rate
        // control loop. For PID_HZ=400, feedback gain is RATE_KD
        rate_error_dot[0] = PID_HZ / 400 * (rate_error[0] - rate_error_prev[0]);
        rate_error_dot[1] = PID_HZ / 400 * (rate_error[1] - rate_error_prev[1]);
        rate_error_prev[0] = rate_error[0];
        rate_error_prev[1] = rate_error[1];

        long_accum.WW = __builtin_mulus(pid_gains[RRATE_KP_INDEX], rate_error[0]);
        roll_control = long_accum._.W1;
        long_accum.WW = __builtin_mulus(pid_gains[RRATE_KD_INDEX], rate_error_dot[1]);
        roll_control += long_accum._.W1;

        long_accum.WW = __builtin_mulus(pid_gains[PRATE_KP_INDEX], rate_error[1]);
        pitch_control = long_accum._.W1;
        long_accum.WW = __builtin_mulus(pid_gains[PRATE_KD_INDEX], rate_error_dot[0]);
        pitch_control += long_accum._.W1;

        yaw_error_integral.WW += ((__builtin_mulus(pid_gains[YAW_KI_INDEX], yaw_error)) >> 8);
        magClamp32(&yaw_error_integral.WW, MAXIMUM_ERROR_INTEGRAL);

        // use heading error * KP as desired yaw rate
        long_accum.WW = __builtin_mulus(pid_gains[YAW_KP_INDEX], yaw_error);
        rate_error[2] = long_accum._.W1 - pomega[2];
        rate_error[2] += yaw_error_integral._.W1;

        long_accum.WW = __builtin_mulus(pid_gains[YAW_KD_INDEX], rate_error[2]);
        yaw_control = YAW_SIGN * long_accum._.W1;

        // compensate for gyroscopic reaction torque proportional to omegagyro[2]
        // Suppose the relative magnitude of gyro. reaction is omega_z in rad/sec:
        // if this is the ratio of gyroscopic torque to applied torque, then the
        // resultant angle shift is atan(omega_z rad/sec)
        //TODO: loop gain will increase directly with magnitude of omega: compensate
        union longww omega_z; // result in rad/sec, decimal point between words
        omega_z.WW = __builtin_mulus((unsigned int) (65536 * (PI / 180) / DEGPERSEC), pomega[2]);

        // _Q16atan is a 2-quadrant 32 bit fixed point arctangent
        // domain is [-2^15, 2^15-1]
        // result is Q16 radians, range (-pi/2, pi/2)
        long rotLong;
        rotLong = _Q16atan(omega_z.WW);

        // convert to 8 bit byte circular assuming a range of +/-pi/2 radians
        // rotWord is radians * 2^14
        int rotWord = 0xFFFF & (rotLong >> 2);
        union longww rotAngle;
        // lagAngle high word is radians * 2^14/PI
        // *** note that rotWord is negated here ***
        rotAngle.WW = __builtin_mulus((unsigned int) (65536 / PI), -rotWord);
        // lagBC is in byte circular form; should wrap correctly at 360 degrees
        precessBC = 0xFF & (rotAngle._.W1 >> 6);

        // compensate for rate control latency when spinning;
        // try 70msec acceleration lag for aeroFPV
        // result 1.15 fractional format with lsb weight DEGPERSEC count/degree
        union longww lagAngle; // low byte of high word is byte circular angle
        lagAngle.WW = __builtin_mulus((unsigned int) (65536 * (128.0 / 180) * 0.07 / DEGPERSEC), pomega[2]);
        // lagBC is in byte circular form; should wrap correctly at 360 degrees
        lagBC = 0xFF & lagAngle._.W1;

        adv_RPY.roll = roll_control;
        adv_RPY.pitch = pitch_control;
        adv_RPY.yaw = yaw_control;
        rotateRP(&adv_RPY, lagBC);
        magClampRPY(&adv_RPY, RPY_CLAMP);

        // Compute the signals that are common to all 4 motors
        //        long_accum.WW = __builtin_mulus((unsigned int) pid_gains[ACCEL_K_INDEX], accelEarth[2]);
        //        accel_feedback = long_accum._.W1;
        int throttle = pwManual[THROTTLE_INPUT_CHANNEL]; // - accel_feedback;

        // Mix in the yaw, pitch, and roll signals into the motors
        motorOut(throttle, &adv_RPY);
    }
}