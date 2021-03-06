/* controller_b.c */
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "stdint.h"
#include "kernel.h"
#include "kernel_id.h"
#include "ecrobot_interface.h"
#include "blue.h"


DeclareTask(MainTask);


/* nxtOSEK hook to be invoked from an ISR in category 2 */
void user_1ms_isr_type2(void) {
    /* do nothing */
    //SignalCounter(SysTimerCnt);
}



void ecrobot_device_initialize(void) {
    ecrobot_init_bt_slave("1234");
    nxt_motor_set_speed(NXT_PORT_A, 0, 1);
    nxt_motor_set_speed(NXT_PORT_B, 0, 1);
    nxt_motor_set_speed(NXT_PORT_C, 0, 1);
    nxt_motor_set_count(NXT_PORT_A, 0);
    nxt_motor_set_count(NXT_PORT_B, 0);
    nxt_motor_set_count(NXT_PORT_C, 0);
}

void ecrobot_device_terminate(void) {
    ecrobot_term_bt_connection();
    nxt_motor_set_speed(NXT_PORT_A, 0, 0);
    nxt_motor_set_speed(NXT_PORT_B, 0, 0);
    nxt_motor_set_speed(NXT_PORT_C, 0, 0);
}


/* Digital filter designed by mkfilter/mkshape/gencode   A.J. Fisher
   Command line: /www/usr/fisher/helpers/mkfilter -Bu -Lp -o 1 -a 5.0000000000e-03 0.0000000000e+00 -l */


/*
int i = 0;
int vels[] = {20,40,50, 60, 70, 80, 90, 100};
*/
void init_motors_counters(void){
    nxt_motor_set_count(NXT_PORT_A, 0);
    nxt_motor_set_count(NXT_PORT_B, 0);
    nxt_motor_set_count(NXT_PORT_C, 0);
}


double raw_speed_prev=0;
double filt_speed_prev = 0;

int motor_counter_prev = 0;

void init_tachometer_globalvariables(void){
   raw_speed_prev=0;
   filt_speed_prev = 0;
   motor_counter_prev = 0;
}

//y[n]=(1*x[n-1])+(1*x[n-0])+(0.9690674172*y[n-1]);
double tachometer_getspeed(void){
  int motor_counter = 0;
  double filt_speed = 0;
  double raw_speed = 0;
  double gain = 64.6567;
  
  motor_counter = nxt_motor_get_count(NXT_PORT_A);
  
  raw_speed = (motor_counter-motor_counter_prev)/0.005;
  
  raw_speed = raw_speed*(3.1415927/180); // conversion on rad/sec
  
  raw_speed=raw_speed/gain;
  
  filt_speed=(1*raw_speed_prev)+(1*raw_speed)+(0.9690674172*filt_speed_prev);
  

  raw_speed_prev = raw_speed;
  filt_speed_prev = filt_speed;  
  motor_counter_prev = motor_counter;
   
  return filt_speed;  
}

double REFERENCE_SPEED = 0;
double power_prev = 0;
double power_prev2 = 0;
double error_prev = 0;
double error_prev2 = 0;

void init_controller_globalvariables(void){
     REFERENCE_SPEED = 0;
     power_prev = 0;
     power_prev2 = 0;
     error_prev = 0;
     error_prev2 = 0;
}

int controller_getpower(double speed){
  double error = 0;
  double power=0;
  int poweri = 0;
  
  
  
  //C=((s+10)^2)/(s*(s+80));
  double CE  = 32.394271;
  double CE1 = 61.628125;
  double CE2 = 29.310937;
  double CY1 = 1.6666667;
  double CY2 = 0.6666667;
  error = REFERENCE_SPEED - speed;
  
  
  power = (CY1*power_prev)-(CY2*power_prev2)+(CE*error)-(CE1*error_prev)+(CE2*error_prev2);
  
  power_prev2 = power_prev;
  power_prev = power;
  error_prev2 = error_prev;
  error_prev = error;
    
  poweri = (int)power;
  
  if(poweri>100)
    poweri=100;
  if(poweri<-100)
    poweri = -100;
  
  return poweri;
}
/*
int i=0;
int pws[]={20,30,60,70,80,90};
*/
#define WAIT_REFERENCE 0
#define COLLECT_DATA 1
#define SEND_DATA 2
#define MAX_BTSAMPLES 2000
uint8_t main_task_state = WAIT_REFERENCE;

