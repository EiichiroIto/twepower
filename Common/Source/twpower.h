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

/****************************************************************************/
/***        Exported Functions                                            ***/
/****************************************************************************/
void vPutHex(uint8 *buf, int hex);
bool_t vCheckCRC(uint8 *buf, int size);

#if defined __cplusplus
}
#endif

#endif /* TWPOWER_H_INCLUDED */
