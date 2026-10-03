import React, { useEffect, useState } from 'react';
import { motion } from 'framer-motion';
import { Cpu, Zap, ShieldAlert, CheckCircle2, BookOpen, Wrench, Play } from 'lucide-react';

export default function Header() {
  const [serialSupported, setSerialSupported] = useState(false);

  useEffect(() => {
    setSerialSupported('serial' in navigator);
  }, []);

  return (
    <header className="sticky top-0 z-50 backdrop-blur-xl bg-[#07090e]/85 border-b border-white/[0.08] px-4 lg:px-8 py-3">
      <div className="max-w-7xl mx-auto flex items-center justify-between">
        
        {/* Brand */}
        <div className="flex items-center gap-3">
          <div className="flex items-center justify-center w-9 h-9 rounded-xl bg-slate-900 border border-cyan-500/40 text-cyan-400 shadow-sm">
            <Cpu className="w-4 h-4" />
          </div>
          <div>
            <div className="flex items-center gap-2">
              <span className="font-bold text-base tracking-tight text-white">
                MOOD ESP
              </span>
              <span className="px-1.5 py-0.5 text-[9px] font-mono font-bold uppercase tracking-wider rounded bg-cyan-500/10 text-cyan-300 border border-cyan-500/20">
                ESP32-C3
              </span>
            </div>
            <p className="text-[11px] text-slate-400 hidden sm:block">
              Desktop Companion & Web Flasher
            </p>
          </div>
        </div>

        {/* Center Minimal Navigation */}
        <nav className="hidden md:flex items-center gap-6 text-xs font-mono text-slate-400">
          <a href="#simulator" className="hover:text-cyan-300 transition-colors">
            Simulator
          </a>
          <a href="#panduan" className="hover:text-cyan-300 transition-colors">
            Panduan
          </a>
          <a href="#flasher" className="hover:text-cyan-300 transition-colors">
            Web Flasher
          </a>
          <a href="#pinout" className="hover:text-cyan-300 transition-colors">
            Pinout
          </a>
        </nav>

        {/* Browser Serial Capability Badge */}
        <div className="flex items-center gap-2.5">
          {serialSupported ? (
            <div className="flex items-center gap-1.5 px-2.5 py-1 rounded-full bg-emerald-500/10 border border-emerald-500/20 text-emerald-400 text-[11px] font-mono">
              <CheckCircle2 className="w-3 h-3" />
              <span>Web Serial OK</span>
            </div>
          ) : (
            <div className="flex items-center gap-1.5 px-2.5 py-1 rounded-full bg-amber-500/10 border border-amber-500/20 text-amber-400 text-[11px] font-mono">
              <ShieldAlert className="w-3 h-3" />
              <span>Gunakan Chrome/Edge</span>
            </div>
          )}

          <a 
            href="#flasher" 
            className="flex items-center gap-1.5 px-3 py-1.5 rounded-lg bg-cyan-500/15 hover:bg-cyan-500/25 text-cyan-300 border border-cyan-500/30 text-xs font-mono font-medium transition-all shadow-sm"
          >
            <Zap className="w-3.5 h-3.5" />
            <span>Flash</span>
          </a>
        </div>

      </div>
    </header>
  );
}
