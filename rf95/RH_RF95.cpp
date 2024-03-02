// RH_RF95.cpp
//
// Copyright (C) 2011 Mike McCauley
// $Id: RH_RF95.cpp,v 1.18 2018/01/06 23:50:45 mikem Exp mikem $
#include "ch.hpp"
#include "hal.h"
using namespace chibios_rt;

#include <RH_RF95.h>
#include <string.h>
#include "chprintf.h"
//#include "lcd.h"


#define STM32_EXTLINES_PRIORITY     0x6


/// The index into _deviceForInterrupt[] for this device (if an interrupt is already allocated)
/// else 0xff
uint8_t             _myInterruptIndex;

/// Number of octets in the buffer
volatile uint8_t    _bufLen;

/// The receiver/transmitter buffer
uint8_t             _buf[RH_RF95_MAX_PAYLOAD_LEN];

/// True when there is a valid message in the buffer
volatile bool       _rxBufValid;

// True if we are using the HF port (779.0 MHz and above)
bool                _usingHFport;

// Last measured SNR, dB
int8_t              _lastSNR;

/// The current transport operating mode
volatile RHMode     _mode;

/// This node id
uint8_t             _thisAddress;

/// Whether the transport is in promiscuous mode
bool                _promiscuous;

/// TO header in the last received mesasge
volatile uint8_t    _rxHeaderTo;

/// FROM header in the last received mesasge
volatile uint8_t    _rxHeaderFrom;

/// ID header in the last received mesasge
volatile uint8_t    _rxHeaderId;

/// FLAGS header in the last received mesasge
volatile uint8_t    _rxHeaderFlags;

/// TO header to send in all messages
uint8_t             _txHeaderTo;

/// FROM header to send in all messages
uint8_t             _txHeaderFrom;

/// ID header to send in all messages
uint8_t             _txHeaderId;

/// FLAGS header to send in all messages
uint8_t             _txHeaderFlags;

/// The value of the last received RSSI value, in some transport specific units
volatile int16_t     _lastRssi;

/// Count of the number of bad messages (eg bad checksum etc) received
volatile uint16_t   _rxBad;

/// Count of the number of successfully transmitted messaged
volatile uint16_t   _rxGood;

/// Count of the number of bad messages (correct checksum etc) received
volatile uint16_t   _txGood;

/// Channel activity detected
volatile bool       _cad;

/// Channel activity timeout in ms
volatile unsigned int        _cad_timeout;

static uint8_t txbuf[SPI_BUFFERS_SIZE];
static uint8_t rxbuf[SPI_BUFFERS_SIZE];

// Interrupt vectors for the 3 Arduino interrupt pins
// Each interrupt can be handled by a different instance of RH_RF95, allowing you to have
// 2 or more LORAs per Arduino
//RH_RF95* RH_RF95::_deviceForInterrupt[RH_RF95_NUM_INTERRUPTS] = {0, 0, 0};
//uint8_t RH_RF95::_interruptCount = 0; // Index into _deviceForInterrupt for next device

// These are indexed by the values of ModemConfigChoice
// Stored in flash (program) memory to save SRAM
static const uint8_t MODEM_CONFIG_TABLE[4][3] =
{
		//  1d,     1e,      26
		{ 0x72,   0x74,    0x04}, // Bw125Cr45Sf128 (the chip default), AGC enabled
		{ 0x92,   0x74,    0x04}, // Bw500Cr45Sf128, AGC enabled
		{ 0x48,   0x94,    0x04}, // Bw31_25Cr48Sf512, AGC enabled
		{ 0x78,   0xc4,    0x0c}, // Bw125Cr48Sf4096, AGC enabled

};


// lee valor de un registro
uint8_t RHSPIDriver_spiRead(uint8_t reg)
{
	uint8_t txbf[2], rxbf[2];
	spiAcquireBus(&SPID1);
	txbf[0] = reg & ~RH_SPI_WRITE_MASK;
	spiSelect(&SPID1);
	spiExchange(&SPID1, 2, txbf, rxbf);
	spiUnselect(&SPID1);
	spiReleaseBus(&SPID1);
	return rxbf[1];
}

