//*****************************************************************************
// Filename : 'config.c'
// Title    : Configuration menu handling
//*****************************************************************************

#ifndef EMULIN
#include <avr/io.h>      // this contains all the IO port definitions
#include <avr/interrupt.h>
#include <util/delay.h>
#include <avr/pgmspace.h>
#include <avr/eeprom.h>
#include "util.h"
#else
#include "emulator/stub.h"
#include "emulator/stubrefs.h"
#endif
#include "ratt.h"
#include "ks0108.h"
#include "glcd.h"
#include "config.h"

// How many seconds to wait before turning off menus
#define INACTIVITYTIMEOUT 10 

// Instructions
#define ACTION_ADVANCE	0
#define ACTION_EXIT	1
#define ACTION_CHANGE	2
#define ACTION_SET	3
#define ACTION_SAVE	4
#define ACTION_NONE	5

// External data
extern volatile uint8_t time_s, time_m, time_h;
extern volatile uint8_t date_m, date_d, date_y;
extern volatile uint8_t mcAlarmH, mcAlarmM;
extern volatile uint8_t alarmSelect;
extern volatile uint8_t just_pressed, pressed;
extern volatile uint8_t mcBgColor, mcFgColor;
extern volatile uint8_t displaymode;

#ifdef BACKLIGHT_AUTO
volatile uint8_t backlightauto; // auto brightness mode on/off
#endif

// This variable keeps track of whether we have not pressed any
// buttons in a few seconds, and turns off the menu display
volatile uint8_t timeoutcounter = 0;

// The screenmutex attempts to prevent printing the time in the
// configuration menu when other display activities are going on.
// It does not always work due to race conditions.
volatile uint8_t screenmutex = 0;

// A shortcut to the active alarm clock
volatile uint8_t almPageDataId;

