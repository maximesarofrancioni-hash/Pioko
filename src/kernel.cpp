// kernel.cpp - filter-lab main loop
//
// Single core, no scheduler: the main loop renders 32-frame blocks into
// Circle's sound write queue whenever there is room, and the I2S DMA
// interrupt drains it. Rendering never runs in interrupt context, so the
// filter code can use the FPU/NEON freely.
#include "kernel.h"
#include <circle/util.h>
#include <circle/string.h>

static const char FromKernel[] = "filterlab";

static const char *kLaneName[] = {"bass", "lead", "stab", "noise", "accent"};

CKernel::CKernel (void)
:	m_Timer (&m_Interrupt),
	m_CPUThrottle (CPUSpeedMaximum),
	m_Logger (m_Options.GetLogLevel (), &m_Timer),
	m_Sound (&m_Interrupt, SAMPLE_RATE, CHUNK_SIZE, FALSE, 0, 0,
		 CI2SSoundBaseDevice::DeviceModeTXOnly),
#if PIN_NEXT_FILTER
	m_NextPin (PIN_NEXT_FILTER, GPIOModeInputPullUp),
#endif
#if PIN_HOLD
	m_HoldPin (PIN_HOLD, GPIOModeInputPullUp),
#endif
	m_nEditLane (1),
	m_nBusyTicks (0),
	m_nLedOffAt (0),
	m_nLastNext (HIGH), m_nLastHold (HIGH),
	m_nNextDebounce (0), m_nHoldDebounce (0)
{
	m_ActLED.Blink (3);
}

CKernel::~CKernel (void)
{
}

boolean CKernel::Initialize (void)
{
	// flush denormals to zero: decaying filter tails would otherwise crawl
	u64 fpcr;
	asm volatile ("mrs %0, fpcr" : "=r" (fpcr));
	fpcr |= (1UL << 24);
	asm volatile ("msr fpcr, %0" : : "r" (fpcr));

	boolean bOK = m_Serial.Initialize (115200);
	if (bOK) bOK = m_Logger.Initialize (&m_Serial);
	if (bOK) bOK = m_Interrupt.Initialize ();
	if (bOK) bOK = m_Timer.Initialize ();
	return bOK;
}

TShutdownMode CKernel::Run (void)
{
	m_Logger.Write (FromKernel, LogNotice, "filter-lab, %u Hz, sst-filters through a Euclidean sequencer",
			SAMPLE_RATE);

	m_Lab.Init ((float) SAMPLE_RATE);
	m_Lab.barsPerFilter = DEFAULT_BARS_PER_FILTER;

	if (!m_Sound.AllocateQueueFrames (QUEUE_FRAMES))
	{
		m_Logger.Write (FromKernel, LogPanic, "Cannot allocate sound queue");
	}
	m_Sound.SetWriteFormat (SoundFormatFloat32, 2);

	const unsigned nQueue = m_Sound.GetQueueSizeFrames ();
	FillQueue (nQueue);

	if (!m_Sound.Start ())
	{
		m_Logger.Write (FromKernel, LogPanic, "Cannot start I2S");
	}
	m_Logger.Write (FromKernel, LogNotice, "Audio running, queue %u frames", nQueue);
	PrintHelp ();

	unsigned nLastStatus = CTimer::GetClockTicks ();
	u64 nWindowStart = CTimer::GetClockTicks64 ();

	while (m_Sound.IsActive ())
	{
		unsigned nFree = nQueue - m_Sound.GetQueueFramesAvail ();
		if (nFree >= FilterLab::kBlock)
		{
			FillQueue (nFree);
		}

		// UART keys (polled; the logger also uses this UART for output)
		char Buf[8];
		int n = m_Serial.Read (Buf, sizeof Buf);
		for (int i = 0; i < n; i++)
		{
			HandleKey (Buf[i]);
		}

		PollButtons ();
		m_CPUThrottle.Update ();	// lowers the clock if the SoC gets too hot

		if (m_Lab.filterChanged)
		{
			m_Lab.filterChanged = false;
			m_Logger.Write (FromKernel, LogNotice, "[%2d/%d] %s%s",
					m_Lab.FilterIndex () + 1, m_Lab.NumFilters (),
					m_Lab.FilterName (), m_Lab.barsPerFilter ? "" : "  (hold)");
			m_ActLED.On ();
			m_nLedOffAt = CTimer::GetClockTicks () + 40000;
		}
		if (m_nLedOffAt && (int) (CTimer::GetClockTicks () - m_nLedOffAt) > 0)
		{
			m_ActLED.Off ();
			m_nLedOffAt = 0;
		}

		// CPU load report every 10 s
		unsigned nNow = CTimer::GetClockTicks ();
		if (nNow - nLastStatus > 10 * CLOCKHZ)
		{
			u64 nNow64 = CTimer::GetClockTicks64 ();
			unsigned nPermille = (unsigned) (m_nBusyTicks * 1000 / (nNow64 - nWindowStart));
			m_Logger.Write (FromKernel, LogNotice, "DSP load %u.%u%% of one core, %u MHz, %u C",
					nPermille / 10, nPermille % 10,
					m_CPUThrottle.GetClockRate () / 1000000,
					m_CPUThrottle.GetTemperature ());
			m_nBusyTicks = 0;
			nWindowStart = nNow64;
			nLastStatus = nNow;
		}
	}

	m_Logger.Write (FromKernel, LogError, "Sound device stopped");
	return ShutdownHalt;
}