// escribe un registro
uint8_t RHSPIDriver_spiWrite(uint8_t reg, uint8_t val)
{
	uint8_t txbf[2], rxbf[2];
	spiAcquireBus(&SPID1);
	txbf[0] = reg | RH_SPI_WRITE_MASK;
	txbf[1] = val;
	spiSelect(&SPID1);
	spiExchange(&SPID1, 2, txbf, rxbf);
	spiUnselect(&SPID1);
	spiReleaseBus(&SPID1);
	return TRUE;
}

uint8_t RHSPIDriver_spiBurstRead(uint8_t reg, uint8_t* dest, uint8_t len)
{
	spiAcquireBus(&SPID1);
	txbuf[0] = reg & ~RH_SPI_WRITE_MASK;
	spiSelect(&SPID1);
	spiExchange(&SPID1, 1, txbuf, rxbuf);
	spiExchange(&SPID1, len, txbuf, dest);
	spiUnselect(&SPID1);
	spiReleaseBus(&SPID1);
	return TRUE;
}

uint8_t RHSPIDriver_spiBurstWrite(uint8_t reg, const uint8_t* src, uint8_t len)
{
	spiAcquireBus(&SPID1);
	txbuf[0] = reg | RH_SPI_WRITE_MASK;
	spiSelect(&SPID1);
	spiExchange(&SPID1, 1, txbuf, rxbuf);
	spiExchange(&SPID1, len, src, rxbuf);
	spiUnselect(&SPID1);
	spiReleaseBus(&SPID1);
	return TRUE;
}

bool RH_RF95_init(void)
{
    _thisAddress = RH_BROADCAST_ADDRESS;
    _txHeaderTo = RH_BROADCAST_ADDRESS;
    _txHeaderFrom = RH_BROADCAST_ADDRESS;
    _txHeaderId = 0;
    _txHeaderFlags = 0;
    _rxBad = 0;
    _rxGood = 0;
    _txGood = 0;
    _cad_timeout = 0;

	osalThreadSleepMilliseconds(10); // 10 ms para inicialización
	_rxBufValid = 0; // estaba definido en el constructor
	// Set sleep mode, so we can also set LORA mode:
	RHSPIDriver_spiWrite(RH_RF95_REG_01_OP_MODE, RH_RF95_MODE_SLEEP | RH_RF95_LONG_RANGE_MODE);
	osalThreadSleepMilliseconds(10);//delay(10); // Wait for sleep mode to take over from say, CAD
	// Check we are in sleep mode, with LORA set
	if (RHSPIDriver_spiRead(RH_RF95_REG_01_OP_MODE) != (RH_RF95_MODE_SLEEP | RH_RF95_LONG_RANGE_MODE))
	{
		//	Serial.println(RHSPIDriver_spiRead(RH_RF95_REG_01_OP_MODE), HEX);
		return false; // No device present?
	}


	// Set up FIFO
	// We configure so that we can use the entire 256 byte FIFO for either receive
	// or transmit, but not both at the same time
	RHSPIDriver_spiWrite(RH_RF95_REG_0E_FIFO_TX_BASE_ADDR, 0);
	RHSPIDriver_spiWrite(RH_RF95_REG_0F_FIFO_RX_BASE_ADDR, 0);

	// Packet format is preamble + explicit-header + payload + crc
	// Explicit Header Mode
	// payload is TO + FROM + ID + FLAGS + message data
	// RX mode is implmented with RXCONTINUOUS
	// max message data length is 255 - 4 = 251 octets

	RH_RF95_setModeIdle();

	// Set up default configuration
	// No Sync Words in LORA mode.
	RH_RF95_setModemConfig(Bw125Cr45Sf128); // Radio default
	//    setModemConfig(Bw125Cr48Sf4096); // slow and reliable?
	RH_RF95_setPreambleLength(8); // Default is 8
	// An innocuous ISM frequency, same as RF22's
	RH_RF95_setFrequency(434.0);
	// Lowish power
	//RH_RF95_setTxPower(13,TRUE);
	RH_RF95_setTxPower(25,FALSE);

	return true;
}





