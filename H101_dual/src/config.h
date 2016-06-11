
#include "defines.h"

/*

 .----------------.  .----------------.  .----------------.  .----------------. 
| .--------------. || .--------------. || .--------------. || .--------------. |
| |  ____  ____  | || |     __       | || |     ____     | || |     __       | |
| | |_   ||   _| | || |    /  |      | || |   .'    '.   | || |    /  |      | |
| |   | |__| |   | || |    `| |      | || |  |  .--.  |  | || |    `| |      | |
| |   |  __  |   | || |     | |      | || |  | |    | |  | || |     | |      | |
| |  _| |  | |_  | || |    _| |_     | || |  |  `--'  |  | || |    _| |_     | |
| | |____||____| | || |   |_____|    | || |   '.____.'   | || |   |_____|    | |
| |              | || |              | || |              | || |              | |
| '--------------' || '--------------' || '--------------' || '--------------' |
 '----------------'  '----------------'  '----------------'  '----------------' 

***DO NOT FLASH H8 FIRMWARE TO THE H101 BY ACCIDENT***

*/

// rate pids in pid.c ( for acro mode)
// angle pids in apid.h ( for level mode )
// level mode also dependent on acro pids

// yaw is the same for both modes

// not including the "f" after float numbers will give a warning
// it will still work



// rate in deg/sec
// for low rates ( acro mode)
#define MAX_RATE 180.0f
#define MAX_RATEYAW 180.0f

// multiplier for high rates
// devo/module uses high rates only
#define HIRATEMULTI 2.0f
#define HIRATEMULTIYAW 2.0f

// max angle for level mode (in degrees)
// low and high rates(angle?)
#define MAX_ANGLE_LO 35.0f
#define MAX_ANGLE_HI 55.0f

// max rate for rate pid in level mode
// this should usually not change unless faster / slower response is desired.
#define LEVEL_MAX_RATE_LO 360.0f
#define LEVEL_MAX_RATE_HI 360.0f


// this make the controls mirrored in inverted mode
//#define NATIVE_INVERTED_MODE

// 3d throttle - center off
//#define THREE_D_THROTTLE

// deadzone in center of 3d throttle 0.0 - 1.0 
#define THREE_D_THROTTLE_DEADZONE 0.2


// disable inbuilt expo functions
#define DISABLE_EXPO

// use if your tx has no expo function
// also comment out DISABLE_EXPO to use
// -1 to 1 , 0 = no exp
// positive = less sensitive near center 
#define EXPO_XY 0.3f
#define EXPO_YAW 0.0f



// Hardware gyro LPF filter frequency
// gyro filter 0 = 250hz delay 0.97mS
// gyro filter 1 = 184hz delay 2.9mS
// gyro filter 2 = 92hz delay 3.9mS
// gyro filter 3 = 41hz delay 5.9mS (Default)
// gyro filter 4 = 20hz
#define GYRO_LOW_PASS_FILTER 3

// software gyro lpf ( iir )
// set only one below
//#define SOFT_LPF_1ST_023HZ
//#define SOFT_LPF_1ST_043HZ
//#define SOFT_LPF_1ST_100HZ
//#define SOFT_LPF_2ND_043HZ
//#define SOFT_LPF_2ND_088HZ
#define SOFT_LPF_4TH_088HZ
//#define SOFT_LPF_4TH_160HZ
//#define SOFT_LPF_4TH_250HZ
//#define SOFT_LPF_NONE

// this works only on newer boards (non mpu-6050)
// on older boards the hw gyro setting controls the acc as well
#define ACC_LOW_PASS_FILTER 5



// Headless mode
// CH_FLIP - 0 - flip 
// CH_EXPERT - 1 - expert
// CH_HEADFREE - 2 - headfree
// CH_RTH - 3 - headingreturn
// CH_AUX1 - 4 - AUX1 ( gestures <<v and >>v)
// CH_AUX2 - 5 - AUX2+ (  up - up - up    )
// CH_PIT_TRIM - 6 - Pitch trims
// CH_RLL_TRIM - 7 - Roll trims
// CH_THR_TRIM - 8 - Throttle trims
// CH_YAW_TRIM - 9 - Yaw trims
// CH_ON - 10 - on always
// CH_OFF - 11 - off always
//
// CH_ON , CH_OFF , CH_FLIP , CH_EXPERT
// CH_HEADFREE , CH_RTH , CH_AUX1 , CH_AUX2 , CH_AUX3 , CH_AUX4
// CH_PIT_TRIM, CH_RLL_TRIM, CH_THR_TRIM, CH_YAW_TRIM
#define HEADLESSMODE CH_OFF