void CKernel::FillQueue (unsigned nFrames)
{
	float L[FilterLab::kBlock], R[FilterLab::kBlock];
	float Inter[FilterLab::kBlock * 2];

	while (nFrames >= (unsigned) FilterLab::kBlock)
	{
		u64 t0 = CTimer::GetClockTicks64 ();
		m_Lab.Render (L, R);
		m_nBusyTicks += CTimer::GetClockTicks64 () - t0;

		for (int i = 0; i < FilterLab::kBlock; i++)
		{
			Inter[2 * i] = L[i];
			Inter[2 * i + 1] = R[i];
		}
		if (m_Sound.Write (Inter, sizeof Inter) != (int) sizeof Inter)
		{
			m_Logger.Write (FromKernel, LogWarning, "Queue overflow");
		}
		nFrames -= FilterLab::kBlock;
	}
}

void CKernel::PollButtons (void)
{
	unsigned nNow = CTimer::GetClockTicks ();
#if PIN_NEXT_FILTER
	unsigned v = m_NextPin.Read ();
	if (v != m_nLastNext && (int) (nNow - m_nNextDebounce) > 0)
	{
		m_nLastNext = v;
		m_nNextDebounce = nNow + 30000;		// 30 ms
		if (v == LOW)
		{
			m_Lab.NextFilter ();
		}
	}
#endif
#if PIN_HOLD
	unsigned h = m_HoldPin.Read ();
	if (h != m_nLastHold && (int) (nNow - m_nHoldDebounce) > 0)
	{
		m_nLastHold = h;
		m_nHoldDebounce = nNow + 30000;
		if (h == LOW)
		{
			HandleKey ('h');
		}
	}
#endif
}

static float Clamp (float x, float lo, float hi)
{
	return x < lo ? lo : (x > hi ? hi : x);
}

void CKernel::HandleKey (char c)
{
	FilterLab &F = m_Lab;
	int nSteps, nHits, nRot;
	F.GetEuclid (m_nEditLane, nSteps, nHits, nRot);

	switch (c)
	{
	case 'n': F.NextFilter (); return;
	case 'p': F.PrevFilter (); return;
	case 'h':
		F.barsPerFilter = F.barsPerFilter ? 0 : DEFAULT_BARS_PER_FILTER;
		m_Logger.Write (FromKernel, LogNotice, F.barsPerFilter ? "Auto-advance every %d bars" : "Hold",
				F.barsPerFilter);
		return;

	case 'r': F.resonance = Clamp (F.resonance - 0.05f, 0.f, 0.99f); break;
	case 'R': F.resonance = Clamp (F.resonance + 0.05f, 0.f, 0.99f); break;
	case 'c': F.cutoffOffset = Clamp (F.cutoffOffset - 3.f, -48.f, 48.f); break;
	case 'C': F.cutoffOffset = Clamp (F.cutoffOffset + 3.f, -48.f, 48.f); break;
	case 'e': F.envAmount = Clamp (F.envAmount - 0.1f, 0.f, 2.f); break;
	case 'E': F.envAmount = Clamp (F.envAmount + 0.1f, 0.f, 2.f); break;
	case 'd': F.drive = Clamp (F.drive * 0.8f, 0.1f, 8.f); break;
	case 'D': F.drive = Clamp (F.drive * 1.25f, 0.1f, 8.f); break;
	case 's': F.sweep = !F.sweep; break;
	case 't': F.bpm = Clamp (F.bpm - 5.f, 40.f, 240.f); break;
	case 'T': F.bpm = Clamp (F.bpm + 5.f, 40.f, 240.f); break;

	case '1': case '2': case '3': case '4':
		F.mute[c - '1'] = !F.mute[c - '1'];
		break;

	case 'l':
		m_nEditLane = (m_nEditLane + 1) % (FilterLab::kLanes + 1);
		F.GetEuclid (m_nEditLane, nSteps, nHits, nRot);
		m_Logger.Write (FromKernel, LogNotice, "Editing %s: E(%d,%d) rot %d",
				kLaneName[m_nEditLane], nHits, nSteps, nRot);
		return;
	case 'z': case 'x': case ',': case '.': case 'v': case 'b':
		if (c == 'z') nHits--;
		if (c == 'x') nHits++;
		if (c == ',') nSteps--;
		if (c == '.') nSteps++;
		if (c == 'v') nRot--;
		if (c == 'b') nRot++;
		F.SetEuclid (m_nEditLane, nSteps, nHits, nRot);
		F.GetEuclid (m_nEditLane, nSteps, nHits, nRot);
		m_Logger.Write (FromKernel, LogNotice, "%s: E(%d,%d) rot %d",
				kLaneName[m_nEditLane], nHits, nSteps, nRot);
		return;

	case '?': PrintHelp (); return;
	case 'i': break;
	default: return;
	}
	PrintStatus ();
}

void CKernel::PrintStatus (void)
{
	FilterLab &F = m_Lab;
	CString Mutes;
	for (int i = 0; i < FilterLab::kLanes; i++)
	{
		Mutes.Append (F.mute[i] ? "-" : kLaneName[i]);
		Mutes.Append (" ");
	}
	m_Logger.Write (FromKernel, LogNotice,
			"[%d] %s | res %.2f cut %.0f env %.1f drive %.2f sweep %s bpm %.0f | %s",
			F.FilterIndex () + 1, F.FilterName (), (double) F.resonance,
			(double) F.cutoffOffset, (double) F.envAmount, (double) F.drive,
			F.sweep ? "on" : "off", (double) F.bpm, (const char *) Mutes);
}

void CKernel::PrintHelp (void)
{
	m_Logger.Write (FromKernel, LogNotice,
		"Keys: n/p next/prev filter, h hold, r/R reso, c/C cutoff, e/E env, d/D drive, "
		"s sweep, t/T tempo, 1-4 mute lanes, l pick lane, z/x hits, ,/. steps, v/b rotate, i status");
}
