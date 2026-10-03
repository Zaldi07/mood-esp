import React from 'react';
import logoImg from '../assets/logo.png';

export default function Footer() {
  return (
    <footer className="border-t border-white/[0.06] py-6 mt-8 text-slate-500 text-xs font-mono">
      <div className="max-w-7xl mx-auto px-4 lg:px-8 flex flex-col sm:flex-row items-center justify-between gap-3">
        <div className="flex items-center gap-2">
          <img src={logoImg} alt="DeskBuddy Logo" className="h-5 w-auto object-contain opacity-80" />
          <span className="text-slate-400">DeskBuddy V1 (Mood ESP)</span>
        </div>
        <div className="flex items-center gap-1 text-slate-500 text-[11px]">
          <span>ESP-IDF • Web Serial API</span>
        </div>
      </div>
    </footer>
  );
}
