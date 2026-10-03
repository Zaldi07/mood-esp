import React from 'react';
import logoImg from '../assets/logo.png';

export default function Footer() {
  return (
    <footer className="border-t border-white/[0.06] py-6 mt-8 text-slate-500 text-xs font-mono">
      <div className="max-w-7xl mx-auto px-4 lg:px-8 flex flex-col sm:flex-row items-center justify-between gap-4">
        <div className="flex items-center justify-center sm:justify-start">
          <a href="#" className="p-1 rounded-xl bg-[#140f29] border border-violet-500/20 hover:border-violet-500/40 transition-colors" title="DeskBuddy">
            <img src={logoImg} alt="DeskBuddy" className="h-8 w-auto object-contain opacity-90 hover:opacity-100 transition-opacity" />
          </a>
        </div>
        <div className="flex flex-wrap items-center justify-center gap-2 text-slate-500 text-[11px]">
          <span>ESP32-C3 Firmware</span>
          <span>•</span>
          <span>ESP-IDF</span>
          <span>•</span>
          <span>Web Serial API</span>
        </div>
      </div>
    </footer>
  );
}
