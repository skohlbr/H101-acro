/*
The MIT License (MIT)

Copyright (c) 2015 silverx

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

#include <inttypes.h>
#include <math.h>

#include "pid.h"
#include "config.h"
#include "util.h"
#include "drv_pwm.h"
#include "control.h"
#include "defines.h"
#include "drv_time.h"
#include "sixaxis.h"
#include "gestures.h"


extern float throttlehpf(float in);

extern int ledcommand;
extern float rx[4];
extern float gyro[3];
extern int failsafe;
extern char auxchange[AUXNUMBER];
extern char aux[AUXNUMBER];
extern float attitude[3];
extern float looptime;
extern float angleerror[3];
extern float error[PIDNUMBER];
extern float pidoutput[PIDNUMBER];

int onground = 1;
int onground_long = 1;

float thrsum;
float rxcopy[4];

float motormap(float input);
int lastchange;

float yawangle;
float overthrottlefilt;
float underthrottlefilt;
float feedback[3] = {1.0 , 1.0 , 1.0};

#ifdef STOCK_TX_AUTOCENTER
float autocenter[3];
float lastrx[3];
unsigned int consecutive[3];
#endif

extern int pwmdir;

void bridge_sequencer(int dir);
// bridge
int bridge_stage = BRIDGE_WAIT;

int currentdir;

// for 3d throttle
#ifdef THREE_D_THROTTLE
int throttlesafe_3d = 0;
#endif

extern float apid(int x);
extern void imu_calc(void);
extern void savecal(void);



void control(void)
{

	// hi rates
	float ratemulti;
	float ratemultiyaw;
	float maxangle;
	float anglerate;


#ifdef TOGGLE_IN
if ( auxchange[TOGGLE_IN] && !aux[TOGGLE_IN] )
{
   ledcommand = 1;
aux[TOGGLE_OUT]=!aux[TOGGLE_OUT];
}
#endif

#ifndef THREE_D_THROTTLE
	if ( aux[INVERTEDMODE] )
	{
		bridge_sequencer(REVERSE);	// reverse
	}
	else
	{
		bridge_sequencer(FORWARD);	// forward
	}
#endif

	// pwmdir controls hardware directly so we make a copy here
	currentdir = pwmdir;


	if (aux[RATES])
	  {
		  ratemulti = HIRATEMULTI;
		  ratemultiyaw = HIRATEMULTIYAW;
		  maxangle = MAX_ANGLE_HI;
		  anglerate = LEVEL_MAX_RATE_HI * DEGTORAD;
	  }
	else
	  {
		  ratemulti = 1.0f;
		  ratemultiyaw = 1.0f;
		  maxangle = MAX_ANGLE_LO;
		  anglerate = LEVEL_MAX_RATE_LO * DEGTORAD;
	  }


	for (int i = 0; i < 3; i++)
	  {
		#ifdef STOCK_TX_AUTOCENTER
		rxcopy[i] = rx[i] - autocenter[i];
		#else
		rxcopy[i] = rx[i];
		#endif

#ifdef STICKS_DEADBAND
		if ( fabsf( rxcopy[ i ] ) <= STICKS_DEADBAND ) {
			rxcopy[ i ] = 0.0f;
		} else {
			if ( rxcopy[ i ] >= 0 ) {
				rxcopy[ i ] = mapf( rxcopy[ i ], STICKS_DEADBAND, 1, 0, 1 );
			} else {
				rxcopy[ i ] = mapf( rxcopy[ i ], -STICKS_DEADBAND, -1, 0, -1 );
			}
		}
#endif
	  }



if (currentdir == REVERSE)
		{
		#ifndef NATIVE_INVERTED_MODE
		// invert pitch in inverted mode
		rxcopy[PITCH] = - rxcopy[PITCH];
		rxcopy[YAW]	= - rxcopy[YAW];
		#endif
		}

	if (auxchange[HEADLESSMODE])
	  {
		  yawangle = 0;
	  }

	if ((aux[HEADLESSMODE]) )
	  {
		yawangle = yawangle + gyro[2] * looptime;

		while (yawangle < -3.14159265f)
    yawangle += 6.28318531f;

    while (yawangle >  3.14159265f)
    yawangle -= 6.28318531f;

		float temp = rxcopy[0];
		rxcopy[0] = rxcopy[0] * fastcos( yawangle) - rxcopy[1] * fastsin(yawangle );
		rxcopy[1] = rxcopy[1] * fastcos( yawangle) + temp * fastsin(yawangle ) ;
	  }
	else
	  {
		  yawangle = 0;
	  }

// check for acc calibration

	int command = gestures2();

	if (command)
	  {
		  if (command == 3)
		    {
			    gyro_cal();	// for flashing lights
			    acc_cal();
			    savecal();
			    // reset loop time
			    extern unsigned lastlooptime;
			    lastlooptime = gettime();
		    }
		  else
		    {
			    ledcommand = 1;
			    if (command == 2)
			      {
				      aux[CH_AUX1] = 1;

			      }
			    if (command == 1)
			      {
				      aux[CH_AUX1] = 0;
			      }
					if (command == 4)
			      {
				      aux[CH_AUX2] = !aux[CH_AUX2];
			      }
		    }
	  }

	pid_precalc();


	if ( aux[LEVELMODE] )
	  {// level mode

		extern void stick_vector( float , int);
		extern float errorvect[];
		float yawerror[3];
		extern float GEstG[3];

		stick_vector( maxangle , currentdir == REVERSE );

		float yawrate = rxcopy[2] * (float) MAX_RATEYAW * DEGTORAD * ratemultiyaw  * ( 1/2048.0f);

		yawerror[0] = GEstG[1]  * yawrate;
		yawerror[1] = - GEstG[0]  * yawrate;
		yawerror[2] = GEstG[2]  * yawrate;

		if (currentdir == REVERSE)
		{
			yawerror[0] = - yawerror[0];
			yawerror[1] = - yawerror[1];
			yawerror[2] = - yawerror[2];
		}


		for ( int i = 0 ; i <2; i++)
			{
			angleerror[i] = errorvect[i] * RADTODEG;
			error[i] = apid(i) * anglerate + yawerror[i] - gyro[i];
			}

		error[2] = yawerror[2]  - gyro[2];

	  }
	else
	  {			// rate mode

		  error[0] = rxcopy[0] * MAX_RATE * DEGTORAD * ratemulti * feedback[0] - gyro[0];
		  error[1] = rxcopy[1] * MAX_RATE * DEGTORAD * ratemulti * feedback[1] - gyro[1];

		  // reduce angle Iterm towards zero
		  extern float aierror[3];

		  aierror[0] = 0.0f;
			aierror[1] = 0.0f;

		error[2] = rxcopy[2] * MAX_RATEYAW * DEGTORAD * ratemultiyaw * feedback[2] - gyro[2];
	  }



	pid(0);
	pid(1);
	pid(2);

	feedback[0] = 1.0f;
	feedback[1] = 1.0f;
	feedback[2] = 1.0f;

#ifndef THREE_D_THROTTLE
// map throttle so under 10% it is zero
	float throttle = mapf(rx[3], 0, 1, -0.1, 1);
	if (throttle < 0)
		throttle = 0;
	if (throttle > 1.0f)
		throttle = 1.0f;
#endif

#ifdef THREE_D_THROTTLE
	// this changes throttle so under center motor direction is reversed

	// map throttle with zero center
	float throttle = mapf(rx[3], 0, 1, -1, 1);

limitf(&throttle, 1.0);

	if ( throttle > 0 )
	{
		bridge_sequencer(FORWARD);	// forward
	}else
	{
		bridge_sequencer(REVERSE);	// reverse
	}

	if ( !throttlesafe_3d )
	{
		if (throttle > 0)
		{
			throttlesafe_3d = 1;
			ledcommand = 1;
		}
		throttle = 0;
	}

  throttle = fabsf(throttle);

	throttle = mapf (throttle , THREE_D_THROTTLE_DEADZONE , 1, 0 , 1);
	if ( failsafe ) throttle = 0;
#endif	// end 3d throttle remap

#ifdef AIRMODE_HOLD_SWITCH
	if (failsafe || aux[AIRMODE_HOLD_SWITCH] || throttle < 0.001f && !onground_long)
	{
		onground_long = 0;
#else
// turn motors off if throttle is off and pitch / roll sticks are centered
	if (failsafe || (throttle < 0.001f && ( !ENABLESTIX || !onground_long || aux[LEVELMODE] || (fabsf(rx[0]) < (float) ENABLESTIX_TRESHOLD && fabsf(rx[1]) < (float) ENABLESTIX_TRESHOLD && fabsf(rx[2]) < (float) ENABLESTIX_TRESHOLD ))))
	  {			// motors off
#endif

		onground = 1;

		if ( onground_long )
		{
			if ( gettime() - onground_long > ENABLESTIX_TIMEOUT)
			{
				onground_long = 0;
			}
		}


	extern float GEstG[3];
	// check gravity vector to see if inverted
	if ( GEstG[2] < 0 ) aux[CH_AUX3] = 1;
  else aux[CH_AUX3] = 0;


		#ifdef MOTOR_BEEPS
		extern void motorbeep( void);
		motorbeep();
		#endif

		  thrsum = 0;
		  for (int i = 0; i <= 3; i++)
		    {
			    pwm_set(i, 0);
		    }

		  // reset the overthrottle filter
		  lpf(&overthrottlefilt, 0.0f, 0.72f);	// 50hz 1khz sample rate
			lpf(&underthrottlefilt, 0.0f, 0.72f);	// 50hz 1khz sample rate
#ifdef MOTOR_FILTER
		  // reset the motor filter
		  for (int i = 0; i <= 3; i++)
		    {
			    motorfilter(0, i);
		    }
#endif


#ifdef 	THROTTLE_TRANSIENT_COMPENSATION
		  // reset hpf filter;
		  throttlehpf(0);
#endif

#ifdef STOCK_TX_AUTOCENTER
      for( int i = 0 ; i <3;i++)
				{
					if ( rx[i] == lastrx[i] )
						{
						  consecutive[i]++;

						}
					else consecutive[i] = 0;
					lastrx[i] = rx[i];
					if ( consecutive[i] > 1000 && fabsf( rx[i]) < 0.1f )
						{
							autocenter[i] = rx[i];
						}
				}
#endif

// end motors off / failsafe / onground
	  }
	else
	  {
// motors on - normal flight

		onground_long = gettime();

#ifdef 	THROTTLE_TRANSIENT_COMPENSATION
		  throttle += 7.0f * throttlehpf(throttle);
		  if (throttle < 0)
			  throttle = 0;
		  if (throttle > 1.0f)
			  throttle = 1.0f;
#endif


		  // throttle angle compensation
#ifdef AUTO_THROTTLE
		  if (aux[LEVELMODE])
		    {

			   // float autothrottle = fastcos(attitude[0] * DEGTORAD) * fastcos(attitude[1] * DEGTORAD);
				 extern float GEstG[];
				float autothrottle;
				if ( 	GEstG[2] < 0 && currentdir == REVERSE)
					autothrottle = - GEstG[2] * ( 1/2048.0f);

				if ( 	GEstG[2] > 0 && currentdir == FORWARD)
					autothrottle =  GEstG[2] * ( 1/2048.0f);

			    float old_throttle = throttle;
			    if (autothrottle <= 0.5f)
				    autothrottle = 0.5f;
			    throttle = throttle / autothrottle;
			    // limit to 90%
			    if (old_throttle < 0.9f)
				    if (throttle > 0.9f)
					    throttle = 0.9f;

			    if (throttle > 1.0f)
				    throttle = 1.0f;

		    }
#endif

#ifdef LVC_PREVENT_RESET
extern float vbatt;
if (vbatt < (float) LVC_PREVENT_RESET_VOLTAGE) throttle = 0;
#endif
		  onground = 0;
		  float mix[4];
  if ( bridge_stage == BRIDGE_WAIT ) onground = 1;

	if (currentdir == REVERSE)
		{
			// inverted flight
			pidoutput[ROLL] = -pidoutput[ROLL];
			pidoutput[PITCH] = -pidoutput[PITCH];
			pidoutput[YAW] = -pidoutput[YAW];
		}



#ifdef INVERT_YAW_PID
		  pidoutput[2] = -pidoutput[2];
#endif

		  mix[MOTOR_FR] = throttle - pidoutput[0] - pidoutput[1] + pidoutput[2];	// FR
		  mix[MOTOR_FL] = throttle + pidoutput[0] - pidoutput[1] - pidoutput[2];	// FL
		  mix[MOTOR_BR] = throttle - pidoutput[0] + pidoutput[1] - pidoutput[2];	// BR
		  mix[MOTOR_BL] = throttle + pidoutput[0] + pidoutput[1] + pidoutput[2];	// BL


#ifdef INVERT_YAW_PID
// we invert again cause it's used by the pid internally (for limit)
		  pidoutput[2] = -pidoutput[2];
#endif
// we invert again cause it's used by the pid internally (for limit)
		if (currentdir == REVERSE)
		{
			// inverted flight
			pidoutput[ROLL] = -pidoutput[ROLL];
			pidoutput[PITCH] = -pidoutput[PITCH];
			pidoutput[YAW] = -pidoutput[YAW];
		}


#if ( defined MIX_LOWER_THROTTLE || defined MIX_INCREASE_THROTTLE)

//#define MIX_INCREASE_THROTTLE

// options for mix throttle lowering if enabled
// 0 - 100 range ( 100 = full reduction / 0 = no reduction )
#ifndef MIX_THROTTLE_REDUCTION_PERCENT
#define MIX_THROTTLE_REDUCTION_PERCENT 100
#endif
// lpf (exponential) shape if on, othewise linear
//#define MIX_THROTTLE_FILTER_LPF

// limit reduction and increase to this amount ( 0.0 - 1.0)
// 0.0 = no action
// 0.5 = reduce up to 1/2 throttle
//1.0 = reduce all the way to zero
#ifndef MIX_THROTTLE_REDUCTION_MAX
#define MIX_THROTTLE_REDUCTION_MAX 0.5
#endif

#ifndef MIX_THROTTLE_INCREASE_MAX
#define MIX_THROTTLE_INCREASE_MAX 0.2
#endif

#ifndef MIX_MOTOR_MAX
#define MIX_MOTOR_MAX 1.0f
#endif


		  float overthrottle = 0;
			float underthrottle = 0.001f;

		  for (int i = 0; i < 4; i++)
		    {
			    if (mix[i] > overthrottle)
				    overthrottle = mix[i];               
					if (mix[i] < underthrottle)
						underthrottle = mix[i];
		    }

		  overthrottle -= MIX_MOTOR_MAX ;

		  if (overthrottle > (float)MIX_THROTTLE_REDUCTION_MAX)
			  overthrottle = (float)MIX_THROTTLE_REDUCTION_MAX;

#ifdef MIX_THROTTLE_FILTER_LPF
		  if (overthrottle > overthrottlefilt)
			  lpf(&overthrottlefilt, overthrottle, 0.82);	// 20hz 1khz sample rate
		  else
			  lpf(&overthrottlefilt, overthrottle, 0.72);	// 50hz 1khz sample rate
#else
		  if (overthrottle > overthrottlefilt)
			  overthrottlefilt += 0.005f;
		  else
			  overthrottlefilt -= 0.01f;
#endif

			// over
		  if (overthrottlefilt > (float)MIX_THROTTLE_REDUCTION_MAX)
			  overthrottlefilt = (float)MIX_THROTTLE_REDUCTION_MAX;
		  if (overthrottlefilt < -0.1f)
			  overthrottlefilt = -0.1;

		  overthrottle = overthrottlefilt;

		  if (overthrottle < 0.0f)
			  overthrottle = -0.0001f;

			// reduce by a percentage only, so we get an inbetween performance
			overthrottle *= ((float)MIX_THROTTLE_REDUCTION_PERCENT / 100.0f);

#ifndef MIX_LOWER_THROTTLE
	// disable if not enabled
	overthrottle = -0.0001f;
#endif


#ifdef MIX_INCREASE_THROTTLE
// under

		  if (underthrottle < -(float)MIX_THROTTLE_INCREASE_MAX)
			  underthrottle = -(float)MIX_THROTTLE_INCREASE_MAX;

#ifdef MIX_THROTTLE_FILTER_LPF
		  if (underthrottle < underthrottlefilt)
			  lpf(&underthrottlefilt, underthrottle, 0.82);	// 20hz 1khz sample rate
		  else
			  lpf(&underthrottlefilt, underthrottle, 0.72);	// 50hz 1khz sample rate
#else
		  if (underthrottle < underthrottlefilt)
			  underthrottlefilt -= 0.005f;
		  else
			  underthrottlefilt += 0.01f;
#endif
// under
			if (underthrottlefilt < - (float)MIX_THROTTLE_REDUCTION_MAX)
			  underthrottlefilt = - (float)MIX_THROTTLE_REDUCTION_MAX;
		  if (underthrottlefilt > 0.1f)
			  underthrottlefilt = 0.1;

			underthrottle = underthrottlefilt;

			if (underthrottle > 0.0f)
			  underthrottle = 0.0001f;

			underthrottle *= ((float)MIX_THROTTLE_REDUCTION_PERCENT / 100.0f);

#else
    underthrottle = 0.001f;        
#endif


		  if (overthrottle > 0 || underthrottle < 0 )
		    {		// exceeding max motor thrust
					float temp = overthrottle + underthrottle;
			    for (int i = 0; i < 4; i++)
			      {
				      mix[i] -= temp;
			      }
		    }
// end MIX_LOWER_THROTTLE
#endif



#ifdef RATELIMITER_ENABLE
// rate limiter if motors limit exceeded

// 50mS lpf to remove vibrations
#define RATELIMITER_FILT_TIME_US 50e3
// reduce rates by half maximum 0.0 - 1.0 higher = more action
#define RATELIMITER_MULTIPLIER_LIMIT 0.5f
// at 1mS loop time 0.01 step takes 50mS to reach 0.5 reduction
#define RATELIMITER_P_TERM 1.0f

// variable for low pass filter
static float feedbackfilt =  1.0f ;
// limit exceeded by this amount
float excess = 0;
// motor thrust used by controls ( for one motor )
float range = (fabsf(pidoutput[0]) + fabsf(pidoutput[1]) +fabsf(pidoutput[2]));
{
		float overthrottle =  0.0f;
		float underthrottle = 0.01f;

				for (int i = 0; i < 4; i++)
					{
						if (mix[i] > overthrottle)
							overthrottle = mix[i];
						if (mix[i] < underthrottle)
							underthrottle = mix[i];
					}

				overthrottle -= 1.0f ;

	if ( overthrottle > 0 )
		excess = overthrottle;
	if (underthrottle < 0)
		excess += underthrottle;
}


float excess_ratio = RATELIMITER_P_TERM * (fabs(excess)) / range ;


if ( excess_ratio > (float) RATELIMITER_MULTIPLIER_LIMIT ) excess_ratio = (float) RATELIMITER_MULTIPLIER_LIMIT;
if ( excess_ratio < 0.0f ) excess_ratio = 0.0;

	lpf( &feedbackfilt , excess_ratio , FILTERCALC( 1e3, RATELIMITER_FILT_TIME_US ) );

	for ( int i = 0 ; i < 3 ; i++)
		{
			feedback[i] =  1.0f - feedbackfilt;
		}
// end RATELIMITER_ENABLE
#endif


#ifdef MOTOR_FILTER

		  for (int i = 0; i < 4; i++)
		    {
			    mix[i] = motorfilter(mix[i], i);
		    }
#endif




#ifdef CLIP_FF
		  float clip_ff(float motorin, int number);

		  for (int i = 0; i < 4; i++)
		    {
			    mix[i] = clip_ff(mix[i], i);
		    }
#endif

		  for (int i = 0; i < 4; i++)
		    {
			    float test = motormap(mix[i]);
					#ifdef MOTORS_TO_THROTTLE
					test = throttle;
					// flash leds in valid throttle range
					ledcommand = 1;
					// Spin all motors if the roll/pitch stick is centered.
					// Otherwise select the motors to test by deflecting the roll/pitch stick.
					if ( i == MOTOR_FL && ( rx[ROLL] > 0.5f || rx[PITCH] < -0.5f ) ) { test = 0; }
					if ( i == MOTOR_BL && ( rx[ROLL] > 0.5f || rx[PITCH] > 0.5f ) ) { test = 0; }
					if ( i == MOTOR_FR && ( rx[ROLL] < -0.5f || rx[PITCH] < -0.5f ) ) { test = 0; }
					if ( i == MOTOR_BR && ( rx[ROLL] < -0.5f || rx[PITCH] > 0.5f ) ) { test = 0; }
					// for battery estimation
					mix[i] = test;
					#warning "MOTORS TEST MODE"
					#endif

					#ifdef MOTOR_MIN_ENABLE
					if (test < (float) MOTOR_MIN_VALUE)
					{
						test = (float) MOTOR_MIN_VALUE;
					}
					#endif

					#ifdef MOTOR_MAX_ENABLE
					if (test > (float) MOTOR_MAX_VALUE)
					{
						test = (float) MOTOR_MAX_VALUE;
					}
					#endif

					#ifndef NOMOTORS
					//normal mode
					if (bridge_stage == BRIDGE_WAIT) {
						pwm_set( i , 0 );
					} else {
						pwm_set( i , test );
					}
					#else
					#warning "NO MOTORS"
					#endif
		    }


		  thrsum = 0;
		  for (int i = 0; i < 4; i++)
		    {
			    if (mix[i] < 0)
				    mix[i] = 0;
			    if (mix[i] > 1)
				    mix[i] = 1;
			    thrsum += mix[i];
		    }
		  thrsum = thrsum / 4;

	  }			// end motors on


	imu_calc();

}

/////////////////////////////
/////////////////////////////





#ifdef MOTOR_CURVE_6MM_490HZ
// the old map for 490Hz
float motormap(float input)
{
	// this is a thrust to pwm function
	//  float 0 to 1 input and output
	// output can go negative slightly
	// measured eachine motors and prop, stock battery
	// a*x^2 + b*x + c
	// a = 0.262 , b = 0.771 , c = -0.0258

	if (input > 1)
		input = 1;
	if (input < 0)
		input = 0;

	input = input * input * 0.262f + input * (0.771f);
	input += -0.0258f;

	return input;
}
#endif


#ifdef MOTOR_CURVE_6MM_H101_490HZ
float motormap( float input)
{

	// H101 thrust curve for normal thrust direction
	// a*x^2 + b*x + c

if (input > 1.0f) input = 1.0f;
if (input < 0) input = 0;

input = input*input*0.277f  + input*(0.715f);
input += 0.0102f;

return input;
}
#endif

// 8k pwm is where the motor thrust is relatively linear for the H8 6mm motors
// it's due to the motor inductance cancelling the nonlinearities.
#ifdef MOTOR_CURVE_NONE
float motormap(float input)
{
	return input;
}
#endif


#ifdef MOTOR_CURVE_85MM_8KHZ
// Hubsan 8.5mm 8khz pwm motor map
// new curve
float motormap(float input)
{
//      Hubsan 8.5mm motors and props

	if (input > 1)
		input = 1;
	if (input < 0)
		input = 0;

	input = input * input * 0.683f + input * (0.262f);
	input += 0.06f;

	return input;
}
#endif



#ifdef MOTOR_CURVE_85MM_8KHZ_OLD
// Hubsan 8.5mm 8khz pwm motor map
float motormap(float input)
{
//      Hubsan 8.5mm motors and props

	if (input > 1)
		input = 1;
	if (input < 0)
		input = 0;

	input = input * input * 0.789f + input * (0.172f);
	input += 0.04f;

	return input;
}
#endif


#ifdef MOTOR_CURVE_85MM_32KHZ
// Hubsan 8.5mm 8khz pwm motor map
float motormap(float input)
{
//      Hubsan 8.5mm motors and props

	if (input > 1)
		input = 1;
	if (input < 0)
		input = 0;

	input = input * input * 0.197f + input * (0.74f);
	input += 0.067f;

	return input;
}
#endif

#ifdef CUSTOM_MOTOR_CURVE

float motormap(float in)
{
    
float exp = CUSTOM_MOTOR_CURVE;
	if ( exp > 1 ) exp = 1;
	if ( exp < -1 ) exp = -1;
 
if (in > 1.0f) in = 1.0f;
if (in < 0) in = 0;
    
	float ans = in * (in*in * exp +  ( 1 - exp ));

if (ans > 1.0f) ans = 1.0f;
if (ans < 0) ans = 0;
    
	return ans;
}
#endif


float hann_lastsample[4];
float hann_lastsample2[4];

// hanning 3 sample filter
float motorfilter(float motorin, int number)
{
	float ans = motorin * 0.25f + hann_lastsample[number] * 0.5f + hann_lastsample2[number] * 0.25f;

	hann_lastsample2[number] = hann_lastsample[number];
	hann_lastsample[number] = motorin;

	return ans;
}


float clip_feedforward[4];
// clip feedforward adds the amount of thrust exceeding 1.0 ( max)
// to the next iteration(s) of the loop
// so samples 0.5 , 1.5 , 0.4 would transform into 0.5 , 1.0 , 0.9;

float clip_ff(float motorin, int number)
{

	if (motorin > 1.0f)
	  {
		  clip_feedforward[number] += (motorin - 1.0f);
		  //cap feedforward to prevent windup
		  if (clip_feedforward[number] > .5f)
			  clip_feedforward[number] = .5f;
	  }
	else if (clip_feedforward[number] > 0)
	  {
		  float difference = 1.0f - motorin;
		  motorin = motorin + clip_feedforward[number];
		  if (motorin > 1.0f)
		    {
			    clip_feedforward[number] -= difference;
			    if (clip_feedforward[number] < 0)
				    clip_feedforward[number] = 0;
		    }
		  else
			  clip_feedforward[number] = 0;

	  }
	return motorin;
}


unsigned long bridgetime = 0;

// the bridge sequencer creates a pause between motor direction changes
// that way the motors do not try to instantly go in reverse and have time to slow down

void bridge_sequencer(int dir)
{

	if (dir == REVERSE && bridge_stage != BRIDGE_REVERSE)
	  {

		  if (bridge_stage == BRIDGE_FORWARD)
		    {
			    bridge_stage = BRIDGE_WAIT;
			    bridgetime = gettime();
			    pwm_dir(FREE);
			    extern float ierror[3];
			    ierror[0] = 0.0; ierror[1] = 0.0; ierror[2] = 0.0;
		    }
		  if (bridge_stage == BRIDGE_WAIT)
		    {
			    if (gettime() - bridgetime > BRIDGE_TIMEOUT)
			      {
				      // timeout has elapsed
				      bridge_stage = BRIDGE_REVERSE;
				      pwm_dir(REVERSE);

			      }

		    }

	  }
	if (dir == FORWARD && bridge_stage != BRIDGE_FORWARD)
	  {

		  if (bridge_stage == BRIDGE_REVERSE)
		    {
			    bridge_stage = BRIDGE_WAIT;
			    bridgetime = gettime();
			    pwm_dir(FREE);
			    extern float ierror[3];
			    ierror[0] = 0.0; ierror[1] = 0.0; ierror[2] = 0.0;
		    }
		  if (bridge_stage == BRIDGE_WAIT)
		    {
			    if (gettime() - bridgetime > BRIDGE_TIMEOUT)
			      {
				      // timeout has elapsed
				      bridge_stage = BRIDGE_FORWARD;
				      pwm_dir(FORWARD);

			      }

		    }

	  }




}