// C++ level interrupt handler for this instance
// LORA is unusual in that it has several interrupt lines, and not a single, combined one.
// On MiniWirelessLoRa, only one of the several interrupt lines (DI0) from the RFM95 is usefuly
// connnected to the processor.
// We use this to get RxDone and TxDone interrupts
paquete_t RH_RF95_handleInterrupt(void)
{
	paquete_t tipoPaq;//=99;
	// Read the interrupt register
	/*
	 * In order to retrieve received data from FIFO the user must ensure that ValidHeader, PayloadCrcError, RxDone and
RxTimeout interrupts in the status register RegIrqFlags are not asserted to ensure that packet reception has terminated
successfully (i.e. no flags should be set).
	 */
	uint8_t irq_flags = RHSPIDriver_spiRead(RH_RF95_REG_12_IRQ_FLAGS);
	tipoPaq = paqUknown; // por ejemplo
	if (_mode == RHModeRx && irq_flags & (RH_RF95_RX_TIMEOUT | RH_RF95_PAYLOAD_CRC_ERROR))
	{
		_rxBad++;
		tipoPaq = paqRxErr;
	}
	else if (_mode == RHModeRx && irq_flags & RH_RF95_RX_DONE)
	{
		// Have received a packet
		uint8_t len = RHSPIDriver_spiRead(RH_RF95_REG_13_RX_NB_BYTES);

		// Reset the fifo read ptr to the beginning of the packet
		RHSPIDriver_spiWrite(RH_RF95_REG_0D_FIFO_ADDR_PTR, RHSPIDriver_spiRead(RH_RF95_REG_10_FIFO_RX_CURRENT_ADDR));
		RHSPIDriver_spiBurstRead(RH_RF95_REG_00_FIFO, _buf, len);
		_bufLen = len;
		RHSPIDriver_spiWrite(RH_RF95_REG_12_IRQ_FLAGS, 0xff); // Clear all IRQ flags

		// Remember the last signal to noise ratio, LORA mode
		// Per page 111, SX1276/77/78/79 datasheet
		_lastSNR = (int8_t)RHSPIDriver_spiRead(RH_RF95_REG_19_PKT_SNR_VALUE) / 4;

		// Remember the RSSI of this packet, LORA mode
		// this is according to the doc, but is it really correct?
		// weakest receiveable signals are reported RSSI at about -66
		_lastRssi = RHSPIDriver_spiRead(RH_RF95_REG_1A_PKT_RSSI_VALUE);
		// Adjust the RSSI, datasheet page 87
		if (_lastSNR < 0)
			_lastRssi = _lastRssi + _lastSNR;
		else
			_lastRssi = (int)_lastRssi * 16 / 15;
		if (_usingHFport)
			_lastRssi -= 157;
		else
			_lastRssi -= 164;

		// We have received a message.
		RH_RF95_validateRxBuf();
		if (_rxBufValid)
		{
			RH_RF95_setModeIdle(); // Got one
			tipoPaq = paqRx;
	    }
		else
            tipoPaq = paqRxErr;
	}
	else if (_mode == RHModeTx && irq_flags & RH_RF95_TX_DONE)
	{
		_txGood++;
		RH_RF95_setModeIdle();
		tipoPaq = paqTx;
	}
	else if (_mode == RHModeCad && irq_flags & RH_RF95_CAD_DONE)
	{
		_cad = irq_flags & RH_RF95_CAD_DETECTED;
		RH_RF95_setModeIdle();
		tipoPaq = paqCAD;
	}
	// Sigh: on some processors, for some unknown reason, doing this only once does not actually
	// clear the radio's interrupt flag. So we do it twice. Why?
	RHSPIDriver_spiWrite(RH_RF95_REG_12_IRQ_FLAGS, 0xff); // Clear all IRQ flags
//	RHSPIDriver_spiWrite(RH_RF95_REG_12_IRQ_FLAGS, 0xff); // Clear all IRQ flags
	return tipoPaq;
}






// Check whether the latest received message is complete and uncorrupted
void RH_RF95_validateRxBuf(void)
{
	if (_bufLen < 4)
		return; // Too short to be a real message
	// Extract the 4 headers
	_rxHeaderTo    = _buf[0];
	_rxHeaderFrom  = _buf[1];
	_rxHeaderId    = _buf[2];
	_rxHeaderFlags = _buf[3];
	if (_promiscuous ||
			_rxHeaderTo == _thisAddress ||
			_rxHeaderTo == RH_BROADCAST_ADDRESS)
	{
		_rxGood++;
		_rxBufValid = true;
	}
}

