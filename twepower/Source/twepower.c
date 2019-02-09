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

#include "twepower.h"
#include "config.h"
#include "common.h"
#include "Version.h"

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

    // LED Counter
    uint32 u32LedCt;

    // シーケンス番号
    uint32 u32Seq;

    // スリープカウンタ
    uint8 u8SleepCt;

	//
	uint32 u32Counter;
} tsAppData;


/****************************************************************************/
/***        Local Function Prototypes                                     ***/
/****************************************************************************/

static void vProcessEvCore(tsEvent *pEv, teEvent eEvent, uint32 u32evarg);

static void vInitHardware(int f_warm_start);

void vSerialInit(uint32 u32Baud, tsUartOpt *pUartOpt);
static void vHandleSerialInput(void);

static void vBroadcastStatus(void);
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

	sAppData.u32Counter ++;
	if (sAppData.u32Counter == 1) {
		vBroadcastStatus();
	} else if (sAppData.u32Counter > 1000) {
		vSleepSec(10);
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
	int i;
	static uint16 u16seqPrev = 0xFFFF;
	//uint8 *p = pRx->auData;

	// print coming payload
	vfPrintf(&sSerStream, LB"[PKT Ad:%04x,Ln:%03d,Seq:%03d,Lq:%03d,Tms:%05d \"",
			pRx->u32SrcAddr,
			pRx->u8Len+4, // Actual payload byte: the network layer uses additional 4 bytes.
			pRx->u8Seq,
			pRx->u8Lqi,
			pRx->u32Tick & 0xFFFF);
	for (i = 0; i < pRx->u8Len; i++) {
		if (i < 32) {
			sSerStream.bPutChar(sSerStream.u8Device,
					(pRx->auData[i] >= 0x20 && pRx->auData[i] <= 0x7f) ? pRx->auData[i] : '.');
		} else {
			vfPrintf(&sSerStream, "..");
			break;
		}
	}
	vfPrintf(&sSerStream, "C\"]");

	// 打ち返す
	if (    pRx->u8Seq != u16seqPrev // シーケンス番号による重複チェック
		&& !memcmp(pRx->auData, "PING:", 5) // パケットの先頭は PING: の場合
	) {
		u16seqPrev = pRx->u8Seq;
		vProcessIncomingData(pRx);
	} else if (!memcmp(pRx->auData, "PONG:", 5)) {
		// ＵＡＲＴに出力
		vfPrintf(&sSerStream, LB "PONG Message from %08x" LB, pRx->u32SrcAddr);
	} else {
		vfPrintf(&sSerStream, LB "Other Message" LB);
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
    case E_AHI_DEVICE_TICK_TIMER:
		// LED BLINK
   		vPortSet_TrueAsLo(PORT_KIT_LED2, u32TickCount_ms & 0x400);

   		// LED ON when receive
   		if (u32TickCount_ms - sAppData.u32LedCt < 300) {
   			vPortSetLo(PORT_KIT_LED1);
   		} else {
  			vPortSetHi(PORT_KIT_LED1);
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

	/// IOs
	vPortDisablePullup(PORT_RESET);
	vPortDisablePullup(PORT_SET);
	vPortDisablePullup(PORT_LED);
	vPortSetHi(PORT_RESET);
	vPortSetHi(PORT_SET);
	vPortSetHi(PORT_LED);
	vPortAsOutput(PORT_RESET);
	vPortAsOutput(PORT_SET);
	vPortAsOutput(PORT_LED);
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

		switch(i16Char) {

		case '>': case '.':
			/* channel up */
			sAppData.u8channel++;
			if (sAppData.u8channel > 26) sAppData.u8channel = 11;
			sToCoNet_AppContext.u8Channel = sAppData.u8channel;
			ToCoNet_vRfConfig();
			vfPrintf(&sSerStream, "set channel to %d.", sAppData.u8channel);
			break;

		case '<': case ',':
			/* channel down */
			sAppData.u8channel--;
			if (sAppData.u8channel < 11) sAppData.u8channel = 26;
			sToCoNet_AppContext.u8Channel = sAppData.u8channel;
			ToCoNet_vRfConfig();
			vfPrintf(&sSerStream, "set channel to %d.", sAppData.u8channel);
			break;

		case 'd': case 'D':
			_C {
				static uint8 u8DgbLvl;

				u8DgbLvl++;
				if(u8DgbLvl > 5) u8DgbLvl = 0;
				ToCoNet_vDebugLevel(u8DgbLvl);

				vfPrintf(&sSerStream, "set NwkCode debug level to %d.", u8DgbLvl);
			}
			break;

		case 's':
			vPortSetHi(PORT_SET);
			break;

		case 'S':
			vPortSetLo(PORT_SET);
			break;

		case 'r':
			vPortSetHi(PORT_RESET);
			break;

		case 'R':
			vPortSetLo(PORT_RESET);
			break;

		case 'l':
			vPortSetHi(PORT_LED);
			break;

		case 'L':
			vPortSetLo(PORT_LED);
			break;

		case 'p':
			// 出力調整のテスト
			_C {
				static uint8 u8pow = 3; // (MIN)0..3(MAX)

				u8pow = (u8pow + 1) % 4;
				vfPrintf(&sSerStream, "set power to %d.", u8pow);

				sToCoNet_AppContext.u8TxPower = u8pow;
				ToCoNet_vRfConfig();
			}
			break;

		case 't': // パケット送信してみる
			_C {
				// transmit Ack back
				tsTxDataApp tsTx;
				memset(&tsTx, 0, sizeof(tsTxDataApp));

				sAppData.u32Seq++;

				tsTx.u32SrcAddr = ToCoNet_u32GetSerial(); // 自身のアドレス
				tsTx.u32DstAddr = 0xFFFF; // ブロードキャスト

				tsTx.bAckReq = FALSE;
				tsTx.u8Retry = 0x82; // ブロードキャストで都合３回送る
				tsTx.u8CbId = sAppData.u32Seq & 0xFF;
				tsTx.u8Seq = sAppData.u32Seq & 0xFF;
				tsTx.u8Cmd = TOCONET_PACKET_CMD_APP_DATA;

				// SPRINTF でメッセージを作成
				SPRINTF_vRewind();
				vfPrintf(SPRINTF_Stream, "PING: %08X", ToCoNet_u32GetSerial());
				memcpy(tsTx.auData, SPRINTF_pu8GetBuff(), SPRINTF_u16Length());
				tsTx.u8Len = SPRINTF_u16Length();

				// 送信
				ToCoNet_bMacTxReq(&tsTx);

				// LEDの制御
				sAppData.u32LedCt = u32TickCount_ms;

				// ＵＡＲＴに出力
				vfPrintf(&sSerStream, LB "Fire PING Broadcast Message.");
			}
			break;

		default:
			break;
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
			vfPrintf(&sSerStream, LB "RAMHOLD");
		}
	    if (u32evarg & EVARG_START_UP_WAKEUP_MASK) {
			vfPrintf(&sSerStream, LB "Wake up by %s. SleepCt=%d",
					bWakeupByButton ? "UART PORT" : "WAKE TIMER",
					sAppData.u8SleepCt);
	    } else {
	    	vfPrintf(&sSerStream, "\r\n*** TWELITE NET PINGPONG SAMPLE %d.%02d-%d ***", VERSION_MAIN, VERSION_SUB, VERSION_VAR);
	    	vfPrintf(&sSerStream, "\r\n*** %08x ***", ToCoNet_u32GetSerial());
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

	vfPrintf(&sSerStream, LB "Send PING" LB);
	SERIAL_vFlush(sSerStream.u8Device);

	memset(&tsTx, 0, sizeof(tsTxDataApp));

	sAppData.u32Seq = ToCoNet_u32GetRand();

	tsTx.u32SrcAddr = ToCoNet_u32GetSerial(); // 自身のアドレス
	tsTx.u32DstAddr = 0xFFFF; // ブロードキャスト

	tsTx.bAckReq = FALSE;
	tsTx.u8Retry = 0x82; // ブロードキャストで都合３回送る
	tsTx.u8CbId = sAppData.u32Seq & 0xFF;
	tsTx.u8Seq = sAppData.u32Seq & 0xFF;
	tsTx.u8Cmd = TOCONET_PACKET_CMD_APP_DATA;

	// SPRINTF でメッセージを作成
	SPRINTF_vRewind();
	vfPrintf(SPRINTF_Stream, "PING: %08X", ToCoNet_u32GetSerial());
	memcpy(tsTx.auData, SPRINTF_pu8GetBuff(), SPRINTF_u16Length());
	tsTx.u8Len = SPRINTF_u16Length();

	// 送信
	ToCoNet_bMacTxReq(&tsTx);
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
 * NAME: vSleepSec()
 *
 * DESCRIPTION:
 *
 * RETURNS:
 *
 ****************************************************************************/
static void vProcessIncomingData(tsRxDataApp *pRx)
{
	// transmit Ack back
	tsTxDataApp tsTx;
	memset(&tsTx, 0, sizeof(tsTxDataApp));

	tsTx.u32SrcAddr = ToCoNet_u32GetSerial(); //
	tsTx.u32DstAddr = pRx->u32SrcAddr; // 送り返す
	tsTx.u32DstAddr = 0xFFFF; // ブロードキャスト

	tsTx.bAckReq = TRUE;
	tsTx.u8Retry = 0x81;
	tsTx.u8CbId = pRx->u8Seq;
	tsTx.u8Seq = pRx->u8Seq;
	tsTx.u8Len = pRx->u8Len;
	tsTx.u8Cmd = TOCONET_PACKET_CMD_APP_DATA;

	if (tsTx.u8Len > 0) {
		memcpy(tsTx.auData, pRx->auData, tsTx.u8Len);
	}
	tsTx.auData[1] = 'O'; // メッセージを PONG に書き換える

	ToCoNet_bMacTxReq(&tsTx);

	// turn on Led a while
	sAppData.u32LedCt = u32TickCount_ms;

	// ＵＡＲＴに出力
	vfPrintf(&sSerStream, LB "Fire PONG Message to %08x" LB, pRx->u32SrcAddr);
}

/****************************************************************************/
/***        END OF FILE                                                   ***/
/****************************************************************************/
