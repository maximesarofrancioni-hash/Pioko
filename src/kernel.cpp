// kernel.cpp - Pyoko's main loop on the Pi 4
//
// Single core, no scheduler: the main loop renders FL_BLOCK-frame blocks of the engine (core/) into Circle's sound
// write queue whenever there is room, and the I2S DMA interrupt drains it. Rendering never runs in interrupt
// context, so the DSP can use the FPU / NEON freely. It boots into the Lofi hip hop scene; the serial port (115200)
// and two buttons play it. The page's SP-303 Vinyl Sim is JavaScript and does not run here yet.
#include "kernel.h"
#include "engine.h"
#include "scenes.h"
#include <circle/util.h>
#include <circle/string.h>

static const char FromKernel[] = "pyoko";

// the sampler's memory, taken from the heap in Run() before the engine starts
static float *s_pSampleRAM = nullptr;
static int s_nSampleFrames = 0;
extern "C" float *pyoko_sample_ram (int *pFrames) { *pFrames = s_nSampleFrames; return s_pSampleRAM; }

static float Clamp (float x, float lo, float hi) { return x < lo ? lo : (x > hi ? hi : x); }
static int ClampI (int x, int lo, int hi) { return x < lo ? lo : (x > hi ? hi : x); }

static const char *const kSeqName[] = {"Euclid", "Marbles", "shift"};
static const char *const kStyleName[] = {"chords", "arp", "bass"};
static const char *const kProgName[] = {"Am F C G", "Fmaj9 Em9 Dm9 Cmaj9"};

CKernel::CKernel (void)
:	m_Timer (&m_Interrupt),
	m_CPUThrottle (CPUSpeedMaximum),
	m_Logger (m_Options.GetLogLevel (), &m_Timer),
	m_Sound (&m_Interrupt, SAMPLE_RATE, CHUNK_SIZE, FALSE, 0, 0,
		 CI2SSoundBaseDevice::DeviceModeTXOnly),
#if PIN_NEXT_PRESET
	m_NextPin (PIN_NEXT_PRESET, GPIOModeInputPullUp),
#endif
#if PIN_PLAY
	m_PlayPin (PIN_PLAY, GPIOModeInputPullUp),
