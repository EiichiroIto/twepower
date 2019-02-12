/* Copyright (C) 2017 Mono Wireless Inc. All Rights Reserved.    *
 * Released under MW-SLA-*J,*E (MONO WIRELESS SOFTWARE LICENSE   *
 * AGREEMENT).                                                   */

/****************************************************************************/
/***        Include files                                                 ***/
/****************************************************************************/
#include <string.h>

#include <jendefs.h>
#include <AppHardwareApi.h>

#include "utils.h"
#include "ccitt8.h"

#include "sensor_driver.h"
#include "adc.h"

#include "Server.h"

// DEBUG options

#include "serial.h"
#include "fprintf.h"
#include "sprintf.h"

/****************************************************************************/
/***        ToCoNet Definitions                                           ***/
/****************************************************************************/
// Select Modules (define befor include "ToCoNet.h")
//#define ToCoNet_USE_MOD_NBSCAN // Neighbour scan module
//#define ToCoNet_USE_MOD_NBSCAN_SLAVE

// includes
#include "ToCoNet.h"
#include "ToCoNet_mod_prototype.h"

#include "app_event.h"

/****************************************************************************/
/***        Macro Definitions                                             ***/
/****************************************************************************/

/****************************************************************************/
/***        Type Definitions                                              ***/
/****************************************************************************/

typedef struct
{
    // MAC
    uint8 u8channel;
    uint16 u16addr;

    // シーケンス番号
    uint32 u32Seq;

    // スリープカウンタ
	uint32 u32SleepCountDown;
	bool_t bCommandInput;

	// ADC
	tsObjData_ADC sObjADC;	// ADC管理構造体（データ部）
	tsSnsObj sADC;			// ADC管理構造体（制御部）
	teAdcState u8AdcState;
	int16 ai16Volt;
	int16 ai16Adc1;
	int16 ai16Adc3;

	// コマンド処理
	teCommand_TWPOWER u8Command;
} tsAppData;

/****************************************************************************/
/***        Local Function Prototypes                                     ***/
/****************************************************************************/

static void vProcessEvCore(tsEvent *pEv, teEvent eEvent, uint32 u32evarg);

static void vInitHardware(int f_warm_start);

void vSerialInit(uint32 u32Baud, tsUartOpt *pUartOpt);
static void vHandleSerialInput(void);

static void vShowMessage(uint8 *buf, int size);
static void vBroadcastStatus(void);
static void vSendCommand(char *buf, int size);
static void vSleepSec(int sec);
static void vProcessIncomingData(tsRxDataApp *pRx);

/****************************************************************************/
/***        Exported Variables                                            ***/
/****************************************************************************/

/****************************************************************************/
/***        Local Variables                                               ***/
/****************************************************************************/
/* Version/build information. This is not used in the application unless we
   are in serial debug mode. However the 'used' attribute ensures it is
   present in all binary files, allowing easy identifaction... */

/* Local data used by the tag during operation */
static tsAppData sAppData;

PUBLIC tsFILE sSerStream;
tsSerialPortSetup sSerPort;

// Wakeup port
const uint32 u32DioPortWakeUp = 1UL << 7; // UART Rx Port

/****************************************************************************
 *
 * NAME: AppColdStart
 *
 * DESCRIPTION:
 *
 * RETURNS:
 *
 ****************************************************************************/
void cbAppColdStart(bool_t bAfterAhiInit)
{
	//static uint8 u8WkState;
	if (!bAfterAhiInit) {
		// before AHI init, very first of code.

		// Register modules
		ToCoNet_REG_MOD_ALL();

	} else {
		// disable brown out detect
		vAHI_BrownOutConfigure(0,//0:2.0V 1:2.3V
				FALSE,
				FALSE,
				FALSE,
				FALSE);

		// clear application context
		memset (&sAppData, 0x00, sizeof(sAppData));
		sAppData.u8channel = CHANNEL;
		sAppData.u32Seq = ToCoNet_u32GetRand();
		sAppData.u32SleepCountDown = SLEEP_COUNT;
		sAppData.bCommandInput = FALSE;

		// ToCoNet configuration
		sToCoNet_AppContext.u32AppId = APP_ID;
		sToCoNet_AppContext.u8Channel = CHANNEL;

		sToCoNet_AppContext.bRxOnIdle = TRUE;

		// others
		SPRINTF_vInit128();

		// Register
		ToCoNet_Event_Register_State_Machine(vProcessEvCore);

		// Others
		vInitHardware(FALSE);

		// MAC start
		ToCoNet_vMacStart();
	}
}

