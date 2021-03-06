//*****************************************************************************
// Filename : 'analog.c'
// Title    : Animation code for MONOCHRON analog clock
//*****************************************************************************

#include <math.h>
#ifdef EMULIN
#include "../emulator/stub.h"
#endif
#ifndef EMULIN
#include "../util.h"
#endif
#include "../ks0108.h"
#include "../ratt.h"
#include "../glcd.h"
#include "../anim.h"
#include "analog.h"

// Specifics for analog clock
#define ANA_X_START		64
#define ANA_Y_START		31
#define ANA_RADIUS		30
#define ANA_DOT_RADIUS		(ANA_RADIUS - 1.9L)
#define ANA_SEC_RADIUS		(ANA_RADIUS - 5.9L)
#define ANA_MIN_RADIUS		(ANA_RADIUS - 2.9L)
#define ANA_HOUR_RADIUS		(ANA_RADIUS - 9.9L)
#define ANA_MIN_LEG_RADIUS	8
#define ANA_HOUR_LEG_RADIUS	5
#define ANA_SECMIN_STEPS	60L
#define ANA_HOUR_STEPS		12L
#define ANA_MIN_LEG_RADIAL_OFFSET	(2L * M_PI / 2.50L)
#define ANA_HOUR_LEG_RADIAL_OFFSET	(2L * M_PI / 2.50L)

#define ANA_ALARM_X_START	9
#define ANA_ALARM_Y_START	54
#define ANA_ALARM_RADIUS	7
#define ANA_ALARM_MIN_RADIUS	(ANA_ALARM_RADIUS)
#define ANA_ALARM_HOUR_RADIUS	(ANA_ALARM_RADIUS - 2)
#define ANA_DATE_X_START	2
#define ANA_DATE_Y_START	57
#define ANA_DATE_X_SIZE		23

extern volatile uint8_t mcClockOldTS, mcClockOldTM, mcClockOldTH;
extern volatile uint8_t mcClockNewTS, mcClockNewTM, mcClockNewTH;
extern volatile uint8_t mcClockOldDD, mcClockOldDM, mcClockOldDY;
extern volatile uint8_t mcClockNewDD, mcClockNewDM, mcClockNewDY;
extern volatile uint8_t mcClockInit;
extern volatile uint8_t mcAlarming, mcAlarmH, mcAlarmM;
extern volatile uint8_t mcAlarmSwitch;
extern volatile uint8_t mcU8Util1;
extern volatile uint8_t mcUpdAlarmSwitch;
extern volatile uint8_t mcCycleCounter;
extern volatile uint8_t mcClockTimeEvent;
extern volatile uint8_t mcBgColor, mcFgColor;
extern unsigned char *months[12];

// Local function prototypes
void analogAlarmAreaUpdate(void);
u08 analogElementCalc(s08 position[], s08 positionNew[], float radial, float radialOffset,
  u08 arrowRadius, u08 legRadius);
void analogElementDraw(s08 position[], u08 color);
void analogElementSync(s08 position[], s08 positionNew[]);
void analogInit(u08 mode);

// Indicator whether to show the seconds needle
u08 anaSecShow;

// Arrays holding the [x,y] positions of the three arrow points
// for the hour and minute arrows and the seconds needle
// arr[0+1] = x,y arrow endpoint
// arr[2+3] = x,y arrow leg endpoint 1 
// arr[4+5] = x,y arrow leg endpoint 2
s08 posSec[6];
s08 posMin[6];
s08 posHour[6];

