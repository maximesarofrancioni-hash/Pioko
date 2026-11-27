// kernel.cpp - filter-lab main loop
//
// Single core, no scheduler: the main loop renders 32-frame blocks into
// Circle's sound write queue whenever there is room, and the I2S DMA
// interrupt drains it. Rendering never runs in interrupt context, so the
// filter code can use the FPU/NEON freely.
#include "kernel.h"
#include <circle/util.h>
#include <circle/string.h>

static const char FromKernel[] = "pyoko";

// the sampler's RAM comes from the heap in Initialize() (Circle keeps the kernel image + BSS under 2 MB)

static float Clamp (float x, float lo, float hi);

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
	m_nLastStep (-1),
	m_nLastBar (-1),
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
	{
		unsigned nFrames = SAMPLER_FRAMES;
		float *pRAM = nullptr;
		while (nFrames >= (1u << 18) && (pRAM = new float[nFrames]) == nullptr)
		{
			nFrames /= 2;			// not that much free: take what there is
		}
		if (pRAM == nullptr)
		{
			m_Logger.Write (FromKernel, LogError, "No memory for the sampler");
			nFrames = 0;
		}
		else
		{
			memset (pRAM, 0, nFrames * sizeof (float));
		}
		m_Sampler.Init (pRAM, (int) nFrames, (float) SAMPLE_RATE);
		m_Logger.Write (FromKernel, LogNotice, "Sampler: %u MB of RAM, %u s at %u Hz",
				nFrames * 4u >> 20, nFrames / SAMPLE_RATE, SAMPLE_RATE);
	}
	m_Sampler.MakeDemo ();
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

		// the sampler on its own lane, clocked by the lab's 16ths; it can resample the lab
		int nStep = m_Lab.Step (), nBar = m_Lab.Bar ();
		if (nStep != m_nLastStep || nBar != m_nLastBar)
		{
			m_nLastStep = nStep; m_nLastBar = nBar;
			double fSp16 = SAMPLE_RATE * 60.0 / m_Lab.bpm / 4.0;
			m_Sampler.Tick ((nStep + 15) % 16, nBar, fSp16);
		}
		m_Sampler.Capture (L, R, FilterLab::kBlock);
		m_Sampler.Render (L, R, FilterLab::kBlock);
		for (int i = 0; i < FilterLab::kBlock; i++)
		{
			L[i] = Clamp (L[i], -1.f, 1.f);
			R[i] = Clamp (R[i], -1.f, 1.f);
		}
		if (m_Sampler.TakeRecorded ())
		{
			m_Logger.Write (FromKernel, LogNotice, "Resampled %u frames", (unsigned) m_Sampler.Length ());
		}
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

	// sampler
	case 'm': m_Sampler.on = !m_Sampler.on; break;
	case 'k': m_Sampler.MakeDemo (); break;
	case 'w': m_Sampler.Arm (2); m_Logger.Write (FromKernel, LogNotice, "Resample armed: 2 bars from the next downbeat"); return;
	case 'g':					// 4, 8, 16, auto
		if (m_Sampler.autoSlice) { m_Sampler.autoSlice = false; m_Sampler.slices = 4; }
		else if (m_Sampler.slices == 16) m_Sampler.autoSlice = true;
		else m_Sampler.slices *= 2;
		m_Sampler.Update ();
		break;
	case 'y': m_Sampler.sens = Clamp (m_Sampler.sens - 0.1f, 0.f, 1.f); m_Sampler.Update (); break;
	case 'Y': m_Sampler.sens = Clamp (m_Sampler.sens + 0.1f, 0.f, 1.f); m_Sampler.Update (); break;
	case 'q': m_Sampler.pitchMode ^= 1; break;
	case 'o': m_Sampler.order ^= 1; break;
	case 'O': m_Sampler.mode ^= 1; break;
	case 'j': m_Sampler.pitch = Clamp (m_Sampler.pitch - 1.f, -24.f, 24.f); break;
	case 'J': m_Sampler.pitch = Clamp (m_Sampler.pitch + 1.f, -24.f, 24.f); break;
	case 'a': m_Sampler.hits = m_Sampler.hits > 0 ? m_Sampler.hits - 1 : 0; break;
	case 'A': m_Sampler.hits = m_Sampler.hits < m_Sampler.steps ? m_Sampler.hits + 1 : m_Sampler.steps; break;

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
	m_Logger.Write (FromKernel, LogNotice, "sampler %s | %s %d%s | pitch %+.0f %s | E(%d,%d) | %u frames",
			m_Sampler.on ? "on" : "muted", m_Sampler.mode ? (m_Sampler.autoSlice ? "auto" : "slice") : "one shot",
			m_Sampler.NumSlices (), m_Sampler.order ? " rnd" : "", (double) m_Sampler.pitch,
			m_Sampler.pitchMode ? "keep" : "vari", m_Sampler.hits, m_Sampler.steps,
			(unsigned) m_Sampler.Length ());
}

void CKernel::PrintHelp (void)
{
	m_Logger.Write (FromKernel, LogNotice,
		"Keys: n/p next/prev filter, h hold, r/R reso, c/C cutoff, e/E env, d/D drive, "
		"s sweep, t/T tempo, 1-4 mute lanes, l pick lane, z/x hits, ,/. steps, v/b rotate, i status");
	m_Logger.Write (FromKernel, LogNotice,
		"Sampler: m mute, k demo break, w resample 2 bars, g slices 4/8/16/auto, y/Y auto sens, o random order, "
		"O one shot/slice, j/J pitch, q varispeed/keep time, a/A hits");
}