/****************************************************************************
 *
 * NAME: AppWarmStart
 *
 * DESCRIPTION:
 *
 * RETURNS:
 *
 ****************************************************************************/
static bool_t bWakeupByButton;

void cbAppWarmStart(bool_t bAfterAhiInit)
{
	if (!bAfterAhiInit) {
		// before AHI init, very first of code.
		//  to check interrupt source, etc.
		bWakeupByButton = FALSE;

		if(u8AHI_WakeTimerFiredStatus()) {
			// wake up timer
		} else
		if(u32AHI_DioWakeStatus() & u32DioPortWakeUp) {
			// woke up from DIO events
			bWakeupByButton = TRUE;
		} else {
			bWakeupByButton = FALSE;
		}
	} else {
		// Initialize hardware
		vInitHardware(TRUE);

		// MAC start
		ToCoNet_vMacStart();

		sAppData.u32SleepCountDown = SLEEP_COUNT;
	}
}

/****************************************************************************/
/***        Local Functions                                               ***/
/****************************************************************************/
/****************************************************************************
 *
 * NAME: vMain
 *
 * DESCRIPTION:
 *
 * RETURNS:
 *
 ****************************************************************************/
void cbToCoNet_vMain(void)
{
	/* handle uart input */
	vHandleSerialInput();

	if (sAppData.u32SleepCountDown == 2) {
		vPortSetLo(PORT_LED);
	}
	if (sAppData.u32SleepCountDown == 0) {
		vPortSetHi(PORT_SET);
		vPortSetHi(PORT_RESET);
		vPortSetHi(PORT_LED);
		vSleepSec(SLEEP_SECONDS);
		return;
	}
	sAppData.u32SleepCountDown --;
	if (sAppData.u8AdcState == E_ADC_COMPLETE) {
		vfPrintf(&sSerStream, "adc complete:%d" LB, sAppData.u32SleepCountDown);
		sAppData.u8AdcState = E_ADC_INIT;
		vBroadcastStatus();
	}
	if (sAppData.u8Command != E_TWPOWER_COMMAND_IDLE) {
		if (sAppData.u8Command == E_TWPOWER_COMMAND_ON) {
			if (sAppData.u32SleepCountDown == 8) {
				vfPrintf(&sSerStream, "Set Start" LB);
				vPortSetLo(PORT_SET);
			}
			if (sAppData.u32SleepCountDown == 4) {
				vfPrintf(&sSerStream, "Set Stop" LB);
				vPortSetHi(PORT_SET);
				sAppData.u8Command = E_TWPOWER_COMMAND_ON_REPLY;
			}
		}
		if (sAppData.u8Command == E_TWPOWER_COMMAND_ON_REPLY) {
			vfPrintf(&sSerStream, "Send On Reply" LB);
			vSendCommand(TWPOWER_CMD_ON_REPLY, TWPOWER_CMD_SIZE);
			sAppData.u8Command = E_TWPOWER_COMMAND_IDLE;
		}
		if (sAppData.u8Command == E_TWPOWER_COMMAND_OFF) {
			if (sAppData.u32SleepCountDown == 8) {
				vfPrintf(&sSerStream, "Reset Start" LB);
				vPortSetLo(PORT_RESET);
			}
			if (sAppData.u32SleepCountDown == 4) {
				vfPrintf(&sSerStream, "Reset Stop" LB);
				vPortSetHi(PORT_RESET);
				sAppData.u8Command = E_TWPOWER_COMMAND_OFF_REPLY;
			}
		}
		if (sAppData.u8Command == E_TWPOWER_COMMAND_OFF_REPLY) {
			vfPrintf(&sSerStream, "Send Off Reply" LB);
			vSendCommand(TWPOWER_CMD_OFF_REPLY, TWPOWER_CMD_SIZE);
			sAppData.u8Command = E_TWPOWER_COMMAND_IDLE;
		}
		if (sAppData.u8Command == E_TWPOWER_COMMAND_AUTOOFF) {
			if ((sAppData.u32SleepCountDown % 250) == 0) {
				vfPrintf(&sSerStream, "Auto Off: %d" LB, sAppData.u32SleepCountDown / 250);
			}
			if (sAppData.u32SleepCountDown < 10) {
				vPortSetLo(PORT_RESET);
			}
		}
	}
}