//
// Function: analogCycle
//
// Update the LCD display of a very simple analog clock
//
void analogCycle(void)
{
  // Update alarm info in clock
  analogAlarmAreaUpdate();

  // Only if a time event or init is flagged we need to update the clock
  if (mcClockTimeEvent == GLCD_FALSE && mcClockInit == GLCD_FALSE)
    return;

  DEBUGP("Update Analog");

  // Local data
  float radElement;
  s08 posSecNew[6], posMinNew[6], posHourNew[6];
  u08 secElementChanged = GLCD_FALSE;
  u08 minElementChanged = GLCD_FALSE;
  u08 hourElementChanged = GLCD_FALSE;

  // Calculate (potential) changes in seconds needle
  if (anaSecShow == GLCD_TRUE)
  {
    radElement = (2L * M_PI / ANA_SECMIN_STEPS) * mcClockNewTS;
    secElementChanged = analogElementCalc(posSec, posSecNew, radElement,
      0L, ANA_SEC_RADIUS, 0);
  }

  // Calculate (potential) changes in minute arrow
  radElement = (2L * M_PI / ANA_SECMIN_STEPS) * mcClockNewTM;
  minElementChanged = analogElementCalc(posMin, posMinNew, radElement,
    ANA_MIN_LEG_RADIAL_OFFSET, ANA_MIN_RADIUS, ANA_MIN_LEG_RADIUS);

  // Calculate (potential) changes in hour arrow.
  // Note: Include progress in minutes during the hour.
  radElement = (2L * M_PI / ANA_HOUR_STEPS) * (mcClockNewTH % 12) +
    (2L * M_PI / ANA_SECMIN_STEPS / ANA_HOUR_STEPS) * mcClockNewTM;
  hourElementChanged = analogElementCalc(posHour, posHourNew, radElement,
    ANA_HOUR_LEG_RADIAL_OFFSET, ANA_HOUR_RADIUS, ANA_HOUR_LEG_RADIUS);

  // Redraw seconds needle if needed
  if (anaSecShow == GLCD_TRUE &&
      (secElementChanged == GLCD_TRUE || mcClockInit == GLCD_TRUE))
  {
    // Remove the old seconds needle, sync with new position and redraw
    analogElementDraw(posSec, mcBgColor);
    analogElementSync(posSec, posSecNew);
    analogElementDraw(posSec, mcFgColor);
  }

  // Redraw minute arrow if needed
  if (minElementChanged == GLCD_TRUE || mcClockInit == GLCD_TRUE)
  {
    // Remove the old minute arrow, sync with new position and redraw
    analogElementDraw(posMin, mcBgColor);
    analogElementSync(posMin, posMinNew);
    analogElementDraw(posMin, mcFgColor);

    // Redraw the seconds needle as it got distorted by the minute
    // arrow draw
    if (anaSecShow == GLCD_TRUE)
      analogElementDraw(posSec, mcFgColor);
  }
  else if (secElementChanged == GLCD_TRUE)
  {
    // The minute arrow has not changed but the sec needle has.
    // Redraw the minute arrow as it got distorted by the seconds
    // needle draw.
    analogElementDraw(posMin, mcFgColor);
  }

  // Redraw hour arrow only if needed
  if (hourElementChanged  == GLCD_TRUE|| mcClockInit == GLCD_TRUE)
  {
    // Remove the old hour arrow, sync with new position and redraw
    analogElementDraw(posHour, mcBgColor);
    analogElementSync(posHour, posHourNew);
    analogElementDraw(posHour, mcFgColor);

    // Redraw the seconds needle and minute arrow as they get distorted
    // by the arrow redraw
    if (anaSecShow == GLCD_TRUE)
      analogElementDraw(posSec, mcFgColor);
    analogElementDraw(posMin, mcFgColor);
  }
  else if (secElementChanged == GLCD_TRUE || minElementChanged == GLCD_TRUE)
  {
    // The hour arrow has not changed but the seconds needle and/or
    // minute arrow has.
    // Redraw the hour arrow as it got distorted by the other
    // draws.
    analogElementDraw(posHour, mcFgColor);
  }
}

//
// Function: analogHmInit
//
// Initialize the LCD display of a very simple analog clock with
// Hour and Minutes arrow
//
void analogHmInit(u08 mode)
{
  anaSecShow = GLCD_FALSE;
  analogInit(mode);
}

//
// Function: analogHmsInit
//
// Initialize the LCD display of a very simple analog clock with
// Hour and Minutes arrow and Seconds needle
//
void analogHmsInit(u08 mode)
{
  anaSecShow = GLCD_TRUE;
  analogInit(mode);
}

