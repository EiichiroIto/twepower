/* Copyright (C) 2017 Mono Wireless Inc. All Rights Reserved.    *
 * Released under MW-SLA-*J,*E (MONO WIRELESS SOFTWARE LICENSE   *
 * AGREEMENT).                                                   */

#ifndef  SLAVE_H_INCLUDED
#define  SLAVE_H_INCLUDED

#if defined __cplusplus
extern "C" {
#endif

/****************************************************************************/
/***        Include Files                                                 ***/
/****************************************************************************/
#include "config.h"
#include "twpower.h"

/****************************************************************************/
/***        Macro Definitions                                             ***/
/****************************************************************************/
#define PORT_OFF 18
#define PORT_ON 19
//#define PORT_OUT3 4
#define PORT_LED 9
#define PORT_INPUT1 12
#define PORT_INPUT2 13
#define PORT_INPUT3 11
#define PORT_INPUT4 16

#define SLEEP_COUNT	125
#define SLEEP_COUNT_KEYPRESSED (250*1)
#define SLEEP_SECONDS 10
#define AUTOOFF_COUNT (250*5)

/****************************************************************************/
/***        Type Definitions                                              ***/
/****************************************************************************/
typedef enum
{
	E_TWPOWER_COMMAND_IDLE = 0x00,    //!< 開始前
	E_TWPOWER_COMMAND_ON,
	E_TWPOWER_COMMAND_OFF,
	E_TWPOWER_COMMAND_ON_REPLY,
	E_TWPOWER_COMMAND_OFF_REPLY,
	E_TWPOWER_COMMAND_AUTOOFF,
} teCommand_TWPOWER;

typedef enum
{
	E_ADC_INIT = 0x00,
	E_ADC_START,
	E_ADC_READY,
	E_ADC_COMPLETE,
} teAdcState;

/****************************************************************************/
/***        Exported Functions                                            ***/
/****************************************************************************/

/****************************************************************************/
/***        Exported Variables                                            ***/
/****************************************************************************/

#if defined __cplusplus
}
#endif

#endif  /* SLAVE_H_INCLUDED */

/****************************************************************************/
/***        END OF FILE                                                   ***/
/****************************************************************************/
