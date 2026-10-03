import React from 'react';
import { motion } from 'framer-motion';
import Header from './components/Header';
import OLEDCompanion from './components/OLEDCompanion';
import UserGuide from './components/UserGuide';
import Flasher from './components/Flasher';
import HardwarePinout from './components/HardwarePinout';
import UpdateLog from './components/UpdateLog';
import Footer from './components/Footer';

export default function App() {
  return (
    <div className="min-h-screen flex flex-col bg-[#0c0a18] text-slate-100 selection:bg-violet-600/30 selection:text-violet-200">
      {/* Top Sticky Minimalist Navigation */}
      <Header />

      {/* Main Content Area */}
      <main className="flex-1 max-w-7xl mx-auto w-full px-3.5 sm:px-6 lg:px-8 py-4 sm:py-6 space-y-6 sm:space-y-8 overflow-hidden">
        <OLEDCompanion />
        <UserGuide />
        <Flasher />
        <HardwarePinout />
        <UpdateLog />
      </main>

      {/* Footer */}
      <Footer />
    </div>
  );
}
