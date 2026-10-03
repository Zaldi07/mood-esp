import React from 'react';
import { motion } from 'framer-motion';
import Header from './components/Header';
import OLEDCompanion from './components/OLEDCompanion';
import UserGuide from './components/UserGuide';
import Flasher from './components/Flasher';
import HardwarePinout from './components/HardwarePinout';
import Footer from './components/Footer';

export default function App() {
  return (
    <div className="min-h-screen flex flex-col bg-[#07090e] text-slate-100 selection:bg-cyan-500/20 selection:text-cyan-300">
      {/* Top Sticky Minimalist Navigation */}
      <Header />

      {/* Main Content Area */}
      <main className="flex-1 max-w-7xl mx-auto w-full px-4 lg:px-8 py-6 space-y-8">
        <OLEDCompanion />
        <UserGuide />
        <Flasher />
        <HardwarePinout />
      </main>

      {/* Footer */}
      <Footer />
    </div>
  );
}