SMPACKET data_array[MAX_BTSAMPLES];
TASK(MainTask) {	
    
    int time_passed = 0;
    int time_passed2 = 0;        
    int speedi = 0;       
    double speed = 0;   
    int power = 0;
    MSPACKET packet;
    int packet_counter = 0;
    
    while(1){          
      
      switch(main_task_state){
	case WAIT_REFERENCE:
		if(time_passed2>=1000){
		  display_clear(0);    
		  display_goto_xy(0, 0);
		  display_string("Waiting reference speed...");		     
		  display_update();
		  time_passed2 = 0;
		}else
		  time_passed2+=5;
		
		memset((void*) &packet, 0x00, sizeof (MSPACKET));                
		if (ecrobot_read_bt(&packet, 0, sizeof (MSPACKET)) == sizeof (MSPACKET)) {
		   // counters for print
		    time_passed2 = 0;
		    time_passed = 0;		    
		    //reinitialization tachometer and controller variables
		    init_tachometer_globalvariables();
		    init_controller_globalvariables();
		    init_motors_counters();
		    
		    //just to be sure motor doesn't move
		    nxt_motor_set_speed(NXT_PORT_A,0,1); 
		    // clean buffer
		    memset((void*) data_array, 0x00, sizeof (SMPACKET)*MAX_BTSAMPLES);
		    packet_counter = 0;
		    // set reference speed obtained from master
		    REFERENCE_SPEED = packet.reference_speed;
		    speedi = (int)REFERENCE_SPEED;		    
		    display_clear(0);    
		    display_goto_xy(0, 0);
		    display_string("speed: ");
		    display_int(speedi, 3);   
		    display_update();
		    main_task_state = COLLECT_DATA;
		    systick_wait_ms(1000);
		}
		  
	  break;
	case COLLECT_DATA:
	    if(packet_counter<MAX_BTSAMPLES){
		//estimate speed		
		speed = tachometer_getspeed();      		
		//estimate controlled power
		power = controller_getpower(speed);    
		//set new power to the motor
		nxt_motor_set_speed(NXT_PORT_A,power,1); 
		//collect data to send on bluetooth channel
		data_array[packet_counter].system_time = systick_get_ms();
		data_array[packet_counter].speed = (float)speed;//
		//*************
		if(time_passed>=1000){
		  //convert to int... just to print it on brick screen
		  speedi = (int)speed;
		  display_clear(0);    
		  display_goto_xy(0, 0);
		  display_string("speed: ");
		  display_int(speedi, 3);      
		  display_goto_xy(0, 1);
		  display_string("new power: ");
		  display_int(power, 3);       
		  display_update();
	      
		  time_passed = 0;
		  /* 
		  nxt_motor_set_speed(NXT_PORT_A,pws[i],1);                 
		  i = (i+1)%6;	  	  
		  */
		}
		packet_counter++;
		// time counter needed to print on screen 1 time at sec
		time_passed = time_passed+5;
	  }else{
	    //after data collecting stop motor
	    nxt_motor_set_speed(NXT_PORT_A,0,1); 
	    
	    main_task_state = SEND_DATA;	    
	  }
	  break;
	case SEND_DATA:
	    
	    display_clear(0);    
	    display_goto_xy(0, 0);
	    display_string("Sending data...");		     
	    display_update();
		  
		
	    packet_counter = 0;
            while(packet_counter<MAX_BTSAMPLES){
	      ecrobot_send_bt(&data_array[packet_counter], 0, sizeof (SMPACKET));		   
	      packet_counter++;	
	      systick_wait_ms(10);
	     }
	     main_task_state = WAIT_REFERENCE;
	     
	  break;
	
      }                        
        systick_wait_ms(5);
	
    }
              	           	
}