/****************************************************************************
 *
 * NAME: cbToCoNet_vNwkEvent
 *
 * DESCRIPTION:
 *
 * PARAMETERS:      Name            RW  Usage
 *
 * RETURNS:
 *
 * NOTES:
 ****************************************************************************/
void cbToCoNet_vNwkEvent(teEvent eEvent, uint32 u32arg) {
	switch(eEvent) {
	default:
		break;
	}
}

/****************************************************************************
 *
 * NAME: cbvMcRxHandler
 *
 * DESCRIPTION:
 *
 * RETURNS:
 *
 ****************************************************************************/
void cbToCoNet_vRxEvent(tsRxDataApp *pRx) {
	static uint16 u16seqPrev = 0xFFFF;

	// print coming payload
	vfPrintf(&sSerStream, LB"[PKT Ad:%04x,Ln:%03d,Seq:%03d,Lq:%03d,Tms:%05d \"",
			pRx->u32SrcAddr,
			pRx->u8Len+4, // Actual payload byte: the network layer uses additional 4 bytes.
			pRx->u8Seq,
			pRx->u8Lqi,
			pRx->u32Tick & 0xFFFF);
	vShowMessage(pRx->auData, pRx->u8Len);
	vfPrintf(&sSerStream, "\"]");

	// 打ち返す
	if (pRx->u8Seq == u16seqPrev) {
		vfPrintf(&sSerStream, LB "Duplicated Message" LB);
	} else if (memcmp(pRx->auData, TWPOWER_HEADER_ID, TWPOWER_HEADER_ID_SIZE)) {
		vfPrintf(&sSerStream, LB "Invalid Message" LB);
	} else if (!vCheckCRC(pRx->auData, pRx->u8Len)) {
		vfPrintf(&sSerStream, LB "Invalid CRC" LB);
	} else {
		u16seqPrev = pRx->u8Seq;
		vProcessIncomingData(pRx);
	}
}

/****************************************************************************
 *
 * NAME: cbvMcEvTxHandler
 *
 * DESCRIPTION:
 *
 * PARAMETERS:      Name            RW  Usage
 *
 * RETURNS:
 *
 * NOTES:
 ****************************************************************************/
void cbToCoNet_vTxEvent(uint8 u8CbId, uint8 bStatus) {
	vfPrintf(&sSerStream, LB "Send Done: %d" LB, bStatus & 0x01);

	return;
}

/****************************************************************************
 *
 * NAME: cbToCoNet_vHwEvent
 *
 * DESCRIPTION:
 * Process any hardware events.
 *
 * PARAMETERS:      Name            RW  Usage
 *                  u32DeviceId
 *                  u32ItemBitmap
 *
 * RETURNS:
 * None.
 *
 * NOTES:
 * None.
 ****************************************************************************/
void cbToCoNet_vHwEvent(uint32 u32DeviceId, uint32 u32ItemBitmap)
{
    switch (u32DeviceId) {
	case E_AHI_DEVICE_ANALOGUE:
		u16ADC_ReadReg(&sAppData.sObjADC);
		break;

	case E_AHI_DEVICE_TICK_TIMER:
		if (sAppData.u8AdcState != E_ADC_INIT) {
			switch (sAppData.u8AdcState) {
			case E_ADC_START:
				vSnsObj_Process(&sAppData.sADC, E_ORDER_KICK);
				sAppData.u8AdcState = E_ADC_READY;
				break;

			case E_ADC_READY:
				vSnsObj_Process(&sAppData.sADC, E_ORDER_KICK);
				if (bSnsObj_isComplete(&sAppData.sADC)) {
					sAppData.ai16Volt = sAppData.sObjADC.ai16Result[TEH_ADC_IDX_VOLT];
					sAppData.ai16Adc1 = sAppData.sObjADC.ai16Result[TEH_ADC_IDX_ADC_1];
					sAppData.ai16Adc3 = sAppData.sObjADC.ai16Result[TEH_ADC_IDX_ADC_3];
					sAppData.u8AdcState = E_ADC_COMPLETE;
					vfPrintf(&sSerStream, LB "ADC Read Complete." LB);
				}
				break;

			case E_ADC_COMPLETE:
				break;

			default:
				break;
			}
		}
		break;

    default:
    	break;
    }
}

