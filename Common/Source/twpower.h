/* Copyright (C) 2019 Eiichiro Ito. All Rights Reserved.    *
 * Released under BSD License                               */

#ifndef  TWPOWER_H_INCLUDED
#define  TWPOWER_H_INCLUDED

#if defined __cplusplus
extern "C" {
#endif

/****************************************************************************/
/***        Macro Definitions                                             ***/
/****************************************************************************/
#define TWPOWER_HEADER_ID ":TP"
#define TWPOWER_HEADER_ID_SIZE 3
#define TWPOWER_HEADER  ":TPszcr"
#define TWPOWER_HEADER_SIZE 7
#define TWPOWER_LEN_POS 3
#define TWPOWER_LEN_SIZE 2
#define TWPOWER_CRC_POS 5
#define TWPOWER_CRC_SIZE 2
#define TWPOWER_CMD_POS 7
#define TWPOWER_CMD_SIZE 2

#define TWPOWER_CMD_STATUS "ST"
#define TWPOWER_CMD_ON "ON"
#define TWPOWER_CMD_OFF "OF"
#define TWPOWER_CMD_ON_REPLY "on"
#define TWPOWER_CMD_OFF_REPLY "of"
#define TWPOWER_CMD_LED "LD"

#define TWPOWER_ADC_SIZE 4
#define TWPOWER_ADCVOLT_POS 2
#define TWPOWER_ADC1_POS 6
#define TWPOWER_ADC3_POS 10

/****************************************************************************/
/***        Exported Functions                                            ***/
/****************************************************************************/
void vPutHexByte(uint8 *buf, uint16 hex);
void vPutHexWord(uint8 *buf, uint16 hex);
uint16 vGetHexNibble(uint8 *buf);
uint16 vGetHexByte(uint8 *buf);
uint16 vGetHexWord(uint8 *buf);
bool_t vCheckCRC(uint8 *buf, int size);

#if defined __cplusplus
}
#endif

#endif /* TWPOWER_H_INCLUDED */