//
// Function: analogAlarmAreaUpdate
//
// Draw update in analog clock alarm area
//
void analogAlarmAreaUpdate(void)
{
  u08 inverseAlarmArea = GLCD_FALSE;
  u08 newAlmDisplayState = GLCD_FALSE;

  if ((mcCycleCounter & 0x0F) >= 8)
    newAlmDisplayState = GLCD_TRUE;

  if (mcUpdAlarmSwitch == GLCD_TRUE)
  {
    if (mcAlarmSwitch == ALARM_SWITCH_ON)
    {
      // Show alarm time in small clock
      s08 dxM, dyM, dxH, dyH;
      float radM, radH;

      // Prepare the analog alarm clock
      radM = (2L * M_PI / ANA_SECMIN_STEPS) * mcAlarmM;
      dxM = (s08)(sin(radM) * ANA_ALARM_MIN_RADIUS);
      dyM = (s08)(-cos(radM) * ANA_ALARM_MIN_RADIUS);
      radH = (2L * M_PI / ANA_HOUR_STEPS) * (mcAlarmH % 12) +
        (2L * M_PI / ANA_SECMIN_STEPS / ANA_HOUR_STEPS) * mcAlarmM;
      dxH = (s08)(sin(radH) * ANA_ALARM_HOUR_RADIUS);
      dyH = (s08)(-cos(radH) * ANA_ALARM_HOUR_RADIUS);

      // Clear date area
      glcdFillRectangle(ANA_DATE_X_START, ANA_DATE_Y_START, ANA_DATE_X_SIZE,
        5, mcBgColor);

      // Show the alarm time
      glcdCircle2(ANA_ALARM_X_START, ANA_ALARM_Y_START, ANA_ALARM_RADIUS,
        CIRCLE_FULL, mcFgColor);
      glcdLine(ANA_ALARM_X_START, ANA_ALARM_Y_START, ANA_ALARM_X_START + dxM,
        ANA_ALARM_Y_START + dyM, mcFgColor);
      glcdLine(ANA_ALARM_X_START, ANA_ALARM_Y_START, ANA_ALARM_X_START + dxH,
        ANA_ALARM_Y_START + dyH, mcFgColor);
    }
    else
    {
      // Show date
      u08 pxDone = 0;
      char msg[4] = {0};

      // Clear alarm area
      glcdFillRectangle(ANA_ALARM_X_START - ANA_ALARM_RADIUS - 1,
        ANA_ALARM_Y_START - ANA_ALARM_RADIUS - 1, ANA_ALARM_RADIUS * 2 + 3,
        ANA_ALARM_RADIUS * 2 + 3, mcBgColor);
      mcU8Util1 = GLCD_FALSE;

      // Show the date
      char *s1, *s2;
#ifdef DATE_MONTHDAY
      s1 = (char *)months[mcClockNewDM - 1];
      s2 = msg;
      msg[0] = ' ';
      animValToStr(mcClockNewDD, &(msg[1]));
#else
      s1 = msg;
      s2 = (char *)months[mcClockNewDM - 1];
      animValToStr(mcClockNewDD, msg);
      msg[2] = ' ';
#endif
      pxDone = glcdPutStr2(ANA_DATE_X_START, ANA_DATE_Y_START, FONT_5X5P,
        s1, mcFgColor) + ANA_DATE_X_START;
      pxDone = pxDone + glcdPutStr2(pxDone, ANA_DATE_Y_START, FONT_5X5P, 
        s2, mcFgColor) - ANA_DATE_X_START;
      if (pxDone <= ANA_DATE_X_SIZE)
        glcdFillRectangle(ANA_DATE_X_START + pxDone, ANA_DATE_Y_START,
          ANA_DATE_X_SIZE - pxDone + 1, FILL_BLANK, mcBgColor);
    }
  }

  if (mcAlarming == GLCD_TRUE)
  {
    // Blink alarm area when we're alarming or snoozing
    if (newAlmDisplayState != mcU8Util1)
    {
      inverseAlarmArea = GLCD_TRUE;
      mcU8Util1 = newAlmDisplayState;
    }
  }
  else
  {
    // Reset inversed alarm area when alarming has stopped
    if (mcU8Util1 == GLCD_TRUE)
    {
      inverseAlarmArea = GLCD_TRUE;
      mcU8Util1 = GLCD_FALSE;
    }
  }

  // Inverse the alarm area if needed
  if (inverseAlarmArea == GLCD_TRUE)
    glcdFillRectangle2(ANA_ALARM_X_START - ANA_ALARM_RADIUS - 1,
      ANA_ALARM_Y_START - ANA_ALARM_RADIUS - 1, ANA_ALARM_RADIUS * 2 + 3,
      ANA_ALARM_RADIUS * 2 + 3, ALIGN_AUTO, FILL_INVERSE, mcBgColor);
}

