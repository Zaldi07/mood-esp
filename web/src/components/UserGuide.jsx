import React, { useState } from 'react';
import { motion } from 'framer-motion';
import { 
  BookOpen, 
  Hand, 
  Gamepad2, 
  HelpCircle, 
  Moon, 
  Zap, 
  Sliders,
  Smile,
  Timer
} from 'lucide-react';

export default function UserGuide() {
  const [activeTab, setActiveTab] = useState('gestures');

  const tabs = [
    { id: 'gestures', label: 'Kontrol Sentuh', icon: Hand },
    { id: 'features', label: 'Fitur', icon: Gamepad2 },
    { id: 'troubleshooting', label: 'Troubleshooting', icon: HelpCircle },
  ];

  return (
    <section id="panduan" className="py-8 border-t border-white/[0.08]">
      <div className="space-y-5">
        
        {/* Section Header */}
        <div className="flex flex-col sm:flex-row sm:items-center justify-between gap-3">
          <div>
            <div className="flex items-center gap-2">
              <span className="text-xs font-mono uppercase tracking-widest text-violet-400">
                Dokumentasi
              </span>
            </div>
            <h2 className="text-xl sm:text-2xl font-bold tracking-tight text-white mt-0.5">
              Panduan Operasional
            </h2>
          </div>

          {/* Minimalist Tab Selector */}
          <div className="flex items-center bg-slate-900/80 border border-slate-800 p-1 rounded-xl self-start sm:self-auto text-xs font-mono">
            {tabs.map((tab) => {
              const Icon = tab.icon;
              const isActive = activeTab === tab.id;
              return (
                <button
                  key={tab.id}
                  onClick={() => setActiveTab(tab.id)}
                  className={`flex items-center gap-1.5 px-3 py-1.5 rounded-lg transition-all cursor-pointer ${
                    isActive
                      ? 'bg-violet-600/25 text-violet-200 border border-violet-500/40 shadow-sm'
                      : 'text-slate-400 hover:text-slate-200'
                  }`}
                >
                  <Icon className="w-3.5 h-3.5" />
                  <span>{tab.label}</span>
                </button>
              );
            })}
          </div>
        </div>

        {/* Tab 1: Gestures */}
        {activeTab === 'gestures' && (
          <motion.div 
            initial={{ opacity: 0, y: 6 }}
            animate={{ opacity: 1, y: 0 }}
            className="grid grid-cols-1 sm:grid-cols-2 lg:grid-cols-4 gap-3 font-mono"
          >
            {/* 1. Tap */}
            <div className="p-4 rounded-xl bg-slate-900/50 border border-white/[0.06] flex flex-col justify-between">
              <div>
                <div className="flex items-center justify-between text-xs mb-2">
                  <span className="text-violet-400 font-bold">1x TAP (&lt;0.4s)</span>
                  <Hand className="w-3.5 h-3.5 text-violet-400" />
                </div>
                <div className="text-sm font-semibold text-white mb-2">Interaksi & Aksi</div>
                <ul className="text-xs text-slate-400 space-y-1">
                  <li>• Elus wajah robot</li>
                  <li>• Lompat / shoot di game</li>
                  <li>• Start / pause Pomodoro</li>
                </ul>
              </div>
              <div className="mt-3 pt-2 border-t border-white/[0.04] text-[10px] text-slate-500">
                GPIO 7 Single Click
              </div>
            </div>

            {/* 2. Double Tap */}
            <div className="p-4 rounded-xl bg-slate-900/50 border border-white/[0.06] flex flex-col justify-between">
              <div>
                <div className="flex items-center justify-between text-xs mb-2">
                  <span className="text-fuchsia-400 font-bold">2x TAP</span>
                  <Zap className="w-3.5 h-3.5 text-fuchsia-400" />
                </div>
                <div className="text-sm font-semibold text-white mb-2">Kembali / Reset</div>
                <ul className="text-xs text-slate-400 space-y-1">
                  <li>• Kembali ke menu sebelumnya</li>
                  <li>• Reset timer Pomodoro</li>
                  <li>• Batal aksi</li>
                </ul>
              </div>
              <div className="mt-3 pt-2 border-t border-white/[0.04] text-[10px] text-slate-500">
                Interval &lt; 380ms
              </div>
            </div>

            {/* 3. Hold 1.5s */}
            <div className="p-4 rounded-xl bg-slate-900/50 border border-white/[0.06] flex flex-col justify-between">
              <div>
                <div className="flex items-center justify-between text-xs mb-2">
                  <span className="text-amber-400 font-bold">TAHAN 1.5s</span>
                  <Sliders className="w-3.5 h-3.5 text-amber-400" />
                </div>
                <div className="text-sm font-semibold text-white mb-2">Menu Astra</div>
                <ul className="text-xs text-slate-400 space-y-1">
                  <li>• Buka daftar aplikasi</li>
                  <li>• Akses Game & Muyu</li>
                  <li>• Keluar dari mini-game</li>
                </ul>
              </div>
              <div className="mt-3 pt-2 border-t border-white/[0.04] text-[10px] text-slate-500">
                Hold Duration &ge; 1.2s
              </div>
            </div>

            {/* 4. Hold 5s Sleep */}
            <div className="p-4 rounded-xl bg-emerald-950/20 border border-emerald-500/30 flex flex-col justify-between">
              <div>
                <div className="flex items-center justify-between text-xs mb-2">
                  <span className="text-emerald-400 font-bold">TAHAN 5s</span>
                  <Moon className="w-3.5 h-3.5 text-emerald-400" />
                </div>
                <div className="text-sm font-semibold text-white mb-2">Power Sleep (0 mA)</div>
                <ul className="text-xs text-emerald-300/80 space-y-1">
                  <li>• Layar mati total (0 mA)</li>
                  <li>• Melodi tidur berbunyi</li>
                  <li>• <strong>Sentuh 1x</strong> untuk bangun</li>
                </ul>
              </div>
              <div className="mt-3 pt-2 border-t border-emerald-500/20 text-[10px] text-emerald-400/70">
                Pengganti Saklar Baterai
              </div>
            </div>
          </motion.div>
        )}

        {/* Tab 2: Features */}
        {activeTab === 'features' && (
          <motion.div 
            initial={{ opacity: 0, y: 6 }}
            animate={{ opacity: 1, y: 0 }}
            className="grid grid-cols-1 md:grid-cols-3 gap-3 font-mono"
          >
            <div className="p-4 rounded-xl bg-slate-900/50 border border-white/[0.06] space-y-2">
              <div className="flex items-center gap-2 text-violet-400">
                <Smile className="w-4 h-4" />
                <span className="text-sm font-semibold text-white">18+ Wajah Robot</span>
              </div>
              <p className="text-xs text-slate-400">
                Animasi ekspresi OLED responsif terhadap getaran meja (LIS3DH) dan sentuhan (TTP223).
              </p>
              <div className="text-[10px] text-violet-300 bg-violet-500/10 px-2 py-1 rounded border border-violet-500/20 inline-block">
                Kompresi RLE Flash
              </div>
            </div>

            <div className="p-4 rounded-xl bg-slate-900/50 border border-white/[0.06] space-y-2">
              <div className="flex items-center gap-2 text-fuchsia-400">
                <Gamepad2 className="w-4 h-4" />
                <span className="text-sm font-semibold text-white">3 Retro Games</span>
              </div>
              <p className="text-xs text-slate-400">
                Pixel Flappy, Space Shooter, dan Brick Breaker langsung di layar OLED 128×64.
              </p>
              <div className="text-[10px] text-fuchsia-300 bg-fuchsia-500/10 px-2 py-1 rounded border border-fuchsia-500/20 inline-block">
                Frame Rate 30 FPS
              </div>
            </div>

            <div className="p-4 rounded-xl bg-slate-900/50 border border-white/[0.06] space-y-2">
              <div className="flex items-center gap-2 text-emerald-400">
                <Timer className="w-4 h-4" />
                <span className="text-sm font-semibold text-white">Timer & Zen</span>
              </div>
              <p className="text-xs text-slate-400">
                Pomodoro kerja 25/5 menit dengan alarm buzzer GPIO 5, digital wooden fish, dan jam NTP Wi-Fi.
              </p>
              <div className="text-[10px] text-emerald-400/80 bg-emerald-500/10 px-2 py-1 rounded border border-emerald-500/20 inline-block">
                Buzzer LEDC PWM
              </div>
            </div>
          </motion.div>
        )}

        {/* Tab 3: Troubleshooting */}
        {activeTab === 'troubleshooting' && (
          <motion.div 
            initial={{ opacity: 0, y: 6 }}
            animate={{ opacity: 1, y: 0 }}
            className="grid grid-cols-1 md:grid-cols-2 gap-2.5 font-mono text-xs"
          >
            <div className="p-3.5 rounded-xl bg-slate-900/60 border border-white/[0.06] flex items-center justify-between gap-3">
              <div>
                <span className="text-amber-400 font-bold block">Garis vertikal di sisi kiri OLED</span>
                <span className="text-slate-400 text-[11px]">Display SH1106 butuh offset 2 kolom</span>
              </div>
              <code className="shrink-0 bg-slate-950 px-2 py-1 rounded border border-slate-800 text-violet-300 text-[11px]">
                OFFSET=2
              </code>
            </div>

            <div className="p-3.5 rounded-xl bg-slate-900/60 border border-white/[0.06] flex items-center justify-between gap-3">
              <div>
                <span className="text-emerald-400 font-bold block">Baterai tanpa saklar fisik</span>
                <span className="text-slate-400 text-[11px]">Matikan layar & masuk sleep mode</span>
              </div>
              <code className="shrink-0 bg-slate-950 px-2 py-1 rounded border border-slate-800 text-emerald-300 text-[11px]">
                Hold 5s GPIO 7
              </code>
            </div>

            <div className="p-3.5 rounded-xl bg-slate-900/60 border border-white/[0.06] flex items-center justify-between gap-3">
              <div>
                <span className="text-violet-400 font-bold block">Jam Wi-Fi gagal sinkron</span>
                <span className="text-slate-400 text-[11px]">ESP32 hanya mendukung 2.4 GHz</span>
              </div>
              <code className="shrink-0 bg-slate-950 px-2 py-1 rounded border border-slate-800 text-violet-300 text-[11px]">
                Wi-Fi 2.4 GHz
              </code>
            </div>

            <div className="p-3.5 rounded-xl bg-slate-900/60 border border-white/[0.06] flex items-center justify-between gap-3">
              <div>
                <span className="text-fuchsia-400 font-bold block">Flash partition overflow</span>
                <span className="text-slate-400 text-[11px]">Partisi app default 1MB terlampaui</span>
              </div>
              <code className="shrink-0 bg-slate-950 px-2 py-1 rounded border border-slate-800 text-fuchsia-300 text-[11px]">
                partitions.csv 3MB
              </code>
            </div>
          </motion.div>
        )}

      </div>
    </section>
  );
}
