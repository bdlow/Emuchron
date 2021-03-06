//*****************************************************************************
// Filename : 'anim.h'
// Title    : Defines for functional MONOCHRON clocks
//*****************************************************************************

#ifndef ANIM_H
#define ANIM_H

// Define the supported visualizations
#define CHRON_NONE		0
#define CHRON_ANALOG_HMS	1
#define CHRON_ANALOG_HM		2
#define CHRON_DIGITAL_HMS	3
#define CHRON_DIGITAL_HM	4
#define CHRON_MOSQUITO		5
#define CHRON_NERD		6
#define CHRON_PONG		7
#define CHRON_PUZZLE		8
#define CHRON_SLIDER		9
#define CHRON_CASCADE		10
#define CHRON_SPEEDDIAL		11
#define CHRON_SPIDERPLOT	12
#define CHRON_TRAFLIGHT		13
// Set CHRON_MAX_VALUE to the highest value in use above
#define CHRON_MAX_VALUE		13

// Define how visualizations draw themselves
#define DRAW_INIT_NONE		0
#define DRAW_INIT_FULL		1
#define DRAW_INIT_PARTIAL 	2
#define DRAW_CYCLE		3

// Functional alarm switch settings
#define ALARM_SWITCH_NONE	0
#define ALARM_SWITCH_ON		1
#define ALARM_SWITCH_OFF	2

// Structure defining the clock_init/clock_cycle/pressed_button methods
// for a single clock. For a clock the init and cycle methods are required
// whereas the button method is optional.  
typedef struct _clockDriver_t
{
  uint8_t clockId;	// Clock Id
  uint8_t initType;	// Init type for clock (full or partial)
  void (*init)(u08);	// Clock init method
  void (*cycle)(void);	// Clock loop cycle (=update) method
  void (*button)(u08);	// Clock button event handler method (optional)
} clockDriver_t;

// Generic clock wrapper functions
void animAlarmSwitchCheck(void);
u08 animClockButton(u08 pressedButton);
void animClockDraw(u08 type);
u08 animClockNext(void);

// Generic clock utility functions
void animValToStr(u08 value, char valString[]);
void animWelcome(void);
#endif