// The months in a year
char *months[12] =
{
  "Jan", "Feb", "Mar", "Apr", "May", "Jun",
  "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

// The days in a week
char *days[7] =
{
  "Sun ", "Mon ", "Tue ", "Wed ", "Thu ", "Fri ", "Sat "
};

// Several fixed substring instructions
char instrPlusPrefix[] = "Press + to change ";
char instrSetPrefix[]  = "Press SET to set ";

// Several fixed complete instructions
char instrAdvance[]    = "Press MENU to advance";
char instrExit[]       = "Press MENU to exit   ";
char instrChange[]     = "Press + to change    ";
char instrSet[]        = "Press SET to set     ";
char instrSave[]       = "Press SET to save    ";

// Local function prototypes
void menu_alarm(void);
void display_alarm_menu(void);
void display_main_menu(void);
void enter_alarm_menu(void);
void print_arrow(u08 x, u08 y, u08 l, u08 color);
void print_date(u08 month, u08 day, u08 year, u08 mode);
void print_display_setting(u08 color);
#ifdef BACKLIGHT_ADJUST
void print_backlight_setting(u08 color);
#endif
void print_instructions(char *line1, char *line2);
void print_instructions2(char *line1a, char *line1b, char *line2a,
  char *line2b, u08 color);
void set_alarm(void);
void set_backlight(void);
void set_date(void);
void set_display(void);
void set_time(void);

//
// Function: menu_alarm
//
// This is the state-event machine for the alarm configuration page
//
void menu_alarm(void)
{
  switch (displaymode)
  {
  case SET_ALARM:
    // Alarm -> Set Alarm 1
    DEBUGP("Set alarm 1");
    displaymode = SET_ALARM_1;
    almPageDataId = 0;
    set_alarm();
    break;
  case SET_ALARM_1:
    // Alarm 1 -> Set Alarm 2
    DEBUGP("Set alarm 2");
    displaymode = SET_ALARM_2;
    almPageDataId = 1;
    set_alarm();
    break;
  case SET_ALARM_2:
    // Alarm 2 -> Set Alarm 3
    DEBUGP("Set alarm 3");
    displaymode = SET_ALARM_3;
    almPageDataId = 2;
    set_alarm();
    break;
  case SET_ALARM_3:
    // Alarm 3 -> Set Alarm 4
    DEBUGP("Set alarm 4");
    displaymode = SET_ALARM_4;
    almPageDataId = 3;
    set_alarm();
    break;
  case SET_ALARM_4:
    // Alarm 4 -> Set Alarm Id
    DEBUGP("Set alarm Id");
    displaymode = SET_ALARM_ID;
    almPageDataId = 4;
    set_alarm();
    break;
  default:
    // Switch back to main menu
    DEBUGP("Return to config menu");
    displaymode = SET_ALARM;
  }
}

//
// Function: menu_main
//
// This is the state-event machine for the main configuration page
//
void menu_main(void)
{
  // If we enter menu mode clear the screen
  if (displaymode == SHOW_TIME)
  {
    screenmutex++;
    glcdClearScreen(mcBgColor);
    screenmutex--;
  }

  switch (displaymode)
  {
  case SHOW_TIME:
    // Clock -> Set Alarm
    DEBUGP("Set alarm");
    displaymode = SET_ALARM;
    enter_alarm_menu();
    break;
  case SET_ALARM:
    // Set Alarm -> Set Time
    DEBUGP("Set time");
    displaymode = SET_TIME;
    set_time();
    break;
  case SET_TIME:
    // Set Time -> Set Date
    DEBUGP("Set date");
    displaymode = SET_DATE;
    set_date();
    break;
  case SET_DATE:
    // Set Date -> Set Display
    DEBUGP("Set display");
    displaymode = SET_DISPLAY;
    set_display();
    break;
#ifdef BACKLIGHT_ADJUST
  case SET_DISPLAY:
    // Set Display -> Set Brightness
    DEBUGP("Set brightness");
    displaymode = SET_BRIGHTNESS;
    set_backlight();
    break;
#endif
  default:
    // Switch back to Clock
    DEBUGP("Exit config menu");
    displaymode = SHOW_TIME;
  }
}

//
// Function: display_alarm_menu
//
// Display the alarm menu page
//
void display_alarm_menu(void)
{
  volatile u08 alarmH, alarmM;
  u08 i;

  DEBUGP("Display alarm menu");
  
  screenmutex++;
  glcdSetAddress(0, 0);
  glcdPutStr("Alarm Setup Menu", mcFgColor);

  // Print the four alarm times
  for (i = 0; i < 4; i++)
  {
    glcdSetAddress(MENU_INDENT, 1 + i);
    glcdPutStr("Alarm ", mcFgColor);
    glcdPrintNumber(i + 1, mcFgColor);
    glcdPutStr(":      ", mcFgColor);
    alarmTimeGet(i, &alarmH, &alarmM);
    glcdPrintNumber(alarmH, mcFgColor);
    glcdWriteChar(':', mcFgColor);
    glcdPrintNumber(alarmM, mcFgColor);
  }

  // Print the selected alarm
  glcdSetAddress(MENU_INDENT, 5);
  glcdPutStr("Select Alarm:     ", mcFgColor);
  glcdPrintNumber(alarmSelect+1, mcFgColor);

  // Print the button functions
  if (displaymode != SET_ALARM_ID)
    print_instructions(instrAdvance, instrSet);
  else
    print_instructions(instrExit, instrSet);

  // Clear the arrow area
  glcdFillRectangle2(0, 8, 7, 40, ALIGN_TOP, FILL_FULL, mcBgColor);
  screenmutex--;
}

//
// Function: display_main_menu
//
// Display the main menu page
//
void display_main_menu(void)
{
  DEBUGP("Display menu");

  screenmutex++;
  glcdSetAddress(0, 0);
  glcdPutStr("Configuration Menu   ", mcFgColor);
  glcdFillRectangle2(126, 0, 2, 8, ALIGN_TOP, FILL_FULL, mcBgColor);

  glcdSetAddress(MENU_INDENT, 1);
  glcdPutStr("Alarm:         Setup", mcFgColor);
  
  glcdSetAddress(MENU_INDENT, 2);
  glcdPutStr("Time:       ", mcFgColor);
  glcdPrintNumber(time_h, mcFgColor);
  glcdWriteChar(':', mcFgColor);
  glcdPrintNumber(time_m, mcFgColor);
  glcdWriteChar(':', mcFgColor);
  glcdPrintNumber(time_s, mcFgColor);
  
  print_date(date_m, date_d, date_y, SET_DATE);
  print_display_setting(mcFgColor);

#ifdef BACKLIGHT_ADJUST
  print_backlight_setting(mcFgColor);
#endif

  print_instructions(instrAdvance, instrSet);
  glcdFillRectangle2(126, 48, 2, 16, ALIGN_TOP, FILL_FULL, mcBgColor);

  // Clear the arrow area
  glcdFillRectangle2(0, 8, 8, 40, ALIGN_TOP, FILL_FULL, mcBgColor);
  screenmutex--;
}

//
// Function: enter_alarm_menu
//
// Enter the alarm setup configuration page
//
void enter_alarm_menu(void)
{
  uint8_t mode = SET_ALARM;

  display_main_menu();
  screenmutex++;
  // Put a small arrow next to 'Alarm'
  print_arrow(0, 11, MENU_INDENT - 1, mcFgColor);
  screenmutex--;
  timeoutcounter = INACTIVITYTIMEOUT;  

  while (1)
  {
#ifdef EMULIN
    stubGetEvent();
#endif
    if (just_pressed & BTTN_MENU)
    {
      // Mode change
      return;
    }
    if (just_pressed || pressed)
    {
      // Button pressed so reset timoutcounter
      timeoutcounter = INACTIVITYTIMEOUT;  
    }
    else if (!timeoutcounter)
    {
      // Timed out!
      displaymode = SHOW_TIME;     
      return;
    }
    if (just_pressed & BTTN_SET)
    {
      just_pressed = 0;
      screenmutex++;
      if (mode == SET_ALARM)
      {
        DEBUG(putstring_nl("Go to alarm setup"));
        glcdClearScreen(mcBgColor);
        do
        {
          menu_alarm();
        }
        while (displaymode != SET_ALARM && displaymode != SHOW_TIME);
        glcdClearScreen(mcBgColor);
      }
      if (displaymode == SET_ALARM)
        just_pressed = just_pressed | BTTN_MENU;
      screenmutex--;
      return;
    }
    if ((just_pressed & BTTN_PLUS) || (pressed & BTTN_PLUS))
    {
      just_pressed = 0;
    }
  }
}

//
// Function: print_arrow
//
// Print an arrow in front of a menu item
//
void print_arrow(u08 x, u08 y, u08 l, u08 color)
{
  glcdFillRectangle(x, y, l, 1, color);
  glcdDot(x + l - 2, y - 1, color);
  glcdDot(x + l - 2, y + 1, color);
  glcdDot(x + l - 3, y - 2, color);
  glcdDot(x + l - 3, y + 2, color);
}

//
// Function: print_date
//
// Print the date (dow+mon+day+year) with optional highlighted item
//
void print_date(u08 month, u08 day, u08 year, u08 mode)
{
  glcdSetAddress(MENU_INDENT, 3);
  glcdPutStr("Date:", mcFgColor);
  glcdPutStr(days[dotw(month, day, year)], mcFgColor);
#ifdef DATE_MONTHDAY
  glcdPutStr(months[month - 1], (mode == SET_MONTH) ? mcBgColor : mcFgColor);
  glcdWriteChar(' ', mcFgColor);
  glcdPrintNumber(day, (mode == SET_DAY) ? mcBgColor : mcFgColor);
  glcdWriteChar(',', mcFgColor);
#else
  glcdPrintNumber(day, (mode == SET_DAY) ? mcBgColor : mcFgColor);
  glcdWriteChar(' ', mcFgColor);
  glcdPutStr(months[month - 1], (mode == SET_MONTH) ? mcBgColor : mcFgColor);
  glcdWriteChar(' ', mcFgColor);
#endif  
  glcdPrintNumber(20, (mode == SET_YEAR) ? mcBgColor : mcFgColor);
  glcdPrintNumber(year, (mode == SET_YEAR) ? mcBgColor : mcFgColor);
}

//
// Function: print_display_setting
//
// Print the display setting
//
void print_display_setting(u08 color)
{
  glcdSetAddress(MENU_INDENT, 4);
  glcdPutStr("Display:     ", mcFgColor);
  if (mcBgColor == OFF)
  {
    glcdPutStr(" ", mcFgColor);
    glcdPutStr("Normal", color);
  }
  else
  {
    glcdPutStr("Inverse", color);
  }
}

#ifdef BACKLIGHT_ADJUST
//
// Function: print_backlight_setting
//
// Print the backlight setting ('Auto' or value)
//
void print_backlight_setting(u08 color)
{
  glcdSetAddress(MENU_INDENT, 5);
#ifdef BACKLIGHT_AUTO
  if (backlightauto)
  {
    glcdPutStr("Backlight:      Auto", mcFgColor);
  }
  else
#endif
  {
    glcdPutStr("Backlight:        ", mcFgColor);
    glcdPrintNumber(OCR2B >> OCR2B_BITSHIFT, mcFgColor);
  }
}
#endif


//
// Function: print_instructions
//
// Print instructions at bottom of screen
//
void print_instructions(char *line1, char *line2)
{
  glcdSetAddress(0, 6);
  glcdPutStr(line1, mcFgColor);
  if (line2 != 0)
  {
    glcdSetAddress(0, 7);
    glcdPutStr(line2, mcFgColor);
  }
}

//
// Function: print_instructions2
//
// Print instructions at bottom of screen
//
void print_instructions2(char *line1a, char *line1b, char *line2a,
  char *line2b, u08 color)
{
  glcdSetAddress(0, 6);
  glcdPutStr(line1a, color);
  glcdPutStr(line1b, color);
  glcdSetAddress(0, 7);
  glcdPutStr(line2a, color);
  glcdPutStr(line2b, color);
}

//
// Function: set_alarm
//
// Set an alarm time and alarm selector by processing button presses
//
void set_alarm(void)
{
  uint8_t mode = SET_ALARM;
  uint8_t newAlarmSelect = alarmSelect;
  volatile uint8_t newHour, newMin;

  display_alarm_menu();
  screenmutex++;
  // Put a small arrow next to proper line
  print_arrow(0, 11 + 8 * almPageDataId, MENU_INDENT - 1, mcFgColor);
  screenmutex--;
  timeoutcounter = INACTIVITYTIMEOUT;  

  // Get current alarm
  if (almPageDataId != 4)
  {
    alarmTimeGet(almPageDataId, &newHour, &newMin);
  }

  while (1)
  {
#ifdef EMULIN
    stubGetEvent();
#endif
    if (just_pressed & BTTN_MENU)
    {
      // Mode change
      just_pressed = 0;
      return;
    }
    if (just_pressed || pressed)
    {
      // Button pressed so reset timoutcounter
      timeoutcounter = INACTIVITYTIMEOUT;  
    }
    else if (!timeoutcounter)
    {
      // Timed out!
      displaymode = SHOW_TIME;     
      return;
    }
    if (just_pressed & BTTN_SET)
    {
      just_pressed = 0;
      screenmutex++;
      if (mode == SET_ALARM)
      {
        if (almPageDataId == 4)
        {
          DEBUG(putstring_nl("Set selected alarm"));
          // Now it is selected
          mode = SET_ALARM_ID;
          // Print the alarm Id inverted
          glcdSetAddress(MENU_INDENT + 18 * 6, 5);
          glcdPrintNumber(newAlarmSelect + 1, mcBgColor);
          // Display instructions below
          print_instructions2(instrPlusPrefix, "alm", instrSetPrefix, "alm ",
            mcFgColor);
        }
        else
        {
          DEBUG(putstring_nl("Set alarm hour"));
          // Now it is selected
          mode = SET_HOUR;
          // Print the hour inverted
          glcdSetAddress(MENU_INDENT + 15 * 6, 1 + almPageDataId);
          glcdPrintNumber(newHour, mcBgColor);
          // Display instructions below
          print_instructions2(instrPlusPrefix, "hr.", instrSetPrefix, "hour",
            mcFgColor);
        }
      }
      else if (mode == SET_HOUR)
      {
        DEBUG(putstring_nl("Set alarm min"));
        mode = SET_MIN;
        // Print the hour normal
        glcdSetAddress(MENU_INDENT + 15 * 6, 1 + almPageDataId);
        glcdPrintNumber(newHour, mcFgColor);
        // and the minutes inverted
        glcdSetAddress(MENU_INDENT + 18 * 6, 1 + almPageDataId);
        glcdPrintNumber(newMin, mcBgColor);
        // Display instructions below
        print_instructions2(instrPlusPrefix, "min", instrSetPrefix, "min ",
          mcFgColor);
      }
      else
      {
        // Clear edit mode
        if (mode == SET_ALARM_ID)
        {
          // Print the alarm Id normal
          glcdSetAddress(MENU_INDENT + 18 * 6, 5);
          glcdPrintNumber(newAlarmSelect + 1, mcFgColor);
          // Save and sync the new alarm Id
          eeprom_write_byte((uint8_t *)EE_ALARM_SELECT, newAlarmSelect);
          alarmSelect = newAlarmSelect;
        }
        else
        {
          // Print the hour and minutes normal
          glcdSetAddress(MENU_INDENT + 15 * 6, 1 + almPageDataId);
          glcdPrintNumber(newHour, mcFgColor);
          glcdSetAddress(MENU_INDENT + 18 * 6, 1 + almPageDataId);
          glcdPrintNumber(newMin, mcFgColor);
          // Save the new alarm time
          alarmTimeSet(almPageDataId, newHour, newMin);
        }

        mode = SET_ALARM;
        // Sync alarm time in case the time or the alarm id was changed
        alarmTimeGet(newAlarmSelect, &mcAlarmH, &mcAlarmM);
        // Display instructions below
        if (displaymode != SET_ALARM_ID)
          print_instructions(instrAdvance, instrSet);
        else
          print_instructions(instrExit, instrSet);
      }
      screenmutex--;
    }
    if ((just_pressed & BTTN_PLUS) || (pressed & BTTN_PLUS))
    {
      just_pressed = 0;
      screenmutex++;
      if (mode == SET_ALARM_ID)
      {
        newAlarmSelect++;
        if (newAlarmSelect >= 4)
          newAlarmSelect = 0;
        // Print the alarm Id inverted
        glcdSetAddress(MENU_INDENT + 18 * 6, 5);
        glcdPrintNumber(newAlarmSelect + 1, mcBgColor);
        DEBUG(putstring("New alarm Id -> "));
        DEBUG(uart_putw_dec(newAlarmSelect + 1));
        DEBUG(putstring_nl(""));
      }
      else if (mode == SET_HOUR)
      {
        newHour++;
        if (newHour >= 24)
          newHour = 0;
        // Print the hour inverted
        glcdSetAddress(MENU_INDENT + 15 * 6, 1 + almPageDataId);
        glcdPrintNumber(newHour, mcBgColor);
        DEBUG(putstring("New alarm hour -> "));
        DEBUG(uart_putw_dec(newHour));
        DEBUG(putstring_nl(""));
      }
      else if (mode == SET_MIN)
      {
        newMin++;
        if (newMin >= 60)
          newMin = 0;
        // Print the minutes inverted
        glcdSetAddress(MENU_INDENT + 18 * 6, 1 + almPageDataId);
        glcdPrintNumber(newMin, mcBgColor);
        DEBUG(putstring("New alarm min -> "));
        DEBUG(uart_putw_dec(newMin));
        DEBUG(putstring_nl(""));
      }
      screenmutex--;
      if (pressed & BTTN_PLUS)
	_delay_ms(KEYPRESS_DLY_1);
    }
  }
}

#ifdef BACKLIGHT_ADJUST
//
// Function: set_backlight
//
// Set display backlight brightness by processing button presses
//
void set_backlight(void)
{
  uint8_t mode = SET_BRIGHTNESS;

  display_main_menu();
  screenmutex++;

  // Last item before leaving setup page
  print_instructions(instrExit, 0);

  // Put a small arrow next to 'Backlight'
  print_arrow(0, 43, MENU_INDENT - 1, mcFgColor);
  screenmutex--;

  timeoutcounter = INACTIVITYTIMEOUT;  

  while (1)
  {
#ifdef EMULIN
    stubGetEvent();
#endif
    if (just_pressed & BTTN_MENU)
    {
      // Mode change
      eeprom_write_byte((uint8_t *)EE_BRIGHT, OCR2B);
#ifdef BACKLIGHT_AUTO
      eeprom_write_byte((uint8_t *)EE_BRIGHT_AUTO, backlightauto);
#endif
      return;
    }

    if (just_pressed || pressed)
    {
      // Button pressed so reset timoutcounter
      timeoutcounter = INACTIVITYTIMEOUT;  
    }
    else if (!timeoutcounter)
    {
      // Timed out!
      eeprom_write_byte((uint8_t *)EE_BRIGHT, OCR2B);
#ifdef BACKLIGHT_AUTO
      eeprom_write_byte((uint8_t *)EE_BRIGHT_AUTO, backlightauto);
#endif
      displaymode = SHOW_TIME;     
      return;
    }
  
    if (just_pressed & BTTN_SET)
    {
      just_pressed = 0;
      screenmutex++;
      if (mode == SET_BRIGHTNESS)
      {
        DEBUG(putstring_nl("Setting backlight"));
        // Now it is selected
        mode = SET_BRT;
        print_backlight_setting(mcFgColor);
        print_instructions(instrChange, instrSave);
      }
      else
      {
        mode = SET_BRIGHTNESS;
        print_backlight_setting(mcFgColor);
        print_instructions(instrExit, instrSet);
      }
      screenmutex--;
    }
    if ((just_pressed & BTTN_PLUS) || (pressed & BTTN_PLUS))
    {
      just_pressed = 0;
      if (mode == SET_BRT)
      {
#ifdef BACKLIGHT_AUTO
        // sequence is 0...16,auto,0...
        if (backlightauto == GLCD_TRUE)
        {
          backlightauto = GLCD_FALSE;
          OCR2B = 0;
        }
        else if((OCR2B + OCR2B_PLUS) > OCR2A_VALUE)
          backlightauto = GLCD_TRUE;
        else
          OCR2B += OCR2B_PLUS;
#else
        OCR2B += OCR2B_PLUS;
#endif

        if(OCR2B > OCR2A_VALUE)
          OCR2B = 0;

        screenmutex++;
        display_main_menu();
        print_instructions(instrChange, instrSave);
        // Put a small arrow next to 'Backlight'
        print_arrow(0, 43, MENU_INDENT - 1, mcFgColor);
        print_backlight_setting(mcFgColor);
        screenmutex--;
      }
      if (pressed & BTTN_PLUS)
        _delay_ms(KEYPRESS_DLY_1);
    }
  }
}
#endif

//
// Function: set_date
//
// Set a data by setting all individual items of a date by processing
// button presses
//
void set_date(void)
{
  uint8_t mode = SET_DATE;
  uint8_t day, month, year;
    
  day = date_d;
  month = date_m;
  year = date_y;

  display_main_menu();

  // Put a small arrow next to 'Date'
  screenmutex++;
  print_arrow(0, 27, MENU_INDENT - 1, mcFgColor);
  screenmutex--;
  
  timeoutcounter = INACTIVITYTIMEOUT;  

  while (1)
  {
#ifdef EMULIN
    stubGetEvent();
#endif
    if (just_pressed & BTTN_MENU)
    {
      // Mode change
      return;
    }
    if (just_pressed || pressed)
    {
      // Button pressed so reset timoutcounter
      timeoutcounter = INACTIVITYTIMEOUT;  
    }
    else if (!timeoutcounter)
    {
      // Timed out!
      displaymode = SHOW_TIME;     
      return;
    }
    if (just_pressed & BTTN_SET)
    {
      just_pressed = 0;
      screenmutex++;

#ifdef DATE_MONTHDAY
      if (mode == SET_DATE)
#else
      if (mode == SET_DAY)
#endif
      {
        DEBUG(putstring_nl("Set date month"));
        // Now it is selected
        mode = SET_MONTH;
        // Print the month inverted
        print_date(month, day, year, mode);
        // Display instructions below
        print_instructions2(instrPlusPrefix, "mon", instrSetPrefix, "mon ",
          mcFgColor);
      }
#ifdef DATE_MONTHDAY
      else if (mode == SET_MONTH)
#else
      else if (mode == SET_DATE)
#endif
      {
        DEBUG(putstring_nl("Set date day"));
        mode = SET_DAY;
        // Print the day inverted
        print_date(month, day, year, mode);
        // Display instructions below
        print_instructions2(instrPlusPrefix, "day", instrSetPrefix, "day ",
          mcFgColor);
      }
#ifdef DATE_MONTHDAY
      else if ((mode == SET_DAY))
#else
      else if (mode == SET_MONTH)
#endif
      {
        DEBUG(putstring_nl("Set year"));
        mode = SET_YEAR;
        // Print the year inverted
        print_date(month, day, year, mode);
        // Display instructions below
        print_instructions2(instrPlusPrefix, "yr.", instrSetPrefix, "year",
          mcFgColor);
      }
      else
      {
        // Done!
        DEBUG(putstring_nl("Done setting date"));
        mode = SET_DATE;
        // Print the date normal
        print_date(month,day,year,mode);
        // Display instructions below
        print_instructions(instrAdvance, instrSet);
	// Update date
        date_y = year;
        date_m = month;
        date_d = day;
        writei2ctime(time_s, time_m, time_h, 0, day, month, year);
      }
      screenmutex--;
    }
    if ((just_pressed & BTTN_PLUS) || (pressed & BTTN_PLUS))
    {
      // Increment the date element currently in edit mode
      just_pressed = 0;
      screenmutex++;
      if (mode == SET_MONTH)
      {
        month++;
        if (month >= 13)
        {
          month = 1;
        }
        else if (month == 2)
        {
          if (day > 29)
            day = 29;
          if (!leapyear(year) && (day > 28))
            day = 28;
        }
        else if (month == 4 || month == 6 || month == 9 || month == 11)
        {
          if (day > 30)
      	    day = 30;
        }
      }
      if (mode == SET_DAY)
      {
        day++;
        if (day > 31)
        {
          day = 1;
        }
        else if (month == 2)
        {
          if (day > 29)
            day = 1;
          else if (!leapyear(year) && (day > 28))
            day = 1;
        }
        else if (month == 4 || month == 6 || month == 9 || month == 11)
        {
          if (day > 30)
      	    day = 1;
        }
      }
      if (mode == SET_YEAR)
      {
        year++;
        if (year >= 100)
          year = 0;
        if (!leapyear(year) && month == 2 && day > 28)
          day = 28;
      }
      print_date(month, day, year, mode);
      screenmutex--;
      if (pressed & BTTN_PLUS)
        _delay_ms(KEYPRESS_DLY_1);  
    }
  }
}

//
// Function: set_display
//
// Set the display type by processing button presses
//
void set_display(void)
{
  uint8_t mode = SET_DISPLAY;

  display_main_menu();

  screenmutex++;
#ifndef BACKLIGHT_ADJUST
  print_instructions(instrExit, 0);
#endif
  // Put a small arrow next to 'Display'
  print_arrow(0, 35, MENU_INDENT - 1, mcFgColor);
  screenmutex--;

  timeoutcounter = INACTIVITYTIMEOUT;  
  while (1)
  {
#ifdef EMULIN
    stubGetEvent();
#endif
    if (just_pressed & BTTN_MENU)
    {
      // Menu change
      eeprom_write_byte((uint8_t *)EE_BGCOLOR, mcBgColor);
      return;
    }

    if (just_pressed || pressed)
    {
      // Button pressed so reset timoutcounter
      timeoutcounter = INACTIVITYTIMEOUT;  
    }
    else if (!timeoutcounter)
    {
      // Timed out!
      eeprom_write_byte((uint8_t *)EE_BGCOLOR, mcBgColor);
      displaymode = SHOW_TIME;     
      return;
    }
  
    if (just_pressed & BTTN_SET)
    {
      just_pressed = 0;
      screenmutex++;
      if (mode == SET_DISPLAY)
      {
        // In set mode: display value inverse
        DEBUG(putstring_nl("Setting display"));
        mode = SET_DSP;
        print_display_setting(mcBgColor);
        // Display instructions below
        print_instructions(instrChange, instrSave);
      }
      else
      {
        // In select mode: display value normal
        mode = SET_DISPLAY;
        print_display_setting(mcFgColor);
        // Display instructions below
#ifdef BACKLIGHT_ADJUST
        print_instructions(instrAdvance, instrSet);
#else
        print_instructions(instrExit, instrSet);
#endif
      }
      screenmutex--;
    }
    if ((just_pressed & BTTN_PLUS) || (pressed & BTTN_PLUS))
    {
      just_pressed = 0;
      if (mode == SET_DSP)
      {
        // Toggle display mode
        if (mcBgColor == OFF)
        {
          mcBgColor = ON;
          mcFgColor = OFF;
        }
        else
        {
          mcBgColor = OFF;
          mcFgColor = ON;
        }

        // Inverse and rebuild screen
        screenmutex++;
        display_main_menu();
        // Display instructions below
        print_instructions(instrChange, instrSave);
        // Put a small arrow next to 'Display'
        print_arrow(0, 35, MENU_INDENT - 1, mcFgColor);
        print_display_setting(mcBgColor);
        screenmutex--;
        DEBUG(putstring("New display type -> "));
        DEBUG(uart_putw_dec(mcBgColor));
        DEBUG(putstring_nl(""));
      }
      if (pressed & BTTN_PLUS)
        _delay_ms(KEYPRESS_DLY_1);
    }
  }
}

//
// Function: set_time
//
// Set the system time by processing button presses
//
void set_time(void)
{
  uint8_t mode = SET_TIME;

  uint8_t hour, min, sec;
    
  hour = time_h;
  min = time_m;
  sec = time_s;

  display_main_menu();
  
  screenmutex++;
  // Put a small arrow next to 'Time'
  print_arrow(0, 19, MENU_INDENT - 1, mcFgColor);
  screenmutex--;
 
  timeoutcounter = INACTIVITYTIMEOUT;  

  while (1) {
#ifdef EMULIN
    stubGetEvent();
#endif
    if (just_pressed & BTTN_MENU)
    {
      // Mode change
      return;
    }
    if (just_pressed || pressed)
    {
      // Button pressed so reset timoutcounter
      timeoutcounter = INACTIVITYTIMEOUT;  
    }
    else if (!timeoutcounter)
    {
      // Timed out!
      displaymode = SHOW_TIME;     
      return;
    }
    if (just_pressed & BTTN_SET)
    {
      just_pressed = 0;
      screenmutex++;
      if (mode == SET_TIME)
      {
        DEBUG(putstring_nl("Set time hour"));
        // Now it is selected
        mode = SET_HOUR;
        // Print the hour inverted
        glcdSetAddress(MENU_INDENT + 12 * 6, 2);
        glcdPrintNumber(hour, mcBgColor);
        // Display instructions below
        print_instructions2(instrPlusPrefix, "hr.", instrSetPrefix, "hour",
          mcFgColor);
      }
      else if (mode == SET_HOUR)
      {
        DEBUG(putstring_nl("Set time min"));
        mode = SET_MIN;
        // Print the hour normal
        glcdSetAddress(MENU_INDENT + 12 * 6, 2);
        glcdPrintNumber(hour, mcFgColor);
        // and the minutes inverted
        glcdWriteChar(':', mcFgColor);
        glcdPrintNumber(min, mcBgColor);
        // Display instructions below
        print_instructions2(instrPlusPrefix, "min", instrSetPrefix, "min ",
          mcFgColor);
      }
      else if (mode == SET_MIN)
      {
        DEBUG(putstring_nl("Set time sec"));
        mode = SET_SEC;
        // Print the minutes normal
        glcdSetAddress(MENU_INDENT + 15 * 6, 2);
        glcdPrintNumber(min, mcFgColor);
        glcdWriteChar(':', mcFgColor);
        // and the seconds inverted
        glcdPrintNumber(sec, mcBgColor);
        // Display instructions below
        print_instructions2(instrPlusPrefix, "sec", instrSetPrefix, "sec ",
          mcFgColor);
      }
      else
      {
        // done!
        DEBUG(putstring_nl("Done setting time"));
        mode = SET_TIME;
        // Print the seconds normal
        glcdSetAddress(MENU_INDENT + 18 * 6, 2);
        glcdPrintNumber(sec, mcFgColor);
        // Display instructions below
        print_instructions(instrAdvance, instrSet);
        // Update time
        time_h = hour;
        time_m = min;
        time_s = sec;
        writei2ctime(sec, min, hour, 0, date_d, date_m, date_y);
      }
      screenmutex--;
    }
    if ((just_pressed & BTTN_PLUS) || (pressed & BTTN_PLUS))
    {
      // Increment the time element currently in edit mode
      just_pressed = 0;
      screenmutex++;
      if (mode == SET_HOUR)
      {
        hour++;
        if (hour >= 24)
          hour = 0;
        glcdSetAddress(MENU_INDENT + 12 * 6, 2);
        glcdPrintNumber(hour, mcBgColor);
      }
      if (mode == SET_MIN)
      {
        min++;
        if (min >= 60)
          min = 0;
        glcdSetAddress(MENU_INDENT + 15 * 6, 2);
        glcdPrintNumber(min, mcBgColor);
      }
      if (mode == SET_SEC)
      {
        sec++;
        if (sec >= 60)
          sec = 0;
        glcdSetAddress(MENU_INDENT + 18 * 6, 2);
        glcdPrintNumber(sec, mcBgColor);
      }
      screenmutex--;
      if (pressed & BTTN_PLUS)
        _delay_ms(KEYPRESS_DLY_1);
    }
  }
}

