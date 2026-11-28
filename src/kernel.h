// kernel.h - Pyoko on the Pi 4: the engine (core/) over I2S, played from the serial port and two buttons
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

	// what the keys change, as the engine has it (the engine keeps no readable copy of most of it)
	struct TTrack
	{
		int	nSeq;			// 0 its Euclidean pattern, 1 Marbles, 2 shift register
		int	nSteps, nHits, nRot;
		int	nModel[2];
		float	fCut[2], fRes[2];	// MIDI notes, 0..1
		boolean	bParallel;
	};
	void Lofi (void);
	void SendSeq (int nTrack);
	void SendPattern (int nTrack);
	void SendFilters (int nTrack);

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
#if PIN_NEXT_PRESET
	CGPIOPin		m_NextPin;
#endif
#if PIN_PLAY
	CGPIOPin		m_PlayPin;
#endif

	boolean			m_bPlaying;
	TTrack			m_Track[2];
	int			m_nTrack;		// 0 or 1: the track the keys edit
	float			m_fBpm;
	int			m_nSwing;		// 50..70 %
	int			m_nPreset, m_nProg, m_nStyle, m_nOctave;
	float			m_fSmpPitch;
	boolean			m_bSmpOn, m_bBeads;
	int			m_nLastBar;
	u64			m_nBusyTicks;
	unsigned		m_nLedOffAt;
	unsigned		m_nLastNext, m_nLastPlay;
	unsigned		m_nNextDebounce, m_nPlayDebounce;
};