bool RH_RF95_available(void)
{
	if (_mode == RHModeTx)
		return false;
	RH_RF95_setModeRx();
	return _rxBufValid; // Will be set by the interrupt handler when a good message is received
}

void RH_RF95_clearRxBuf(void)
{
	_rxBufValid = false;
	_bufLen = 0;
}

bool RH_RF95_recv(uint8_t* buf, uint8_t* len)
{
	if (!RH_RF95_available())
		return false;
	if (buf && len)
	{
		// Skip the 4 headers that are at the beginning of the rxBuf
		if (*len > _bufLen-RH_RF95_HEADER_LEN)
			*len = _bufLen-RH_RF95_HEADER_LEN;
		memcpy(buf, _buf+RH_RF95_HEADER_LEN, *len);
	}
	RH_RF95_clearRxBuf(); // This message accepted and cleared
	return true;
}

bool RH_RF95_send(const uint8_t* data, uint8_t len)
{
	if (len > RH_RF95_MAX_MESSAGE_LEN)
		return false;

	RHGenericDriver_waitPacketSent(1000);  // waitPacketSent(); // Make sure we dont interrupt an outgoing message
	RH_RF95_setModeIdle();

	if (!RHGenericDriver_waitCAD())
	{
		return false;  // Check channel activity
	}

	// Position at the beginning of the FIFO
	RHSPIDriver_spiWrite(RH_RF95_REG_0D_FIFO_ADDR_PTR, 0);
	// The headers
	RHSPIDriver_spiWrite(RH_RF95_REG_00_FIFO, _txHeaderTo);
	RHSPIDriver_spiWrite(RH_RF95_REG_00_FIFO, _txHeaderFrom);
	RHSPIDriver_spiWrite(RH_RF95_REG_00_FIFO, _txHeaderId);
	RHSPIDriver_spiWrite(RH_RF95_REG_00_FIFO, _txHeaderFlags);
	// The message data
	RHSPIDriver_spiBurstWrite(RH_RF95_REG_00_FIFO, data, len);
	RHSPIDriver_spiWrite(RH_RF95_REG_22_PAYLOAD_LENGTH, len + RH_RF95_HEADER_LEN);
	//JCF
    RHSPIDriver_spiWrite(RH_RF95_REG_40_DIO_MAPPING1, 0x40); // Interrupt on TxDone

    RH_RF95_setModeTx(); // Start the transmitter
	// when Tx is done, interruptHandler will fire and radio mode will return to STANDBY
	return true;
}

bool RH_RF95_printRegisters(void)
{
#ifdef RH_HAVE_SERIAL
	uint8_t registers[] = { 0x01, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13, 0x014, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27};

	uint8_t i;
	for (i = 0; i < sizeof(registers); i++)
	{
		Serial.print(registers[i], HEX);
		Serial.print(": ");
		Serial.println(RHSPIDriver_spiRead(registers[i]), HEX);
	}
#endif
	return true;
}

uint8_t RH_RF95_maxMessageLength(void)
{
	return RH_RF95_MAX_MESSAGE_LEN;
}

bool RH_RF95_setFrequency(float centre)
{
	// Frf = FRF / FSTEP
	uint32_t frf = (centre * 1000000.0) / RH_RF95_FSTEP;
	RHSPIDriver_spiWrite(RH_RF95_REG_06_FRF_MSB, (frf >> 16) & 0xff);
	RHSPIDriver_spiWrite(RH_RF95_REG_07_FRF_MID, (frf >> 8) & 0xff);
	RHSPIDriver_spiWrite(RH_RF95_REG_08_FRF_LSB, frf & 0xff);
	_usingHFport = (centre >= 779.0);
	return true;
}

void RH_RF95_setModeIdle(void)
{
    RHSPIDriver_spiWrite(RH_RF95_REG_01_OP_MODE, RH_RF95_MODE_STDBY);
	_mode = RHModeIdle;
}

bool RH_RF95_sleep(void)
{
	RHSPIDriver_spiWrite(RH_RF95_REG_01_OP_MODE, RH_RF95_MODE_SLEEP);
	_mode = RHModeSleep;
	return true;
}

void RH_RF95_setModeRx(void)
{
	RHSPIDriver_spiWrite(RH_RF95_REG_01_OP_MODE, RH_RF95_MODE_RXCONTINUOUS);
	RHSPIDriver_spiWrite(RH_RF95_REG_40_DIO_MAPPING1, 0x00); // Interrupt on RxDone
	_mode = RHModeRx;
}

