import React, { useState, useEffect, useRef } from 'react';
import { motion, AnimatePresence } from 'framer-motion';
import { 
  Smile, 
  Heart, 
  Flame, 
  Sparkles, 
  Coffee, 
  Moon, 
  Eye, 
  HelpCircle, 
  Hand, 
  Battery, 
  Wifi, 
  Activity,
  Rocket,
  Volume2,
  Music,
  Play,
  CheckCircle2,
  BookOpen,
  Power,
  PowerOff
} from 'lucide-react';
import prototypeImg from '../assets/prototipe.png';

const BABEL_QUOTES = [
  "\"We have to die to get their pity. We have to die for them to find us noble. Our deaths become their battle cry... I want to live.\"",
  "\"That's the beauty of learning a new language. It should feel like an enormous undertaking. It ought to intimidate you. It makes you appreciate the complexity of the ones you know already.\"",
  "\"Nice comes from the Latin word for 'stupid,' said Griffin. 'We do not want to be nice.'\"",
  "\"After all, we're here to make the unknown known, to make the other familiar. We're here to make magic with words.\"",
  "\"History isn't a premade tapestry that we've got to suffer, a closed world with no exit. We can form it. Make it. We just have to choose to make it.\"",
  "\"Books are meant to be touched, otherwise they're useless.\"",
  "\"Betrayal. Translation means doing violence upon the original, means warping and distorting it for foreign, unintended eyes.\"",
  "\"Power did not lie in the tip of a pen. Power could only be brought to heel by acts of defiance it could not ignore. With brute force.\""
];

const MOODS = [
  { id: 'happy', name: 'Happy', gif: '/gifs/robot-happy-hq.gif', icon: Smile, desc: 'Ceria & aktif menyapa' },
  { id: 'cool', name: 'Cool', gif: '/gifs/robot-cool-hq.gif', icon: Sparkles, desc: 'Kacamata gaya retro' },
  { id: 'love', name: 'Love', gif: '/gifs/robot-love-hq.gif', icon: Heart, desc: 'Mata hati penuh afeksi' },
  { id: 'wink', name: 'Wink', gif: '/gifs/robot-wink-hq.gif', icon: Eye, desc: 'Kedipan mata menggoda' },
  { id: 'excited', name: 'Excited', gif: '/gifs/robot-excited-hq.gif', icon: Activity, desc: 'Sangat bersemangat!' },
  { id: 'thinking', name: 'Thinking', gif: '/gifs/robot-thinking-hq.gif', icon: HelpCircle, desc: 'Sedang memproses ide' },
  { id: 'sleepy', name: 'Sleepy', gif: '/gifs/robot-sleepy-hq.gif', icon: Moon, desc: 'Mengantuk & lelah' },
  { id: 'angry', name: 'Angry', gif: '/gifs/robot-angry-hq.gif', icon: Flame, desc: 'Sedikit kesal' },
  { id: 'yawn', name: 'Yawn', gif: '/gifs/robot-yawn-hq.gif', icon: Coffee, desc: 'Menguap butuh kopi' },
  { id: 'intro', name: 'Boot Intro', gif: '/gifs/intro.gif', icon: Rocket, desc: 'Intro Boot + Chiptune Melody' },
];

