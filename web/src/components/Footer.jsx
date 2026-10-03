import React from 'react';
import { Cpu } from 'lucide-react';

export default function Footer() {
  return (
    <footer className="border-t border-white/5 py-8 mt-12 text-slate-500 text-xs font-mono">
      <div className="max-w-7xl mx-auto px-4 lg:px-8 flex flex-col sm:flex-row items-center justify-between gap-4">
        <div className="flex items-center gap-2">
          <Cpu className="w-4 h-4 text-cyan-400" />
          <span>Mood ESP — Robot Companion Project</span>
        </div>
        <div className="flex items-center gap-1 text-slate-400">
          <span>Powered by Web Serial API & ESP-IDF</span>
        </div>
      </div>
    </footer>
  );
}