/****************************************************************************
 *
 * NAME: cbToCoNet_u8HwInt
 *
 * DESCRIPTION:
 *   called during an interrupt
 *
 * PARAMETERS:      Name            RW  Usage
 *                  u32DeviceId
 *                  u32ItemBitmap
 *
 * RETURNS:
 *                  FALSE -  interrupt is not handled, escalated to further
 *                           event call (cbToCoNet_vHwEvent).
 *                  TRUE  -  interrupt is handled, no further call.
 *
 * NOTES:
 *   Do not put a big job here.
 ****************************************************************************/
uint8 cbToCoNet_u8HwInt(uint32 u32DeviceId, uint32 u32ItemBitmap) {
	return FALSE;
}

/****************************************************************************
 *
 * NAME: vInitHardware
 *
 * DESCRIPTION:
 *
 * RETURNS:
 *
 ****************************************************************************/
static void vInitHardware(int f_warm_start)
{
	// Serial Initialize
	vSerialInit(UART_BAUD, NULL);

	ToCoNet_vDebugInit(&sSerStream);
	ToCoNet_vDebugLevel(0);

	// IOs
	vPortDisablePullup(PORT_RESET);
	vPortDisablePullup(PORT_SET);
	vPortDisablePullup(PORT_LED);
	vPortSetHi(PORT_RESET);
	vPortSetHi(PORT_SET);
	vPortSetHi(PORT_LED);
	vPortAsOutput(PORT_RESET);
	vPortAsOutput(PORT_SET);
	vPortAsOutput(PORT_LED);
	vPortAsInput(PORT_INPUT1);
	vPortAsInput(PORT_INPUT2);
	vPortAsInput(PORT_INPUT3);
	vPortAsInput(PORT_INPUT4);

	// ADC関係のデータを初期化する
	vADC_Init(&sAppData.sObjADC, &sAppData.sADC, TRUE);
	sAppData.sObjADC.u8SourceMask = TEH_ADC_SRC_VOLT | TEH_ADC_SRC_ADC_1 | TEH_ADC_SRC_ADC_3;
	sAppData.u8AdcState = E_ADC_START;

	vfPrintf(&sSerStream, LB "Initialize Hardware complete.");
}

/****************************************************************************
 *
 * NAME: vSerialInit
 *
 * DESCRIPTION:
 *
 * RETURNS:
 *
 ****************************************************************************/
void vSerialInit(uint32 u32Baud, tsUartOpt *pUartOpt) {
	/* Create the debug port transmit and receive queues */
	static uint8 au8SerialTxBuffer[96];
	static uint8 au8SerialRxBuffer[32];

	/* Initialise the serial port to be used for debug output */
	sSerPort.pu8SerialRxQueueBuffer = au8SerialRxBuffer;
	sSerPort.pu8SerialTxQueueBuffer = au8SerialTxBuffer;
	sSerPort.u32BaudRate = u32Baud;
	sSerPort.u16AHI_UART_RTS_LOW = 0xffff;
	sSerPort.u16AHI_UART_RTS_HIGH = 0xffff;
	sSerPort.u16SerialRxQueueSize = sizeof(au8SerialRxBuffer);
	sSerPort.u16SerialTxQueueSize = sizeof(au8SerialTxBuffer);
	sSerPort.u8SerialPort = UART_PORT_SLAVE;
	sSerPort.u8RX_FIFO_LEVEL = E_AHI_UART_FIFO_LEVEL_1;
	SERIAL_vInitEx(&sSerPort, pUartOpt);

	sSerStream.bPutChar = SERIAL_bTxChar;
	sSerStream.u8Device = UART_PORT_SLAVE;
}

/****************************************************************************
 *
 * NAME: vHandleSerialInput
 *
 * DESCRIPTION:
 *
 * PARAMETERS:      Name            RW  Usage
 *
 * RETURNS:
 *
 * NOTES:
 ****************************************************************************/