export default function OLEDCompanion() {
  const [currentMood, setCurrentMood] = useState(MOODS[0]); // default to happy
  const [bootMode, setBootMode] = useState('companion'); // 'companion' | 'boot_intro' | 'boot_quote' | 'sleeping'
  const [selectedQuoteIdx, setSelectedQuoteIdx] = useState(0);
  const [revealedChars, setRevealedChars] = useState(0);
  const [isTypingDone, setIsTypingDone] = useState(false);
  const [isTouched, setIsTouched] = useState(false);
  const [isPlayingMelody, setIsPlayingMelody] = useState(false);
  const [clock, setClock] = useState('18:30');
  const [sleepProgress, setSleepProgress] = useState(null); // null or 0..100

  const audioContextRef = useRef(null);
  const typingIntervalRef = useRef(null);
  const bootTimeoutRef = useRef(null);
  const touchHoldTimerRef = useRef(null);
  const touchHoldStartRef = useRef(null);

  useEffect(() => {
    const updateTime = () => {
      const now = new Date();
      setClock(now.toLocaleTimeString([], { hour: '2-digit', minute: '2-digit', hour12: false }));
    };
    updateTime();
    const interval = setInterval(updateTime, 10000);
    return () => {
      clearInterval(interval);
      if (typingIntervalRef.current) clearInterval(typingIntervalRef.current);
      if (bootTimeoutRef.current) clearTimeout(bootTimeoutRef.current);
      if (touchHoldTimerRef.current) clearInterval(touchHoldTimerRef.current);
    };
  }, []);

  // Web Audio Chiptune Synthesizer (Space Retro Chiptune on GPIO 5)
  const playIntroMelody = () => {
    try {
      const AudioCtx = window.AudioContext || window.webkitAudioContext;
      if (!AudioCtx) return;

      if (!audioContextRef.current) {
        audioContextRef.current = new AudioCtx();
      }
      const ctx = audioContextRef.current;
      if (ctx.state === 'suspended') {
        ctx.resume();
      }

      // Space chiptune notes in Hz
      const melody = [
        523, 659, 784, 1047,  // C5, E5, G5, C6
        988, 784, 659, 523,   // B5, G5, E5, C5
        587, 698, 880, 1047,  // D5, F5, A5, C6
        988, 880, 784, 659    // B5, A5, G5, E5
      ];

      const durations = [
        8, 8, 8, 4,
        8, 8, 8, 4,
        8, 8, 8, 4,
        8, 8, 8, 4
      ];

      setIsPlayingMelody(true);
      let currentTime = ctx.currentTime + 0.05;

      for (let i = 0; i < melody.length; i++) {
        const noteDur = 1.0 / durations[i];
        const osc = ctx.createOscillator();
        const gain = ctx.createGain();

        // 8-bit square wave representing ESP32 passive buzzer on GPIO 5
        osc.type = 'square';
        osc.frequency.setValueAtTime(melody[i], currentTime);

        // Sound volume envelope
        gain.gain.setValueAtTime(0.08, currentTime);
        gain.gain.exponentialRampToValueAtTime(0.0001, currentTime + noteDur * 0.82);

        osc.connect(gain);
        gain.connect(ctx.destination);

        osc.start(currentTime);
        osc.stop(currentTime + noteDur * 0.85);

        currentTime += noteDur * 1.30;
      }

      const totalDurationMs = (currentTime - ctx.currentTime) * 1000;
      setTimeout(() => {
        setIsPlayingMelody(false);
      }, totalDurationMs);

    } catch (err) {
      console.warn('Audio playback not permitted or failed', err);
      setIsPlayingMelody(false);
    }
  };

  // Play soft falling sleep chime
  const playSleepMelody = () => {
    try {
      const AudioCtx = window.AudioContext || window.webkitAudioContext;
      if (!AudioCtx) return;
      if (!audioContextRef.current) audioContextRef.current = new AudioCtx();
      const ctx = audioContextRef.current;
      if (ctx.state === 'suspended') ctx.resume();

      const notes = [659, 523, 392, 261]; // E5, C5, G4, C4
      const durs = [0.12, 0.15, 0.18, 0.32];
      let t = ctx.currentTime + 0.05;

      for (let i = 0; i < notes.length; i++) {
        const osc = ctx.createOscillator();
        const gain = ctx.createGain();
        osc.type = 'sine';
        osc.frequency.setValueAtTime(notes[i], t);
        gain.gain.setValueAtTime(0.08, t);
        gain.gain.exponentialRampToValueAtTime(0.0001, t + durs[i] * 0.9);
        osc.connect(gain);
        gain.connect(ctx.destination);
        osc.start(t);
        osc.stop(t + durs[i]);
        t += durs[i] + 0.04;
      }
    } catch (e) {
      console.warn('Sleep chime failed', e);
    }
  };

  // Full Boot Sequence: intro.gif + Melody -> Babel Quote -> Companion Face
  const startBootSequence = () => {
    if (typingIntervalRef.current) clearInterval(typingIntervalRef.current);
    if (bootTimeoutRef.current) clearTimeout(bootTimeoutRef.current);

    setBootMode('boot_intro');
    const introItem = MOODS.find(m => m.id === 'intro') || { id: 'intro', name: 'Boot Intro', gif: '/gifs/intro.gif' };
    setCurrentMood(introItem);
    playIntroMelody();

    // After intro animation and melody (~3.3 seconds), transition to quote stage
    bootTimeoutRef.current = setTimeout(() => {
      startQuoteStage();
    }, 3350);
  };

  const startQuoteStage = () => {
    setBootMode('boot_quote');
    const randomIdx = Math.floor(Math.random() * BABEL_QUOTES.length);
    setSelectedQuoteIdx(randomIdx);
    const quote = BABEL_QUOTES[randomIdx];
    setRevealedChars(0);
    setIsTypingDone(false);

    let currentLen = 0;
    typingIntervalRef.current = setInterval(() => {
      currentLen += 2;
      setRevealedChars(currentLen);
      if (currentLen >= quote.length) {
        clearInterval(typingIntervalRef.current);
        setIsTypingDone(true);
        // After quote typing finishes, pause for reading then transition to companion
        bootTimeoutRef.current = setTimeout(() => {
          endBootSequence();
        }, 3500);
      }
    }, 35);
  };

  const endBootSequence = () => {
    if (typingIntervalRef.current) clearInterval(typingIntervalRef.current);
    if (bootTimeoutRef.current) clearTimeout(bootTimeoutRef.current);
    setBootMode('companion');
    const happyMood = MOODS.find(m => m.id === 'happy') || MOODS[0];
    setCurrentMood(happyMood);
  };

  const enterSleepMode = () => {
    playSleepMelody();
    setBootMode('sleeping');
    setSleepProgress(null);
  };

  // Handle Touch Tap
  const handleTouch = () => {
    setIsTouched(true);
    setTimeout(() => setIsTouched(false), 1200);

    // If currently sleeping, touch wakes up and starts boot intro
    if (bootMode === 'sleeping') {
      startBootSequence();
      return;
    }

    // If currently in boot intro stage, touch skips to quote
    if (bootMode === 'boot_intro') {
      clearTimeout(bootTimeoutRef.current);
      startQuoteStage();
      return;
    }

    // If currently in boot quote stage:
    if (bootMode === 'boot_quote') {
      const quote = BABEL_QUOTES[selectedQuoteIdx];
      if (!isTypingDone) {
        // Fast-forward text
        if (typingIntervalRef.current) clearInterval(typingIntervalRef.current);
        setRevealedChars(quote.length);
        setIsTypingDone(true);
      } else {
        // Exit quote to companion face
        endBootSequence();
      }
      return;
    }

    // In normal companion mode: trigger interactive reaction
    const touchReactions = ['love', 'wink', 'excited'];
    const randomReaction = MOODS.find(m => m.id === touchReactions[Math.floor(Math.random() * touchReactions.length)]);
    if (randomReaction) setCurrentMood(randomReaction);
  };

  // Touch Hold Handlers for 5-Second Sleep Mode
  const handleTouchDown = () => {
    touchHoldStartRef.current = Date.now();
    if (touchHoldTimerRef.current) clearInterval(touchHoldTimerRef.current);

    touchHoldTimerRef.current = setInterval(() => {
      if (!touchHoldStartRef.current) return;
      const elapsed = Date.now() - touchHoldStartRef.current;

      if (bootMode === 'sleeping') {
        // Just waking up on tap
        return;
      }

      if (elapsed >= 5000) {
        // 5 seconds reached! Trigger Sleep Mode
        clearInterval(touchHoldTimerRef.current);
        touchHoldStartRef.current = null;
        enterSleepMode();
      } else if (elapsed >= 2000) {
        // Showing countdown from 2s to 5s
        const progress = Math.min(100, Math.floor(((elapsed - 2000) / 3000) * 100));
        setSleepProgress(progress);
      }
    }, 40);
  };

  const handleTouchUp = () => {
    if (touchHoldTimerRef.current) {
      clearInterval(touchHoldTimerRef.current);
      touchHoldTimerRef.current = null;
    }

    const elapsed = touchHoldStartRef.current ? Date.now() - touchHoldStartRef.current : 0;
    touchHoldStartRef.current = null;
    setSleepProgress(null);

    // If released before 2s, treat as regular single tap
    if (elapsed < 600) {
      handleTouch();
    }
  };

  const currentQuote = BABEL_QUOTES[selectedQuoteIdx];
  const displayedQuote = currentQuote.slice(0, revealedChars);

  return (
    <div id="simulator" className="relative glass-panel rounded-2xl p-4 sm:p-6 lg:p-8 border border-white/10 shadow-card overflow-hidden scroll-mt-20">
      <div className="grid grid-cols-1 lg:grid-cols-12 gap-6 lg:gap-8 items-center">
        
        {/* Left: Device Hardware Prototype & OLED Simulator */}
        <div className="lg:col-span-6 flex flex-col items-center w-full">
          <div className="w-full max-w-[340px] sm:max-w-[400px] mx-auto flex flex-col items-center">
            
            {/* Hardware Status Header */}
            <div className="w-full flex items-center justify-between px-2 mb-2 text-[10px] sm:text-[11px] font-mono text-slate-400">
              <span className="flex items-center gap-1.5 font-bold tracking-wider text-violet-400">
                <span className={`w-2 h-2 rounded-full ${
                  bootMode === 'sleeping' 
                    ? 'bg-slate-700' 
                    : isPlayingMelody 
                      ? 'bg-emerald-400 animate-ping' 
                      : 'bg-violet-400 animate-pulse'
                }`} />
                DESKBUDDY HARDWARE
              </span>
              <span className="text-slate-500">SSD1306 128×64 OLED</span>
            </div>

            {/* Hardware Prototype Container */}
            <motion.div 
              className="relative w-full aspect-square select-none"
              animate={{ y: bootMode === 'sleeping' ? 0 : [0, -4, 0] }}
              transition={{ repeat: Infinity, duration: 4, ease: "easeInOut" }}
            >
              {/* Chassis Image */}
              <img 
                src={prototypeImg} 
                alt="DeskBuddy Hardware Prototype" 
                className="w-full h-full object-contain filter drop-shadow-2xl pointer-events-none select-none" 
              />

              {/* Dynamic OLED Screen Overlay inside the prototype cutout */}
              <div 
                onMouseDown={handleTouchDown}
                onMouseUp={handleTouchUp}
                onTouchStart={handleTouchDown}
                onTouchEnd={handleTouchUp}
                style={{ 
                  position: 'absolute', 
                  left: '27.4%', 
                  top: '16.9%', 
                  width: '46.3%', 
                  height: '25.3%' 
                }}
                className={`bg-[#0d0e12] rounded-[3px] sm:rounded-[5px] cursor-pointer overflow-hidden flex flex-col justify-between p-1 sm:p-1.5 z-10 select-none group border border-white/[0.06] shadow-inner transition-opacity duration-300 ${
                  bootMode === 'sleeping' ? 'opacity-85' : 'opacity-100'
                }`}
                title="Layar OLED DeskBuddy (Klik untuk Touch Sensor)"
              >
                {/* OLED Status Bar Header */}
                <div className="flex items-center justify-between text-[7.5px] sm:text-[8.5px] font-mono text-violet-200/90 relative z-20 px-0.5 border-b border-violet-500/20 pb-0.5 leading-none">
                  {bootMode === 'sleeping' ? (
                    <div className="w-full text-center text-slate-500 font-mono text-[7px] sm:text-[8px]">
                      [ SLEEP MODE (0 mA) ]
                    </div>
                  ) : sleepProgress !== null ? (
                    <div className="w-full text-center text-violet-300 font-mono font-bold text-[7px] sm:text-[8px] animate-pulse">
                      TAHAN 5s UNTUK MATIKAN
                    </div>
                  ) : bootMode === 'companion' ? (
                    <>
                      <div className="flex items-center gap-1">
                        <Wifi className="w-2.5 h-2.5 text-violet-300" />
                        <span>NTP</span>
                      </div>
                      <span className="font-bold tracking-wider">{clock}</span>
                      <div className="flex items-center gap-0.5">
                        <span>94%</span>
                        <Battery className="w-2.5 h-2.5 text-violet-300" />
                      </div>
                    </>
                  ) : bootMode === 'boot_intro' ? (
                    <>
                      <div className="flex items-center gap-1 text-emerald-300 font-bold">
                        <Rocket className="w-2.5 h-2.5 animate-bounce" />
                        <span>BOOT</span>
                      </div>
                      <span className="text-[7px] text-violet-400/80 animate-pulse">BUZZER</span>
                      <span className="text-[7px] text-slate-400">128×64</span>
                    </>
                  ) : (
                    <>
                      <div className="flex items-center gap-1 text-violet-300 font-bold">
                        <BookOpen className="w-2.5 h-2.5 text-violet-400" />
                        <span>#{selectedQuoteIdx + 1}/26</span>
                      </div>
                      <span className="text-[6.5px] sm:text-[7px] text-violet-400/60 uppercase">TAP TO SKIP</span>
                    </>
                  )}
                </div>

                {/* OLED Display Area (GIF / Quotes / Sleep) */}
                <div className="relative z-20 flex-1 flex items-center justify-center my-0.5 overflow-hidden">
                  <AnimatePresence mode="wait">
                    {sleepProgress !== null ? (
                      <motion.div
                        key="sleep-countdown"
                        initial={{ opacity: 0, scale: 0.95 }}
                        animate={{ opacity: 1, scale: 1 }}
                        exit={{ opacity: 0 }}
                        className="flex-1 flex flex-col justify-center items-center px-1 font-mono text-violet-300 select-none text-center"
                      >
                        <div className="text-[7px] sm:text-[8px] text-violet-400 font-bold uppercase tracking-wider">== SLEEP MODE ==</div>
                        <div className="text-[10px] sm:text-xs font-bold text-violet-200 my-0.5">(- . -) z Z</div>
                        <div className="text-[7px] sm:text-[7.5px] text-violet-400/90 mb-0.5">
                          Tidur {Math.max(1, Math.ceil((100 - sleepProgress) * 3 / 100))}s...
                        </div>
                        <div className="w-20 sm:w-28 h-1.5 bg-slate-950 border border-violet-400/50 rounded-full overflow-hidden p-0.5">
                          <div 
                            className="h-full bg-gradient-to-r from-violet-500 to-fuchsia-500 rounded-full transition-all duration-75"
                            style={{ width: `${sleepProgress}%` }}
                          />
                        </div>
                      </motion.div>
                    ) : bootMode === 'sleeping' ? (
                      <motion.div
                        key="sleeping-screen"
                        initial={{ opacity: 0 }}
                        animate={{ opacity: 1 }}
                        exit={{ opacity: 0 }}
                        className="flex-1 flex flex-col justify-center items-center text-center p-1 select-none"
                      >
                        <div className="w-2 h-2 rounded-full bg-slate-700 animate-ping mb-1" />
                        <span className="text-[8px] sm:text-[9px] font-mono text-slate-500 font-bold tracking-wider">LAYAR MATI (0 mA)</span>
                        <span className="text-[6.5px] sm:text-[7.5px] font-mono text-slate-600">Sentuh sensor untuk bangun</span>
                      </motion.div>
                    ) : bootMode === 'boot_quote' ? (
                      <motion.div
                        key="boot-quote"
                        initial={{ opacity: 0, y: 3 }}
                        animate={{ opacity: 1, y: 0 }}
                        exit={{ opacity: 0, y: -3 }}
                        className="w-full px-0.5 text-left font-mono text-violet-200 select-none"
                      >
                        <p className="text-[8px] sm:text-[9px] leading-tight tracking-tight text-violet-100 line-clamp-3">
                          {displayedQuote}
                          {!isTypingDone && (
                            <span className="inline-block w-1 h-2.5 bg-violet-400 ml-0.5 animate-pulse align-middle" />
                          )}
                        </p>
                      </motion.div>
                    ) : (
                      <motion.img
                        key={currentMood.id + bootMode}
                        src={bootMode === 'boot_intro' ? '/gifs/intro.gif' : currentMood.gif}
                        alt={currentMood.name}
                        className="max-h-[46px] sm:max-h-[58px] object-contain contrast-125 brightness-110"
                        initial={{ opacity: 0, scale: 0.8 }}
                        animate={{ opacity: 1, scale: 1 }}
                        exit={{ opacity: 0, scale: 0.85 }}
                        transition={{ duration: 0.2 }}
                      />
                    )}
                  </AnimatePresence>
                </div>

                {/* OLED Footer status */}
                <div className="flex items-center justify-between text-[7px] sm:text-[8px] font-mono text-violet-400/80 relative z-20 px-0.5 border-t border-violet-500/20 pt-0.5 leading-none">
                  {bootMode === 'sleeping' ? (
                    <div className="w-full text-center text-[6.5px] sm:text-[7px] text-slate-600">
                      STATUS: LIGHT SLEEP
                    </div>
                  ) : sleepProgress !== null ? (
                    <div className="w-full text-center text-[6.5px] sm:text-[7px] text-violet-400">
                      LEPAS UNTUK BATAL
                    </div>
                  ) : bootMode === 'boot_intro' ? (
                    <>
                      <span className="text-emerald-400 font-bold flex items-center gap-1 animate-pulse">
                        <Music className="w-2 h-2" /> CHIPTUNE
                      </span>
                      <span className="text-[6.5px] text-violet-400/50">STEP 1/2</span>
                    </>
                  ) : bootMode === 'boot_quote' ? (
                    <>
                      <span className="truncate text-violet-300">
                        {isTypingDone ? 'QUOTES COMPLETE' : 'TYPEWRITER...'}
                      </span>
                      <span className="text-[6.5px] text-violet-400/60">STEP 2/2</span>
                    </>
                  ) : (
                    <>
                      <span className="truncate">MODE: {currentMood.name.toUpperCase()}</span>
                      {isPlayingMelody ? (
                        <span className="flex items-center gap-0.5 text-emerald-400 font-bold animate-pulse">
                          <Music className="w-2 h-2" /> BUZZER
                        </span>
                      ) : (
                        <span className="text-[6.5px] text-violet-400/50">TAHAN 5s = SLEEP</span>
                      )}
                    </>
                  )}
                </div>

                {/* Touch Feedback overlay inside screen */}
                {isTouched && bootMode !== 'sleeping' && (
                  <motion.div 
                    initial={{ opacity: 0 }}
                    animate={{ opacity: 1 }}
                    exit={{ opacity: 0 }}
                    className="absolute inset-0 bg-violet-600/20 pointer-events-none z-30 flex items-center justify-center backdrop-blur-[0.5px]"
                  >
                    <span className="px-1.5 py-0.5 bg-[#160f33]/90 border border-violet-400/60 rounded text-violet-200 text-[8px] sm:text-[9px] font-mono font-bold">
                      TOUCH!
                    </span>
                  </motion.div>
                )}
              </div>

              {/* Touch Sensor Area on Bottom Wooden Plate */}
              <div 
                onMouseDown={handleTouchDown}
                onMouseUp={handleTouchUp}
                onTouchStart={handleTouchDown}
                onTouchEnd={handleTouchUp}
                style={{
                  position: 'absolute',
                  left: '23.5%',
                  top: '58.5%',
                  width: '53.5%',
                  height: '27.5%'
                }}
                className={`rounded-lg cursor-pointer transition-all duration-200 flex items-center justify-center group ${
                  isTouched 
                    ? 'bg-violet-600/30 ring-2 ring-violet-400/60' 
                    : 'hover:bg-violet-500/10'
                }`}
                title="Sensor Sentuh Kapasitif (GPIO 7) - Klik/Tahan di sini"
              >
                <div className={`opacity-0 group-hover:opacity-100 transition-opacity bg-black/60 px-2 py-0.5 rounded text-[9px] sm:text-[10px] font-mono text-violet-200 pointer-events-none ${
                  isTouched ? 'opacity-100 !bg-violet-900/80' : ''
                }`}>
                  {isTouched ? '✨ SENSOR TOUCH (GPIO 7)' : '👆 Sentuh Sensor (GPIO 7)'}
                </div>
              </div>

            </motion.div>

            {/* Hardware Action Buttons below case */}
            <div className="w-full mt-3 grid grid-cols-2 gap-2 pt-2 border-t border-slate-800">
              <button
                onClick={startBootSequence}
                className={`flex items-center justify-center gap-1.5 sm:gap-2 py-2 sm:py-2.5 px-2 sm:px-3 rounded-xl border text-[11px] sm:text-xs font-mono transition-all active:scale-95 cursor-pointer ${
                  bootMode === 'boot_intro' || isPlayingMelody
                    ? 'bg-emerald-500/20 border-emerald-400 text-emerald-300 font-bold'
                    : 'bg-violet-500/15 hover:bg-violet-500/25 border-violet-500/40 text-violet-200'
                }`}
              >
                <Rocket className={`w-3 sm:w-3.5 h-3 sm:h-3.5 ${isPlayingMelody ? 'animate-bounce text-emerald-400' : 'text-violet-400'}`} />
                <span className="truncate">{bootMode === 'boot_intro' ? 'Booting...' : 'Boot Sequence'}</span>
              </button>

              <button
                onMouseDown={handleTouchDown}
                onMouseUp={handleTouchUp}
                onTouchStart={handleTouchDown}
                onTouchEnd={handleTouchUp}
                className={`flex items-center justify-center gap-1.5 sm:gap-2 py-2 sm:py-2.5 px-2 sm:px-3 rounded-xl border text-[11px] sm:text-xs font-mono transition-all active:scale-95 cursor-pointer ${
                  bootMode === 'sleeping'
                    ? 'bg-emerald-500/20 border-emerald-400 text-emerald-300 animate-pulse font-bold'
                    : 'bg-slate-800/80 hover:bg-violet-500/20 border-slate-700 hover:border-violet-500/40 text-slate-300 hover:text-violet-200'
                }`}
              >
                {bootMode === 'sleeping' ? (
                  <>
                    <Power className="w-3 sm:w-3.5 h-3 sm:h-3.5 text-emerald-400 shrink-0" />
                    <span className="truncate">Bangunkan</span>
                  </>
                ) : (
                  <>
                    <Hand className="w-3 sm:w-3.5 h-3 sm:h-3.5 text-violet-400 shrink-0" />
                    <span className="truncate">Touch / Sleep</span>
                  </>
                )}
              </button>
            </div>
            
            <p className="text-[10px] text-slate-400 text-center mt-2 font-mono">
              💡 Klik layar atau sentuh panel bawah (GPIO 7) untuk ganti ekspresi / tahan 5s untuk sleep
            </p>

          </div>
        </div>

        {/* Right: Companion Controls & Expression Selector */}
        <div className="lg:col-span-6 flex flex-col justify-between space-y-5 sm:space-y-6">
          <div className="space-y-3 sm:space-y-4">
            <div>
              <div className="flex flex-wrap items-center gap-2 mb-2">
                <span className="px-2 py-0.5 text-[10px] font-mono uppercase tracking-widest text-violet-300 bg-violet-500/15 border border-violet-500/25 rounded">
                  ESP32-C3 Firmware v1.0
                </span>
                <span className={`px-2 py-0.5 text-[10px] font-mono rounded border flex items-center gap-1 ${
                  bootMode === 'sleeping'
                    ? 'text-slate-400 border-slate-700 bg-slate-900'
                    : 'text-emerald-400 border-emerald-500/20 bg-emerald-500/10'
                }`}>
                  <span className={`w-1.5 h-1.5 rounded-full ${bootMode === 'sleeping' ? 'bg-slate-500' : 'bg-emerald-400 animate-pulse'}`} />
                  {bootMode === 'sleeping' ? 'Light Sleep (0 mA)' : 'Active (Online)'}
                </span>
              </div>
              <h1 className="text-2xl sm:text-3xl lg:text-4xl font-black text-white tracking-tight">
                DeskBuddy <span className="text-violet-400">V1</span>
              </h1>
              <p className="text-xs font-mono text-slate-400 mt-1">
                ESP32-C3 Robot Companion & Interactive Simulator
              </p>
            </div>

            {/* Quick Gestures Grid */}
            <div className="grid grid-cols-2 sm:grid-cols-4 gap-2 pt-1">
              <div className="p-2 sm:p-2.5 rounded-xl bg-slate-900/60 border border-white/[0.06] text-center">
                <div className="text-[10px] font-mono text-violet-400 font-bold uppercase">1x Tap</div>
                <div className="text-xs font-medium text-slate-200 mt-0.5">Pet / Jump</div>
              </div>
              <div className="p-2 sm:p-2.5 rounded-xl bg-slate-900/60 border border-white/[0.06] text-center">
                <div className="text-[10px] font-mono text-violet-300 font-bold uppercase">2x Tap</div>
                <div className="text-xs font-medium text-slate-200 mt-0.5">Back / Reset</div>
              </div>
              <div className="p-2 sm:p-2.5 rounded-xl bg-slate-900/60 border border-white/[0.06] text-center">
                <div className="text-[10px] font-mono text-amber-400 font-bold uppercase">Hold 1.5s</div>
                <div className="text-xs font-medium text-slate-200 mt-0.5">Menu Astra</div>
              </div>
              <div className="p-2 sm:p-2.5 rounded-xl bg-emerald-950/20 border border-emerald-500/30 text-center">
                <div className="text-[10px] font-mono text-emerald-400 font-bold uppercase">Hold 5s</div>
                <div className="text-xs font-medium text-emerald-200 mt-0.5">Sleep (0 mA)</div>
              </div>
            </div>
          </div>

          {/* Quick Mood Switches */}
          <div>
            <div className="flex items-center justify-between mb-2">
              <label className="text-[11px] font-mono text-slate-400 uppercase tracking-wider">
                Emosi & Animasi
              </label>
              {bootMode === 'sleeping' ? (
                <span className="text-[11px] font-mono text-slate-500 flex items-center gap-1">
                  <Moon className="w-3 h-3" /> Sedang Tidur
                </span>
              ) : (
                <span className="text-[11px] font-mono text-violet-400 truncate max-w-[150px]">
                  {bootMode !== 'companion' ? 'Boot Sequence' : currentMood.name}
                </span>
              )}
            </div>
            
            <div className="grid grid-cols-2 sm:grid-cols-3 gap-1.5 sm:gap-2">
              {MOODS.map((mood) => {
                const Icon = mood.icon;
                const active = bootMode === 'companion' && currentMood.id === mood.id;
                return (
                  <button
                    key={mood.id}
                    onClick={() => {
                      if (mood.id === 'intro') {
                        startBootSequence();
                      } else {
                        endBootSequence();
                        setCurrentMood(mood);
                      }
                    }}
                    className={`flex items-center gap-2 px-2.5 sm:px-3 py-1.5 sm:py-2 rounded-xl text-[11px] sm:text-xs font-medium border transition-all text-left cursor-pointer ${
                      active
                        ? 'bg-violet-600/30 border-violet-400 text-violet-200 font-semibold shadow-sm'
                        : 'bg-slate-900/60 border-slate-800 text-slate-400 hover:text-slate-200 hover:border-slate-700'
                    }`}
                  >
                    <Icon className={`w-3.5 h-3.5 shrink-0 ${active ? 'text-violet-400' : 'text-slate-500'}`} />
                    <span className="truncate">{mood.name}</span>
                  </button>
                );
              })}
            </div>
          </div>

          {/* Hardware Specs Minimal Pills */}
          <div className="pt-2 border-t border-white/[0.06] flex flex-wrap gap-1.5 sm:gap-2 text-[10px] sm:text-[11px] font-mono text-slate-400">
            <span className="px-2 sm:px-2.5 py-1 rounded-md bg-slate-900/80 border border-slate-800 text-violet-300">
              I2C: GPIO 8 / 9
            </span>
            <span className="px-2 sm:px-2.5 py-1 rounded-md bg-slate-900/80 border border-slate-800 text-fuchsia-300">
              Touch: GPIO 7
            </span>
            <span className="px-2 sm:px-2.5 py-1 rounded-md bg-slate-900/80 border border-slate-800 text-amber-300">
              Buzzer: GPIO 5
            </span>
            <span className="px-2 sm:px-2.5 py-1 rounded-md bg-slate-900/80 border border-slate-800 text-slate-400">
              Display: 0.96" SSD1306
            </span>
          </div>

        </div>

      </div>
    </div>
  );
}