// rates / expert mode
// same settings as above (default - CH_EXPERT)
#define RATES CH_EXPERT


// level / acro mode switch
// CH_AUX1 = gestures (default - CH_AUX1)
// same settings as above
#define LEVELMODE CH_AUX1


// channel for inverted mode
// same settings as above (default - off)
#define INVERTEDMODE CH_OFF


// aux1 channel starts on if this is defined, otherwise off.
#define AUX1_START_ON

// use yaw/pitch instead of roll/pitch for gestures
//#define GESTURES_USE_YAW

// comment out if not using ( disables trim as channels, will still work with stock tx except that feature )
// remember if using trims as switches => devo/tx module incompatible
//#define USE_STOCK_TX

// automatically remove center bias ( needs throttle off for 1 second )
//#define STOCK_TX_AUTOCENTER


// throttle angle compensation in level mode
// comment out to disable
//#define AUTO_THROTTLE


// enable auto lower throttle near max throttle to keep control
// comment out to disable
//#define MIX_LOWER_THROTTLE

// options for mix throttle lowering if enabled
// 0 - 100 range ( 100 = full reduction / 0 = no reduction )
#define MIX_THROTTLE_REDUCTION_PERCENT 100
// lpf (exponential) shape if on, othewise linear
//#define MIX_THROTTLE_FILTER_LPF



// battery saver ( only at powerup )
// does not start software if battery is too low
// flashes 2 times repeatedly at startup
#define STOP_LOWBATTERY

// under this voltage the software will not start 
// if STOP_LOWBATTERY is defined above
#define STOP_LOWBATTERY_TRESH 3.3f

// voltage too start warning
// volts
#define VBATTLOW 3.5f

// compensation for battery voltage vs throttle drop
// increase if battery low comes on at max throttle
// decrease if battery low warning goes away at high throttle
// in volts
#define VDROP_FACTOR 0.60f

// voltage hysteresys
// in volts
#define HYST 0.10f

//#define LVC_PREVENT_RESET
#define LVC_PREVENT_RESET_VOLTAGE 2.85


// enable motor filter
// hanning 3 sample fir filter
#define MOTOR_FILTER

// lost quad beeps using motors
//#define MOTOR_BEEPS

// clip feedforward attempts to resolve issues that occur near full throttle
//#define CLIP_FF

// motor transient correction applied	to throttle stick
//#define THROTTLE_TRANSIENT_COMPENSATION



// motor curve to use
// the pwm frequency has to be set independently
//#define MOTOR_CURVE_NONE
//#define MOTOR_CURVE_6MM_H101_490HZ
//#define MOTOR_CURVE_6MM_490HZ
#define MOTOR_CURVE_85MM_8KHZ
//#define MOTOR_CURVE_85MM_32KHZ

// pwm frequency for motor control
// a higher frequency makes the motors more linear
//#define PWM_490HZ
#define PWM_8KHZ
//#define PWM_16KHZ
//#define PWM_24KHZ
//#define PWM_32KHZ

// failsafe time in uS
#define FAILSAFETIME 1000000  // one second



// ########################################
// things that are experimental / old / etc
// do not change things below

// invert yaw pid for hubsan motors
//#define INVERT_YAW_PID

//some debug stuff
//#define DEBUG

// disable motors for testing
//#define NOMOTORS


// time to change motor direction (uS)
#define BRIDGE_TIMEOUT 100000

// enable motors if pitch / roll controls off center (at zero throttle)
// possible values: 0 / 1
#define ENABLESTIX 0
#define ENABLESTIX_TRESHOLD 0.3


// only for compilers other than gcc
#ifndef __GNUC__
#pragma diag_warning 1035 , 177 , 4017
#pragma diag_error 260 
#endif
// --fpmode=fast ON





