static void vHandleSerialInput(void)
{
    // handle UART command
	while (!SERIAL_bRxQueueEmpty(sSerPort.u8SerialPort)) {
		int16 i16Char;

		i16Char = SERIAL_i16RxChar(sSerPort.u8SerialPort);

		vfPrintf(&sSerStream, "\n\r# [%c] --> ", i16Char);
	    SERIAL_vFlush(sSerStream.u8Device);

		if (i16Char == ':') {
			sAppData.bCommandInput = TRUE;
			sAppData.u32SleepCountDown = SLEEP_COUNT_KEYPRESSED;
			vfPrintf(&sSerStream, "Enter Command [sSrRlL]");
		} else if (sAppData.bCommandInput) {
			sAppData.bCommandInput = FALSE;
			switch(i16Char) {
			case 'd': case 'D':
				_C {
					static uint8 u8DgbLvl;

					u8DgbLvl++;
					if(u8DgbLvl > 5) u8DgbLvl = 0;
					ToCoNet_vDebugLevel(u8DgbLvl);

					vfPrintf(&sSerStream, "set NwkCode debug level to %d.", u8DgbLvl);
				}
				break;

			case 'a':
				sAppData.u8Command = E_TWPOWER_COMMAND_AUTOOFF;
				sAppData.u32SleepCountDown = AUTOOFF_COUNT;
				vfPrintf(&sSerStream, LB "Auto Off Start");
				break;
			case 's':
				vPortSetHi(PORT_SET);
				vfPrintf(&sSerStream, LB "Set High PORT_SET.");
				break;

			case 'S':
				vPortSetLo(PORT_SET);
				vfPrintf(&sSerStream, LB "Set Low PORT_SET.");
				break;

			case 'r':
				vPortSetHi(PORT_RESET);
				vfPrintf(&sSerStream, LB "Set High PORT_RESET.");
				break;

			case 'R':
				vPortSetLo(PORT_RESET);
				vfPrintf(&sSerStream, LB "Set Low PORT_RESET.");
				break;

			case 'l':
				vPortSetHi(PORT_LED);
				vfPrintf(&sSerStream, LB "Set High PORT_LED.");
				break;

			case 'L':
				vPortSetLo(PORT_LED);
				vfPrintf(&sSerStream, LB "Set Low PORT_LED.");
				break;

			default:
				vfPrintf(&sSerStream, "Invalid Command");
				break;
			}
		} else {
			vfPrintf(&sSerStream, "Invalid Command");
		}

		vfPrintf(&sSerStream, LB);
	    SERIAL_vFlush(sSerStream.u8Device);
	}
}

/****************************************************************************
 *
 * NAME: vProcessEvent
 *
 * DESCRIPTION:
 *
 * RETURNS:
 *
 ****************************************************************************/
static void vProcessEvCore(tsEvent *pEv, teEvent eEvent, uint32 u32evarg) {
	if (eEvent == E_EVENT_START_UP) {
		// ここで UART のメッセージを出力すれば安全である。
		if (u32evarg & EVARG_START_UP_WAKEUP_RAMHOLD_MASK) {
			vfPrintf(&sSerStream, LB "RAMHOLD" LB);
		}
	    if (u32evarg & EVARG_START_UP_WAKEUP_MASK) {
			vfPrintf(&sSerStream, LB "Wake up by %s." LB,
					bWakeupByButton ? "UART PORT" : "WAKE TIMER");
	    } else {
	    	vfPrintf(&sSerStream, LB "*** TWEPOWER %d.%02d-%d ***" LB, VERSION_MAIN, VERSION_SUB, VERSION_VAR);
	    	vfPrintf(&sSerStream, "*** %08x ***" LB, ToCoNet_u32GetSerial());
	    }
	}
}

/****************************************************************************
 *
 * NAME: vShowMessage
 *
 * DESCRIPTION:
 *   Put message text to serial console.
 *
 * PARAMETERS:      Name            RW  Usage
 *                  buf				R	pointer to message text
 *                  size			R	size of message text
 *
 * RETURNS:
 *
 ****************************************************************************/
static void vShowMessage(uint8 *buf, int size)
{
	int i, c;

	for (i = 0; i < size; i++) {
		c = *buf++;
		if (i < 32) {
			sSerStream.bPutChar(sSerStream.u8Device,
					(c >= 0x20 && c <= 0x7f) ? c : '.');
		} else {
			vfPrintf(&sSerStream, "..");
			break;
		}
	}
}