//
// Function: analogElementCalc
//
// Calculate the position of a needle or three points of an analog clock arrow
//
u08 analogElementCalc(s08 position[], s08 positionNew[], float radial, float radialOffset,
  u08 arrowRadius, u08 legRadius)
{
  u08 i;
  u08 isSecondsNeedle;
  u08 posLimit;

  // For the seconds needle we don't need leg calculations 
  if (position[2] == ANA_X_START && position[3] == ANA_Y_START)
  {
    isSecondsNeedle = GLCD_TRUE;
    posLimit = 2; 
  }
  else
  {
    isSecondsNeedle = GLCD_FALSE;
    posLimit = 6;
  }

  // Calculate the new position of a needle or each of the three arrow points
  positionNew[0] = (s08)(sin(radial) * arrowRadius) + ANA_X_START;
  positionNew[1] = (s08)(-cos(radial) * arrowRadius) + ANA_Y_START;
  if (isSecondsNeedle == GLCD_FALSE)
  {
    positionNew[2] = (s08)(sin(radial + radialOffset) * legRadius) + ANA_X_START;
    positionNew[3] = (s08)(-cos(radial + radialOffset) * legRadius) + ANA_Y_START;
    positionNew[4] = (s08)(sin(radial - radialOffset) * legRadius) + ANA_X_START;
    positionNew[5] = (s08)(-cos(radial - radialOffset) * legRadius) + ANA_Y_START;
  }

  // Provide info if the needle or arrow has changed position  
  for (i = 0; i < posLimit; i++)
  {
    if (position[i] != positionNew[i])
      return GLCD_TRUE;
  }

  return GLCD_FALSE;
}

//
// Function: analogElementDraw
//
// Draw an arrow or needle in the analog clock
//
void analogElementDraw(s08 position[], u08 color)
{
  // An arrow consists of three points, so draw lines between each
  // of them. If it turns out to be a needle only draw the first line.
  glcdLine(position[0], position[1], position[2], position[3], color);
  if (position[2] != ANA_X_START || position[3] != ANA_Y_START)
  {
    // We're dealing with an arrow so draw the other two lines
    glcdLine(position[0], position[1], position[4], position[5], color);
    glcdLine(position[2], position[3], position[4], position[5], color);
  }
}

//
// Function: analogElementSync
//
// Sync the current needle or arrow position with the new one
//
void analogElementSync(s08 position[], s08 positionNew[])
{
  u08 i, posLimit;

  // For the seconds needle we don't want to copy leg info 
  if (position[2] == ANA_X_START && position[3] == ANA_Y_START)
    posLimit = 2; 
  else
    posLimit = 6;

  for (i = 0; i < posLimit; i++)
  {
    position[i] = positionNew[i];
  }
}

//
// Function: analogInit
//
// Initialize the LCD display of an analog clock
//
void analogInit(u08 mode)
{
  s08 i, dxDot, dyDot;

  DEBUGP("Init Analog");

  if (mode == DRAW_INIT_FULL)
  {
    // Draw static clock layout
    glcdClearScreen(mcBgColor);
    glcdCircle2(ANA_X_START, ANA_Y_START, ANA_RADIUS, CIRCLE_FULL, mcFgColor);
    glcdDot(ANA_X_START, ANA_Y_START, mcFgColor);
    
    // Paint 5-minute and 15 minute markers in clock
    for (i = 0; i < 12; i++)
    {
      // The 5-minute markers
      dxDot = (s08)(sin(2L * M_PI / 12L * i) * ANA_DOT_RADIUS);
      dyDot = (s08)(-cos(2L * M_PI / 12L * i) * ANA_DOT_RADIUS);
      glcdDot(ANA_X_START + dxDot, ANA_Y_START + dyDot, mcFgColor);

      // The additional 15-minute markers
      if (i % 3 == 0)
      {
        if (i == 0)
          dyDot--;
        else if (i == 3)
          dxDot++;
        else if (i == 6)
          dyDot++;
        else
          dxDot--;
        glcdDot(ANA_X_START + dxDot, ANA_Y_START + dyDot, mcFgColor);
      }
    }

    // Init the arrow point position arrays with harmless values
    // inside the clock area
    for (i = 0; i < 6; i++)
    {
      posSec[i] = 40;
      posMin[i] = 40;
      posHour[i] = 40;
    }

    // The following inits force the seconds element to become a needle
    posSec[2] = ANA_X_START;
    posSec[3] = ANA_Y_START;

    // Force the alarm info area to init itself
    mcAlarmSwitch = ALARM_SWITCH_NONE;
    mcU8Util1 = GLCD_FALSE;
  }
  else if (anaSecShow == GLCD_FALSE)
  {
    // Assume this is a partial init from an analog HMS clock to an
    // analog HM clock. So, we should remove the seconds needle.
    analogElementDraw(posSec, mcBgColor);

    // Restore dot at center of clock
    glcdDot(ANA_X_START, ANA_Y_START, mcFgColor);
  }
}

