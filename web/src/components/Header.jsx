import React, { useEffect, useState } from 'react';
import { motion, AnimatePresence } from 'framer-motion';
import { Zap, ShieldAlert, CheckCircle2, Menu, X } from 'lucide-react';
import logoImg from '../assets/logo.png';

export default function Header() {
  const [serialSupported, setSerialSupported] = useState(false);
  const [mobileMenuOpen, setMobileMenuOpen] = useState(false);

  useEffect(() => {
    setSerialSupported('serial' in navigator);
  }, []);

  const navLinks = [
    { name: 'Simulator', href: '#simulator' },
    { name: 'Panduan', href: '#panduan' },
    { name: 'Web Flasher', href: '#flasher' },
    { name: 'Pinout', href: '#pinout' },
    { name: 'Log Update', href: '#updates' },
  ];

  return (
    <header className="sticky top-0 z-50 backdrop-blur-xl bg-[#0c0a18]/90 border-b border-white/[0.08] px-3 sm:px-6 lg:px-8 py-2 transition-all">
      <div className="max-w-7xl mx-auto flex items-center justify-between gap-2">

        {/* Brand Logo Only (No Text) */}
        <a
          href="#"
          className=""
          title="DeskBuddy"
        >
          <img
            src={logoImg}
            alt="DeskBuddy Logo"
            className="h-10 lg:h-20 w-auto object-contain filter group-hover:scale-105 transition-transform"
          />
        </a>

        {/* Center Desktop Navigation */}
        <nav className="hidden md:flex items-center gap-6 text-xs font-mono text-slate-400">
          {navLinks.map((link) => (
            <a
              key={link.name}
              href={link.href}
              className="hover:text-violet-300 transition-colors"
            >
              {link.name}
            </a>
          ))}
        </nav>

        {/* Right Section: Serial Badge + Action Buttons */}
        <div className="flex items-center gap-2 sm:gap-2.5 md:hidden">

          <button
            onClick={() => setMobileMenuOpen(!mobileMenuOpen)}
            className="md:hidden flex items-center justify-center p-1.5 rounded-lg bg-slate-900 border border-slate-800 text-slate-400 hover:text-white hover:border-slate-700 transition-colors"
            aria-label="Toggle navigation menu"
          >
            {mobileMenuOpen ? <X className="w-4 h-4" /> : <Menu className="w-4 h-4" />}
          </button>
        </div>

      </div>

      {/* Mobile Navigation Dropdown */}
      <AnimatePresence>
        {mobileMenuOpen && (
          <motion.div
            initial={{ opacity: 0, height: 0 }}
            animate={{ opacity: 1, height: 'auto' }}
            exit={{ opacity: 0, height: 0 }}
            transition={{ duration: 0.2 }}
            className="md:hidden overflow-hidden border-t border-white/[0.06] mt-2.5 pt-2 pb-1"
          >
            <nav className="flex flex-col space-y-1 font-mono text-xs">
              {navLinks.map((link) => (
                <a
                  key={link.name}
                  href={link.href}
                  onClick={() => setMobileMenuOpen(false)}
                  className="px-3 py-2 rounded-lg text-slate-300 hover:text-white hover:bg-violet-600/20 hover:border-violet-500/30 transition-all"
                >
                  {link.name}
                </a>
              ))}
            </nav>
          </motion.div>
        )}
      </AnimatePresence>
    </header>
  );
}
