/* Copyright (C) 2019 Eiichiro Ito. All Rights Reserved.    *
 * Released under BSD License                               */

/****************************************************************************/
/***        Include files                                                 ***/
/****************************************************************************/
#include <string.h>

#include <jendefs.h>
#include <AppHardwareApi.h>

#include "utils.h"
#include "ccitt8.h"

#include "twpower.h"

static char hextable[17] = "0123456789ABCDEF";

/****************************************************************************
 *
 * NAME: vPutHexByte()
 *
 * DESCRIPTION:
 *
 * RETURNS:
 *
 ****************************************************************************/
void vPutHexByte(uint8 *buf, uint16 hex)
{
	*buf++ = hextable[(hex & 0xF0) >> 4];
	*buf++ = hextable[hex & 0x0F];
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
void vPutHexWord(uint8 *buf, uint16 hex)
{
	vPutHexByte(buf, (hex & 0xFF00) >> 8);
	vPutHexByte(buf + 2, hex & 0xFF);
}

/****************************************************************************
 *
 * NAME: vGetHexNibble()
 *
 * DESCRIPTION:
 *
 * RETURNS:
 *
 ****************************************************************************/
uint16 vGetHexNibble(uint8 *buf)
{
	uint16 c = *buf++;
	if (c >= '0' && c <= '9') {
		c -= '0';
	} else if (c >= 'a' && c <= 'f') {
		c -= 'a' - 10;
	} else if (c >= 'A' && c <= 'F') {
		c -= 'A' - 10;
	} else {
		c = 0;
	}
	return c;
}

/****************************************************************************
 *
 * NAME: vGetHexByte()
 *
 * DESCRIPTION:
 *
 * RETURNS:
 *
 ****************************************************************************/
uint16 vGetHexByte(uint8 *buf)
{
	uint16 ret = vGetHexNibble(buf);
	ret <<= 4;
	ret |= vGetHexNibble(buf + 1);
	return ret;
}

/****************************************************************************
 *
 * NAME: vGetHexWord()
 *
 * DESCRIPTION:
 *
 * RETURNS:
 *
 ****************************************************************************/
uint16 vGetHexWord(uint8 *buf)
{
	uint16 ret = vGetHexByte(buf);
	ret <<= 8;
	ret |= vGetHexByte(buf + 2);
	return ret;
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