void RH_RF95_setModeTx(void)
{
      RHSPIDriver_spiWrite(RH_RF95_REG_01_OP_MODE, RH_RF95_MODE_TX);
      RHSPIDriver_spiWrite(RH_RF95_REG_40_DIO_MAPPING1, 0x40); // Interrupt on TxDone
      _mode = RHModeTx;
}

void RH_RF95_setTxPower(int8_t power, bool useRFO)
{
	// Sigh, different behaviours depending on whther the module use PA_BOOST or the RFO pin
	// for the transmitter output
	if (useRFO)
	{
		if (power > 14)
			power = 14;
		if (power < -1)
			power = -1;
		RHSPIDriver_spiWrite(RH_RF95_REG_09_PA_CONFIG, RH_RF95_MAX_POWER | (power + 1));
	}
	else
	{
		if (power > 23)
			power = 23;
		if (power < 5)
			power = 5;

		// For RH_RF95_PA_DAC_ENABLE, manual says '+20dBm on PA_BOOST when OutputPower=0xf'
		// RH_RF95_PA_DAC_ENABLE actually adds about 3dBm to all power levels. We will us it
		// for 21, 22 and 23dBm
		if (power > 20)
		{
			RHSPIDriver_spiWrite(RH_RF95_REG_4D_PA_DAC, RH_RF95_PA_DAC_ENABLE);
			power -= 3;
		}
		else
		{
			RHSPIDriver_spiWrite(RH_RF95_REG_4D_PA_DAC, RH_RF95_PA_DAC_DISABLE);
		}

		// RFM95/96/97/98 does not have RFO pins connected to anything. Only PA_BOOST
		// pin is connected, so must use PA_BOOST
		// Pout = 2 + OutputPower.
		// The documentation is pretty confusing on this topic: PaSelect says the max power is 20dBm,
		// but OutputPower claims it would be 17dBm.
		// My measurements show 20dBm is correct
		RHSPIDriver_spiWrite(RH_RF95_REG_09_PA_CONFIG, RH_RF95_PA_SELECT | (power-5));
	}
}

// Sets registers from a canned modem configuration structure
void RH_RF95_setModemRegisters(ModemConfigChoice index)
{
	RHSPIDriver_spiWrite(RH_RF95_REG_1D_MODEM_CONFIG1, MODEM_CONFIG_TABLE[index][0]);
	RHSPIDriver_spiWrite(RH_RF95_REG_1E_MODEM_CONFIG2, MODEM_CONFIG_TABLE[index][1]);
	RHSPIDriver_spiWrite(RH_RF95_REG_26_MODEM_CONFIG3, MODEM_CONFIG_TABLE[index][2]);
}

// Set one of the canned FSK Modem configs
// Returns true if its a valid choice
bool RH_RF95_setModemConfig(ModemConfigChoice index)
{
	if (index > 3) return false;
		//
		//    ModemConfig cfg;
		//    memcpy_P(&cfg, &MODEM_CONFIG_TABLE[index], sizeof(RH_RF95::ModemConfig));
    RH_RF95_setModemRegisters(index);
	return true;
}

void RH_RF95_setPreambleLength(uint16_t bytes)
{
	RHSPIDriver_spiWrite(RH_RF95_REG_20_PREAMBLE_MSB, bytes >> 8);
	RHSPIDriver_spiWrite(RH_RF95_REG_21_PREAMBLE_LSB, bytes & 0xff);
}

bool RH_RF95_isChannelActive(void)
{
	// Set mode RHModeCad
	if (_mode != RHModeCad)
	{
		RHSPIDriver_spiWrite(RH_RF95_REG_01_OP_MODE, RH_RF95_MODE_CAD);
		RHSPIDriver_spiWrite(RH_RF95_REG_40_DIO_MAPPING1, 0x80); // Interrupt on CadDone
		_mode = RHModeCad;
	}

	while (_mode == RHModeCad)
		osalThreadSleepMilliseconds(1); //YIELD;

	return _cad;
}