#endif
	m_bPlaying (TRUE),
	m_nTrack (0),
	m_nLastBar (-1),
	m_nBusyTicks (0),
	m_nLedOffAt (0),
	m_nLastNext (HIGH), m_nLastPlay (HIGH),
	m_nNextDebounce (0), m_nPlayDebounce (0)
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
	m_Logger.Write (FromKernel, LogNotice, "Pyoko, %u Hz: osc440 + sampler, sst-filters, Marbles, Tides, Beads",
			SAMPLE_RATE);

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
		s_pSampleRAM = pRAM;
		s_nSampleFrames = (int) nFrames;
		m_Logger.Write (FromKernel, LogNotice, "Sampler: %u MB of RAM, %u s at %u Hz",
				nFrames * 4u >> 20, nFrames / SAMPLE_RATE, SAMPLE_RATE);
	}
	fl_init ((float) SAMPLE_RATE);
	Lofi ();

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
	PrintStatus ();

	unsigned nLastStatus = CTimer::GetClockTicks ();
	u64 nWindowStart = CTimer::GetClockTicks64 ();

	while (m_Sound.IsActive ())
	{
		unsigned nFree = nQueue - m_Sound.GetQueueFramesAvail ();
		if (nFree >= FL_BLOCK)
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

		// the ACT LED flashes on every bar
		int nBar = fl_bar ();
		if (m_bPlaying && nBar != m_nLastBar)
		{
			m_nLastBar = nBar;
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
	float Inter[FL_BLOCK * 2];

	while (nFrames >= (unsigned) FL_BLOCK)
	{
		if (m_bPlaying)
		{
			u64 t0 = CTimer::GetClockTicks64 ();
			fl_render ();
			m_nBusyTicks += CTimer::GetClockTicks64 () - t0;
			const float *L = fl_left (), *R = fl_right ();
			for (int i = 0; i < FL_BLOCK; i++)
			{
				Inter[2 * i] = L[i];
				Inter[2 * i + 1] = R[i];
			}
		}
		else
		{
			memset (Inter, 0, sizeof Inter);	// paused: the clock stops with the sound
		}
		if (m_Sound.Write (Inter, sizeof Inter) != (int) sizeof Inter)
		{
			m_Logger.Write (FromKernel, LogWarning, "Queue overflow");
		}
		nFrames -= FL_BLOCK;
	}
}

// ---------------------------------------------------------------- what the keys change, sent to the engine

void CKernel::Lofi (void)
{
	pyoko_scene_lofi ();
	// the scene's settings (core/scenes.cpp), as the keys start from
	m_fBpm = 78.f; m_nSwing = 62;
	m_nPreset = pyoko_preset_find ("bedroom_pop", "e-piano");
	m_nProg = 1; m_nStyle = 0; m_nOctave = 0;
	m_Track[0] = {0, 16, 3, 0, {17, 4}, {100.f, 86.f}, {0.1f, 0.12f}, FALSE};
	m_Track[1] = {0, 16, 16, 0, {17, 4}, {135.f, 135.f}, {0.f, 0.f}, FALSE};
	m_fSmpPitch = -3.f; m_bSmpOn = TRUE; m_bBeads = FALSE;
}

void CKernel::SendSeq (int nTrack)
{
	const TTrack &T = m_Track[nTrack];
	if (nTrack == 0)
		fl_wave_cfg (0, (float) (T.nSeq == 0 ? m_nStyle : T.nSeq == 1 ? 4 : 3));	// the engine's wave modes
	else
		fl_smp_set (20, (float) T.nSeq);
}

void CKernel::SendPattern (int nTrack)
{
	const TTrack &T = m_Track[nTrack];
	if (nTrack == 0)
		fl_wave_euclid (T.nSteps, T.nHits, T.nRot);
	else
	{
		fl_smp_set (11, (float) T.nSteps); fl_smp_set (12, (float) T.nHits); fl_smp_set (13, (float) T.nRot);
	}
}

void CKernel::SendFilters (int nTrack)
{
	const TTrack &T = m_Track[nTrack];
	if (nTrack == 0)
	{
		fl_wave_set (20, (float) T.nModel[0]); fl_wave_set (21, (float) T.nModel[1]);
		fl_wave_set (2, T.fCut[0]); fl_wave_set (7, T.fCut[1]);
		fl_wave_set (3, T.fRes[0]); fl_wave_set (8, T.fRes[1]);
		fl_wave_set (12, T.bParallel ? 1.f : 0.f);
	}
	else
	{
		fl_smp_set (21, (float) T.nModel[0]); fl_smp_set (22, (float) T.nModel[1]);
		fl_smp_set (23, T.fCut[0]); fl_smp_set (24, T.fCut[1]);
		fl_smp_set (25, T.fRes[0]); fl_smp_set (26, T.fRes[1]);
		fl_smp_set (27, T.bParallel ? 1.f : 0.f);
	}
}

void CKernel::PollButtons (void)
{
	unsigned nNow = CTimer::GetClockTicks ();
#if PIN_NEXT_PRESET
	unsigned v = m_NextPin.Read ();
	if (v != m_nLastNext && (int) (nNow - m_nNextDebounce) > 0)
	{
		m_nLastNext = v;
		m_nNextDebounce = nNow + 30000;		// 30 ms
		if (v == LOW)
		{
			HandleKey ('n');
		}
	}
#endif
#if PIN_PLAY
	unsigned h = m_PlayPin.Read ();
	if (h != m_nLastPlay && (int) (nNow - m_nPlayDebounce) > 0)
	{
		m_nLastPlay = h;
		m_nPlayDebounce = nNow + 30000;
		if (h == LOW)
		{
			HandleKey (' ');
		}
	}
#endif
}

void CKernel::HandleKey (char c)
{
	TTrack &T = m_Track[m_nTrack];
	const int nModels = fl_num_filters ();

	switch (c)
	{
	case ' ': m_bPlaying = !m_bPlaying; m_Logger.Write (FromKernel, LogNotice, m_bPlaying ? "Play" : "Pause"); return;
	case 'L': Lofi (); break;
	case 't': m_fBpm = Clamp (m_fBpm - 2.f, 40.f, 240.f); fl_set (4, m_fBpm); break;
	case 'T': m_fBpm = Clamp (m_fBpm + 2.f, 40.f, 240.f); fl_set (4, m_fBpm); break;
	case 's': case 'S':
		m_nSwing = ClampI (m_nSwing + (c == 'S' ? 2 : -2), 50, 70);
		fl_set (7, (float) (m_nSwing - 50) / 50.f);
		break;
	case '1': case '2': m_nTrack = c - '1'; break;

	// the edited track: its sequencer, Euclidean pattern and VCF
	case 'q': T.nSeq = (T.nSeq + 1) % 3; SendSeq (m_nTrack); break;
	case 'z': case 'x': case ',': case '.': case 'v': case 'b':
		if (c == 'z') T.nHits--;
		if (c == 'x') T.nHits++;
		if (c == ',') T.nSteps--;
		if (c == '.') T.nSteps++;
		if (c == 'v') T.nRot--;
		if (c == 'b') T.nRot++;
		T.nSteps = ClampI (T.nSteps, 1, 16);
		T.nHits = ClampI (T.nHits, 0, T.nSteps);
		T.nRot = (T.nRot % T.nSteps + T.nSteps) % T.nSteps;
		SendPattern (m_nTrack);
		break;
	case '[': case ']': T.nModel[0] = (T.nModel[0] + (c == ']' ? 1 : nModels - 1)) % nModels; SendFilters (m_nTrack); break;
	case '{': case '}': T.nModel[1] = (T.nModel[1] + (c == '}' ? 1 : nModels - 1)) % nModels; SendFilters (m_nTrack); break;
	case 'f': case 'F': T.fCut[0] = Clamp (T.fCut[0] + (c == 'F' ? 3.f : -3.f), 20.f, 135.f); SendFilters (m_nTrack); break;
	case 'g': case 'G': T.fCut[1] = Clamp (T.fCut[1] + (c == 'G' ? 3.f : -3.f), 20.f, 135.f); SendFilters (m_nTrack); break;
	case 'e': case 'E': T.fRes[0] = Clamp (T.fRes[0] + (c == 'E' ? 0.05f : -0.05f), 0.f, 1.f); SendFilters (m_nTrack); break;
	case 'r': case 'R': T.fRes[1] = Clamp (T.fRes[1] + (c == 'R' ? 0.05f : -0.05f), 0.f, 1.f); SendFilters (m_nTrack); break;
	case 'p': T.bParallel = !T.bParallel; SendFilters (m_nTrack); break;

	// track 1, osc440
	case 'n': case 'N':
		m_nPreset = (m_nPreset + (c == 'n' ? 1 : pyoko_preset_count () - 1)) % pyoko_preset_count ();
		pyoko_load_preset (m_nPreset);
		SendFilters (0);		// the preset resets the voice; keep its VCF as set here
		break;
	case 'c': m_nProg ^= 1; fl_wave_cfg (5, (float) m_nProg); break;
	case 'y': m_nStyle = (m_nStyle + 1) % 3; SendSeq (0); break;
	case 'o': case 'O': m_nOctave = ClampI (m_nOctave + (c == 'O' ? 1 : -1), -2, 2); fl_wave_cfg (1, (float) m_nOctave); break;

	// track 2, the sampler
	case 'k': fl_smp_demo (); break;
	case 'w': fl_smp_arm (2); m_Logger.Write (FromKernel, LogNotice, "Resample armed: 2 bars from the next downbeat"); return;
	case 'j': case 'J': m_fSmpPitch = Clamp (m_fSmpPitch + (c == 'J' ? 1.f : -1.f), -24.f, 24.f); fl_smp_set (2, m_fSmpPitch); break;
	case 'm': m_bSmpOn = !m_bSmpOn; fl_smp_set (10, m_bSmpOn ? 1.f : 0.f); break;

	// FX
	case 'B': m_bBeads = !m_bBeads; fl_mod_set (3, 0, m_bBeads ? 1.f : 0.f); break;

	case '?': PrintHelp (); return;
	case 'i': break;
	default: return;
	}
	PrintStatus ();
}

void CKernel::PrintStatus (void)
{
	m_Logger.Write (FromKernel, LogNotice, "%s %.0f BPM, swing %d%% | editing track %d | Beads %s",
			m_bPlaying ? "playing" : "paused", (double) m_fBpm, m_nSwing, m_nTrack + 1, m_bBeads ? "on" : "off");
	for (int t = 0; t < 2; t++)
	{
		const TTrack &T = m_Track[t];
		CString Src;
		if (t == 0)
			Src.Format ("osc440 %s / %s, %s, %s, oct %d", pyoko_preset_group (m_nPreset), pyoko_preset_name (m_nPreset),
				    kProgName[m_nProg], kStyleName[m_nStyle], m_nOctave);
		else
			Src.Format ("sampler %s, pitch %.0f st", m_bSmpOn ? "on" : "muted", (double) m_fSmpPitch);
		m_Logger.Write (FromKernel, LogNotice, "%c %d: %s | %s E(%d,%d)+%d", t == m_nTrack ? '>' : ' ', t + 1,
				(const char *) Src, kSeqName[T.nSeq], T.nHits, T.nSteps, T.nRot);
		m_Logger.Write (FromKernel, LogNotice, "     VCF %s: 1 %s %.0f res %.2f, 2 %s %.0f res %.2f",
				T.bParallel ? "parallel" : "series", fl_filter_name (T.nModel[0]), (double) T.fCut[0], (double) T.fRes[0],
				fl_filter_name (T.nModel[1]), (double) T.fCut[1], (double) T.fRes[1]);
	}
}

void CKernel::PrintHelp (void)
{
	m_Logger.Write (FromKernel, LogNotice,
		"Keys: space play/pause, L lofi scene, t/T tempo, s/S swing, 1/2 edit track, i status, ? help");
	m_Logger.Write (FromKernel, LogNotice,
		"Track: q sequencer (Euclid/Marbles/shift), z/x hits, ,/. steps, v/b rotate, [ ] filter 1 model, { } filter 2 model, "
		"f/F g/G cutoffs, e/E r/R resonances, p series/parallel");
	m_Logger.Write (FromKernel, LogNotice,
		"Osc: n/N preset, c chords, y chords/arp/bass, o/O octave | Sampler: k demo break, w resample 2 bars, j/J pitch, "
		"m mute | B Beads on/off");
}