/****************************************************************************
 *
 * NAME: vBroadcastStatus
 *
 * DESCRIPTION:
 *
 * RETURNS:
 *
 ****************************************************************************/
static void vBroadcastStatus(void) {
	tsTxDataApp tsTx;

	vfPrintf(&sSerStream, "%d %d %d" LB, sAppData.ai16Volt, sAppData.ai16Adc1, sAppData.ai16Adc3);

	memset(&tsTx, 0, sizeof(tsTxDataApp));

	sAppData.u32Seq ++;

	tsTx.u32SrcAddr = ToCoNet_u32GetSerial(); // 自身のアドレス
	tsTx.u32DstAddr = 0xFFFF; // ブロードキャスト

	tsTx.bAckReq = FALSE;
	tsTx.u8Retry = 0x82; // ブロードキャストで都合３回送る
	tsTx.u8CbId = sAppData.u32Seq & 0xFF;
	tsTx.u8Seq = sAppData.u32Seq & 0xFF;
	tsTx.u8Cmd = TOCONET_PACKET_CMD_APP_DATA;

	// ヘッダー部
	memcpy(tsTx.auData, TWPOWER_HEADER, TWPOWER_HEADER_SIZE);
	// データ部
	memcpy(&tsTx.auData[TWPOWER_HEADER_SIZE], TWPOWER_CMD_STATUS, TWPOWER_CMD_SIZE);
	vPutHexWord(&tsTx.auData[TWPOWER_HEADER_SIZE + TWPOWER_ADCVOLT_POS], sAppData.ai16Volt);
	vPutHexWord(&tsTx.auData[TWPOWER_HEADER_SIZE + TWPOWER_ADC1_POS], sAppData.ai16Adc1);
	vPutHexWord(&tsTx.auData[TWPOWER_HEADER_SIZE + TWPOWER_ADC3_POS], sAppData.ai16Adc3);
	tsTx.auData[TWPOWER_HEADER_SIZE + TWPOWER_INPUT1_POS] = bPortRead(PORT_INPUT1) ? '1' : '0';
	tsTx.auData[TWPOWER_HEADER_SIZE + TWPOWER_INPUT2_POS] = bPortRead(PORT_INPUT2) ? '1' : '0';
	tsTx.auData[TWPOWER_HEADER_SIZE + TWPOWER_INPUT3_POS] = bPortRead(PORT_INPUT3) ? '1' : '0';
	tsTx.auData[TWPOWER_HEADER_SIZE + TWPOWER_INPUT4_POS] = bPortRead(PORT_INPUT4) ? '1' : '0';
	tsTx.auData[TWPOWER_HEADER_SIZE + TWPOWER_STATUS_SIZE] = '\n';
	tsTx.auData[TWPOWER_HEADER_SIZE + TWPOWER_STATUS_SIZE + 1] = '\0';
	int size = TWPOWER_STATUS_SIZE + 1;
	// データ部CRC
	uint8 u8crc = u8CCITT8(&tsTx.auData[TWPOWER_HEADER_SIZE], size);
	vPutHexByte(&tsTx.auData[TWPOWER_CRC_POS], u8crc);
	// データ部サイズ
	vPutHexByte(&tsTx.auData[TWPOWER_LEN_POS], size);

	// ペイロード長設定
	tsTx.u8Len = TWPOWER_HEADER_SIZE + size;

	// 送信
	ToCoNet_bMacTxReq(&tsTx);

	vfPrintf(&sSerStream, LB "Send Broadcast(%d): <", tsTx.u8Len);
	vShowMessage(tsTx.auData, tsTx.u8Len);
	vfPrintf(&sSerStream, ">" LB);
	SERIAL_vFlush(sSerStream.u8Device);
}

/****************************************************************************
 *
 * NAME: vSendCommand
 *
 * DESCRIPTION:
 *
 * RETURNS:
 *
 ****************************************************************************/
