// kernel.h - filter-lab: Euclidean sequencer auditioning sst-filters over I2S
#pragma once

#include <circle/actled.h>
#include <circle/koptions.h>
#include <circle/devicenameservice.h>
#include <circle/serial.h>
#include <circle/exceptionhandler.h>
#include <circle/interrupt.h>
#include <circle/timer.h>
#include <circle/cputhrottle.h>
#include <circle/logger.h>
#include <circle/gpiopin.h>
#include <circle/sound/i2ssoundbasedevice.h>
#include <circle/types.h>
#include "filterlab.h"
#include "config.h"

enum TShutdownMode
{
	ShutdownNone,
	ShutdownHalt,
	ShutdownReboot
};

class CKernel
{
public:
	CKernel (void);
	~CKernel (void);

	boolean Initialize (void);
	TShutdownMode Run (void);

private:
	void FillQueue (unsigned nFrames);
	void HandleKey (char c);
	void PollButtons (void);
	void PrintHelp (void);
	void PrintStatus (void);

private:
	CActLED			m_ActLED;
	CKernelOptions		m_Options;
	CDeviceNameService	m_DeviceNameService;
	CSerialDevice		m_Serial;
	CExceptionHandler	m_ExceptionHandler;
	CInterruptSystem	m_Interrupt;
	CTimer			m_Timer;
	CCPUThrottle		m_CPUThrottle;
	CLogger			m_Logger;
	CI2SSoundBaseDevice	m_Sound;
#if PIN_NEXT_FILTER
	CGPIOPin		m_NextPin;
#endif
#if PIN_HOLD
	CGPIOPin		m_HoldPin;
#endif

	FilterLab		m_Lab;
	int			m_nEditLane;
	u64			m_nBusyTicks;
	unsigned		m_nLedOffAt;
	unsigned		m_nLastNext, m_nLastHold;
	unsigned		m_nNextDebounce, m_nHoldDebounce;
};
