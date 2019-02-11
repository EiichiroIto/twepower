/* Copyright (C) 2019 Eiichiro Ito. All Rights Reserved.    *
 * Released under BSD License                               */

/****************************************************************************/
/***        Include files                                                 ***/
/****************************************************************************/
#include <string.h>

#include <jendefs.h>
#include <AppHardwareApi.h>

#include "utils.h"

#include "twpower.h"

/****************************************************************************
 *
 * NAME: vPutHexByte()
 *
 * DESCRIPTION:
 *
 * RETURNS:
 *
 ****************************************************************************/
void vPutHexByte(uint8 *buf, uint16 hex) {
	static char table[17] = "0123456789ABCDEF";

	*buf++ = table[(hex & 0xF0) >> 4];
	*buf++ = table[hex & 0x0F];
}

/****************************************************************************
 *
 * NAME: vPutHexWord()
 *
 * DESCRIPTION:
 *
 * RETURNS:
 *
 ****************************************************************************/
void vPutHexWord(uint8 *buf, uint16 hex) {
	vPutHexByte(buf, (hex & 0xFF00) >> 8);
	vPutHexByte(buf + 2, hex & 0xFF);
}

/****************************************************************************
 *
 * NAME: vCheckCRC()
 *
 * DESCRIPTION:
 *
 * RETURNS:
 *
 ****************************************************************************/
bool_t vCheckCRC(uint8 *buf, int size)
{
	if (size < TWPOWER_HEADER_SIZE) {
		return FALSE;
	}
	uint8 u8crc = u8CCITT8(&buf[TWPOWER_HEADER_SIZE], size - TWPOWER_HEADER_SIZE);
	uint8 dataCrc[TWPOWER_CRC_SIZE+1] = "00";
	vPutHexByte(dataCrc, u8crc);
	return !memcmp(&buf[TWPOWER_CRC_POS], dataCrc, TWPOWER_CRC_SIZE);
}