static void vSendCommand(char *buf, int size)
{
	tsTxDataApp tsTx;
	memset(&tsTx, 0, sizeof(tsTxDataApp));

	sAppData.u32Seq ++;

	tsTx.u32SrcAddr = ToCoNet_u32GetSerial(); // 自身のアドレス
	tsTx.u32DstAddr = 0xFFFF; // ブロードキャスト
	tsTx.bAckReq = FALSE;
	tsTx.u8Retry = 0x82; // ブロードキャストで都合３回送る
	tsTx.u8CbId = sAppData.u32Seq & 0xFF;
	tsTx.u8Seq = sAppData.u32Seq & 0xFF;
	tsTx.u8Cmd = TOCONET_PACKET_CMD_APP_DATA;

	// ヘッダー部
	memcpy(tsTx.auData, TWPOWER_HEADER, TWPOWER_HEADER_SIZE);
	// データ部
	memcpy(&tsTx.auData[TWPOWER_HEADER_SIZE], buf, size);
	// データ部CRC
	uint8 u8crc = u8CCITT8(&tsTx.auData[TWPOWER_HEADER_SIZE], size);
	vPutHexByte(&tsTx.auData[TWPOWER_CRC_POS], u8crc);
	// データ部サイズ
	vPutHexByte(&tsTx.auData[TWPOWER_LEN_POS], size);

	// ペイロード長設定
	tsTx.u8Len = TWPOWER_HEADER_SIZE + size;

	// 送信
	ToCoNet_bMacTxReq(&tsTx);

	vfPrintf(&sSerStream, LB "Send (%d): <", tsTx.u8Len);
	vShowMessage(tsTx.auData, tsTx.u8Len);
	vfPrintf(&sSerStream, ">" LB);
	SERIAL_vFlush(sSerStream.u8Device);
}

/****************************************************************************
 *
 * NAME: vSleepSec()
 *
 * DESCRIPTION:
 *
 * RETURNS:
 *
 ****************************************************************************/
static void vSleepSec(int sec)
{
	vfPrintf(&sSerStream, "now sleep for %d seconds." LB, sec);
	SERIAL_vFlush(sSerStream.u8Device); // flushing

	vAHI_UartDisable(sSerStream.u8Device);
	vAHI_DioSetDirection(u32DioPortWakeUp, 0); // set as input
	(void)u32AHI_DioInterruptStatus(); // clear interrupt register
	vAHI_DioWakeEnable(u32DioPortWakeUp, 0); // also use as DIO WAKE SOURCE
	vAHI_DioWakeEdge(u32DioPortWakeUp, 0); // 割り込みエッジ（立上がりに設定）

	ToCoNet_vSleep(E_AHI_WAKE_TIMER_0, sec * 1000, FALSE, TRUE); // RAM ON SLEEP USING WK0
}

/****************************************************************************
 *
 * NAME: vProcessIncomingData()
 *
 * DESCRIPTION:
 *
 * RETURNS:
 *
 ****************************************************************************/
static void vProcessIncomingData(tsRxDataApp *pRx)
{
	uint8 cmd[TWPOWER_CMD_SIZE+1] = "XX";

	memcpy(cmd, &pRx->auData[TWPOWER_CMD_POS], TWPOWER_CMD_SIZE);
	cmd[TWPOWER_CMD_SIZE] = 0;

	if (!memcmp(cmd, TWPOWER_CMD_ON, TWPOWER_CMD_SIZE)) {
		vfPrintf(&sSerStream, LB "On Received" LB);
		sAppData.u8Command = E_TWPOWER_COMMAND_ON;
		sAppData.u32SleepCountDown = 10;
	} else if (!memcmp(cmd, TWPOWER_CMD_OFF, TWPOWER_CMD_SIZE)) {
		vfPrintf(&sSerStream, LB "Off Received" LB);
		sAppData.u8Command = E_TWPOWER_COMMAND_OFF;
		sAppData.u32SleepCountDown = 10;
	} else if (!memcmp(cmd, TWPOWER_CMD_LED, TWPOWER_CMD_SIZE)) {
		vfPrintf(&sSerStream, LB "Led Received" LB);
		vPortSetLo(PORT_LED);
		sAppData.u32SleepCountDown = 500;
	} else if (!memcmp(cmd, TWPOWER_CMD_AUTOOFF, TWPOWER_CMD_SIZE)) {
		sAppData.u8Command = E_TWPOWER_COMMAND_AUTOOFF;
		sAppData.u32SleepCountDown = AUTOOFF_COUNT;
		vfPrintf(&sSerStream, LB "Auto Off Start");
	} else {
		vfPrintf(&sSerStream, LB "Invalid Command: %s" LB, cmd);
	}
}

/****************************************************************************/
/***        END OF FILE                                                   ***/
/****************************************************************************/