void RH_RF95_enableTCXO(void)
{
	while ((RHSPIDriver_spiRead(RH_RF95_REG_4B_TCXO) & RH_RF95_TCXO_TCXO_INPUT_ON) != RH_RF95_TCXO_TCXO_INPUT_ON)
	{
		osalThreadSleepMilliseconds(2);
		RHSPIDriver_spiWrite(RH_RF95_REG_4B_TCXO, (RHSPIDriver_spiRead(RH_RF95_REG_4B_TCXO) | RH_RF95_TCXO_TCXO_INPUT_ON));
	}
}

// From section 4.1.5 of SX1276/77/78/79
// Ferror = FreqError * 2**24 * BW / Fxtal / 500
int RH_RF95_frequencyError(void)
{
	int32_t freqerror = 0;

	// Convert 2.5 bytes (5 nibbles, 20 bits) to 32 bit signed int
	// Caution: some C compilers make errors with eg:
	// freqerror = RHSPIDriver_spiRead(RH_RF95_REG_28_FEI_MSB) << 16
	// so we go more carefully.
	freqerror = RHSPIDriver_spiRead(RH_RF95_REG_28_FEI_MSB);
	freqerror <<= 8;
	freqerror |= RHSPIDriver_spiRead(RH_RF95_REG_29_FEI_MID);
	freqerror <<= 8;
	freqerror |= RHSPIDriver_spiRead(RH_RF95_REG_2A_FEI_LSB);
	// Sign extension into top 3 nibbles
	if (freqerror & 0x80000)
		freqerror |= 0xfff00000;

	int error = 0; // In hertz
	float bw_tab[] = {7.8, 10.4, 15.6, 20.8, 31.25, 41.7, 62.5, 125, 250, 500};
	uint8_t bwindex = RHSPIDriver_spiRead(RH_RF95_REG_1D_MODEM_CONFIG1) >> 4;
	if (bwindex < (sizeof(bw_tab) / sizeof(float)))
		error = (float)freqerror * bw_tab[bwindex] * ((float)(1L << 24) / (float)RH_RF95_FXOSC / 500.0);
	// else not defined

	return error;
}

int RH_RF95_lastSNR(void)
{
	return _lastSNR;
}


// de RHGenericDriver.cpp


// Wait until no channel activity detected or timeout
bool RHGenericDriver_waitCAD(void)
{
	if (!_cad_timeout)
		return true;

	// Wait for any channel activity to finish or timeout
	// Sophisticated DCF function...
	// DCF : BackoffTime = random() x aSlotTime
	// 100 - 1000 ms
	// 10 sec timeout
	systime_t start = chVTGetSystemTime();
	while (RH_RF95_isChannelActive())
	{
		if (chVTTimeElapsedSinceX(start) > chTimeMS2I(_cad_timeout))
			return false;
		osalThreadSleepMilliseconds(10);
	}

	return true;
}


bool RHGenericDriver_waitPacketSent(uint16_t timeout)
{
	systime_t start = chVTGetSystemTime();
	while (chVTTimeElapsedSinceX(start) < chTimeMS2I(timeout))
	{
		if (_mode != RHModeTx) // Any previous transmit finished?
			return true;
		osalThreadSleepMilliseconds(10);
	}
	return false;
}

//void checkRx(void)
//{
//
//  uint8_t opMode = RHSPIDriver_spiRead(RH_RF95_REG_01_OP_MODE);
//  uint8_t dioMap = RHSPIDriver_spiRead(RH_RF95_REG_40_DIO_MAPPING1); // Interrupt on CadDone
//
////  if (opMode != ((RH_RF95_MODE_RXCONTINUOUS | RH_RF95_LONG_RANGE_MODE)))
////  {
////      RHSPIDriver_spiWrite(RH_RF95_REG_01_OP_MODE, RH_RF95_MODE_RXCONTINUOUS);
////      RHSPIDriver_spiWrite(RH_RF95_REG_40_DIO_MAPPING1, 0x00); // Interrupt on RxDone
////      uint8_t irq_flags = RHSPIDriver_spiRead(RH_RF95_REG_12_IRQ_FLAGS);
////      RHSPIDriver_spiWrite(RH_RF95_REG_12_IRQ_FLAGS, 0xFF);
////
////      chLcdprintfFila(3,"opM:%d _op:%d!!",opMode,_mode);
////      osalThreadSleepMilliseconds(500);
////  }
//
//}

