import React, { useState, useRef } from 'react';
import { motion } from 'framer-motion';
import { 
  Zap, 
  CheckCircle2, 
  RotateCcw, 
  Sliders, 
  HelpCircle,
  HardDriveDownload,
  Flame,
  FileCode
} from 'lucide-react';
import confetti from 'canvas-confetti';
import { ESPLoader, Transport } from 'esptool-js';
import Terminal from './Terminal';

const DEFAULT_FIRMWARE_PARTS = [
  { name: 'Bootloader', path: '/firmware/bootloader.bin', address: 0x0, desc: 'ESP-IDF ROM Bootloader' },
  { name: 'Partition Table', path: '/firmware/partition-table.bin', address: 0x8000, desc: 'Partisi Flash (NVS, App)' },
  { name: 'CarMood App', path: '/firmware/carmood.bin', address: 0x10000, desc: 'Firmware Utama Desktop Companion' },
];

export default function Flasher() {
  const [activeTab, setActiveTab] = useState('esp-web-tools'); // 'esp-web-tools' | 'advanced'
  const [baudRate, setBaudRate] = useState(460800);
  const [status, setStatus] = useState('idle'); // 'idle' | 'connecting' | 'connected' | 'flashing' | 'success' | 'error'
  const [chipInfo, setChipInfo] = useState(null);
  const [progress, setProgress] = useState(0);
  const [currentFileStep, setCurrentFileStep] = useState('');
  const [logs, setLogs] = useState([
    '>> Mood ESP Web Flasher siap digunakan.',
    '>> Pastikan kabel USB terhubung ke modul ESP32-C3 / S3 Anda.'
  ]);

  // Custom user-uploaded firmware file
  const [customFile, setCustomFile] = useState(null);
  const [customOffset, setCustomOffset] = useState('0x10000');

  // References
  const transportRef = useRef(null);
  const esploaderRef = useRef(null);

  const addLog = (msg) => {
    setLogs((prev) => [...prev, msg]);
  };

  const clearLogs = () => {
    setLogs([]);
  };

  // Connect to ESP via Web Serial
  const handleConnect = async () => {
    if (!('serial' in navigator)) {
      alert('Browser Anda belum mendukung Web Serial API. Silakan gunakan Google Chrome atau Microsoft Edge di PC/Laptop.');
      return;
    }

    try {
      setStatus('connecting');
      addLog('>> Membuka dialog pemilih Serial Port...');

      const port = await navigator.serial.requestPort();
      addLog('>> Port terpilih! Menginisialisasi transport serial...');

      const transport = new Transport(port);
      transportRef.current = transport;

      const terminalObj = {
        clean: () => {},
        writeLine: (data) => addLog(data),
        write: (data) => addLog(data)
      };

      const loader = new ESPLoader({
        transport: transport,
        baudrate: baudRate,
        terminal: terminalObj
      });
      esploaderRef.current = loader;

      addLog(`>> Menghubungkan ke bootloader ESP pada baudrate ${baudRate}...`);
      const chip = await loader.main();
      
      const info = {
        chipName: chip,
        macAddr: loader.chip?.macAddr ? loader.chip.macAddr() : 'ESP32 Device'
      };
      setChipInfo(info);
      setStatus('connected');
      addLog(`== Berhasil terhubung! Chip terdeteksi: ${chip}`);
    } catch (err) {
      console.error(err);
      addLog(`[ERROR] Gagal terhubung: ${err.message || err}`);
      setStatus('error');
    }
  };

  const handleDisconnect = async () => {
    try {
      if (transportRef.current) {
        await transportRef.current.disconnect();
        transportRef.current = null;
        esploaderRef.current = null;
      }
      setStatus('idle');
      setChipInfo(null);
      addLog('>> Port serial telah diputuskan.');
    } catch (err) {
      addLog(`[ERROR] Gagal disconnect: ${err.message || err}`);
    }
  };

  // Start Flashing process
  const handleStartFlash = async () => {
    if (!esploaderRef.current) {
      addLog('[ERROR] Belum ada ESP32 yang terhubung!');
      return;
    }

    try {
      setStatus('flashing');
      setProgress(0);
      addLog('=======================================');
      addLog('>> MEMULAI PROSES FLASHING FIRMWARE...');
      addLog('=======================================');

      const filesToFlash = [];

      if (customFile) {
        // Flash custom file
        addLog(`>> Membaca file kustom: ${customFile.name} @ offset ${customOffset}...`);
        const arrayBuf = await customFile.arrayBuffer();
        filesToFlash.push({
          data: new Uint8Array(arrayBuf),
          address: parseInt(customOffset, 16)
        });
      } else {
        // Flash bundled default binaries
        for (const part of DEFAULT_FIRMWARE_PARTS) {
          addLog(`>> Mengunduh ${part.name} (${part.path}) @ offset 0x${part.address.toString(16)}...`);
          const res = await fetch(part.path);
          if (!res.ok) throw new Error(`Gagal mengunduh binary: ${part.path}`);
          const buf = await res.arrayBuffer();
          filesToFlash.push({
            data: new Uint8Array(buf),
            address: part.address
          });
        }
      }

      addLog(`>> Total file disiapkan: ${filesToFlash.length}. Menulis ke Flash ROM...`);

      const totalFiles = filesToFlash.length;

      await esploaderRef.current.writeFlash({
        fileArray: filesToFlash,
        flashMode: 'dio',
        flashFreq: '80m',
        flashSize: '4MB',
        eraseAll: false,
        compress: true,
        reportProgress: (fileIndex, written, total) => {
          const filePercent = Math.round((written / total) * 100);
          const currentPartName = customFile 
            ? customFile.name 
            : DEFAULT_FIRMWARE_PARTS[fileIndex]?.name || `Part ${fileIndex + 1}`;

          setCurrentFileStep(`${currentPartName}: ${filePercent}% (${Math.round(written/1024)}KB / ${Math.round(total/1024)}KB)`);
          
          const overallPercent = Math.round(((fileIndex + (written / total)) / totalFiles) * 100);
          setProgress(overallPercent);
        }
      });

      addLog('=======================================');
      addLog('🎉 FLASHING SELESAI DENGAN SUKSES! 🎉');
      addLog('>> Mereset chip ESP untuk menjalankan firmware baru...');
      addLog('=======================================');

      // Hard reset chip
      try {
        await esploaderRef.current.after('hard_reset');
      } catch (e) {
        // ignore reset error if port closes
      }

      setStatus('success');
      setProgress(100);
      confetti({
        particleCount: 120,
        spread: 80,
        origin: { y: 0.6 }
      });

    } catch (err) {
      console.error(err);
      addLog(`[ERROR FLASHING]: ${err.message || err}`);
      setStatus('error');
    }
  };

  return (
    <section id="flasher" className="py-8 scroll-mt-20 border-t border-white/[0.08]">
      
      {/* Tab Selector */}
      <div className="flex flex-col sm:flex-row sm:items-center justify-between gap-3 mb-5">
        <div>
          <span className="text-xs font-mono uppercase tracking-widest text-violet-400">
            Flasher
          </span>
          <h2 className="text-xl sm:text-2xl font-bold tracking-tight text-white mt-0.5">
            Web Firmware Flasher
          </h2>
        </div>

        {/* Tab switcher buttons */}
        <div className="w-full sm:w-auto flex p-1 bg-slate-900/80 border border-slate-800 rounded-xl text-xs font-mono">
          <button
            onClick={() => setActiveTab('esp-web-tools')}
            className={`flex-1 sm:flex-initial px-3 py-1.5 rounded-lg transition-all cursor-pointer text-center whitespace-nowrap ${
              activeTab === 'esp-web-tools'
                ? 'bg-violet-600/25 text-violet-200 border border-violet-500/40 font-semibold'
                : 'text-slate-400 hover:text-slate-200'
            }`}
          >
            1-Click Installer
          </button>
          <button
            onClick={() => setActiveTab('advanced')}
            className={`flex-1 sm:flex-initial px-3 py-1.5 rounded-lg transition-all cursor-pointer text-center whitespace-nowrap ${
              activeTab === 'advanced'
                ? 'bg-violet-600/25 text-violet-200 border border-violet-500/40 font-semibold'
                : 'text-slate-400 hover:text-slate-200'
            }`}
          >
            Manual Serial
          </button>
        </div>
      </div>

      {/* TAB 1: OFFICIAL ESP WEB TOOLS (1-CLICK INSTALL) */}
      {activeTab === 'esp-web-tools' && (
        <motion.div
          initial={{ opacity: 0, y: 8 }}
          animate={{ opacity: 1, y: 0 }}
          transition={{ duration: 0.2 }}
          className="rounded-2xl p-4 sm:p-6 lg:p-8 border border-white/[0.08] bg-slate-900/40 shadow-xl space-y-5"
        >
          <div className="max-w-xl mx-auto text-center space-y-4">
            
            {/* Firmware Parts Included */}
            <div className="bg-slate-950/70 border border-white/[0.06] rounded-xl p-3.5 sm:p-4 text-left font-mono">
              <div className="text-[11px] text-slate-400 uppercase tracking-wider mb-2 flex items-center gap-1.5">
                <FileCode className="w-3.5 h-3.5 text-violet-400" />
                <span>Firmware Bundled v1.0.0:</span>
              </div>
              <ul className="text-xs space-y-1.5 text-slate-300">
                <li className="flex justify-between border-b border-white/[0.04] pb-1">
                  <span>bootloader.bin</span>
                  <span className="text-violet-400">@ 0x0000</span>
                </li>
                <li className="flex justify-between border-b border-white/[0.04] pb-1">
                  <span>partition-table.bin</span>
                  <span className="text-violet-400">@ 0x8000</span>
                </li>
                <li className="flex justify-between">
                  <span>carmood.bin (App)</span>
                  <span className="text-violet-400">@ 0x10000</span>
                </li>
              </ul>
            </div>

            {/* Official Web Component Button */}
            <div className="pt-2 flex flex-col items-center justify-center">
              <esp-web-install-button manifest="./manifest.json">
                <button
                  slot="activate"
                  className="flex items-center justify-center gap-2 w-full sm:w-auto px-4 sm:px-6 py-2.5 sm:py-3 rounded-xl bg-violet-600 hover:bg-violet-500 text-white font-mono font-bold text-[11px] sm:text-xs shadow-sm transition-all active:scale-95 cursor-pointer"
                >
                  <Zap className="w-3.5 h-3.5 fill-current shrink-0" />
                  <span>HUBUNGKAN & FLASH FIRMWARE</span>
                </button>
                <div
                  slot="unsupported"
                  className="p-3 bg-amber-500/10 border border-amber-500/30 rounded-xl text-amber-300 text-xs font-mono max-w-md mx-auto"
                >
                  Browser belum mendukung Web Serial. Buka di Chrome atau Edge pada PC.
                </div>
              </esp-web-install-button>

              <span className="text-[11px] text-slate-500 font-mono mt-2.5">
                Pastikan ESP32 terhubung dengan kabel USB Data
              </span>
            </div>

          </div>
        </motion.div>
      )}

      {/* TAB 2: ADVANCED WEB SERIAL FLASHER (esptool-js) */}
      {activeTab === 'advanced' && (
        <motion.div
          initial={{ opacity: 0, y: 15 }}
          animate={{ opacity: 1, y: 0 }}
          transition={{ duration: 0.3 }}
          className="grid grid-cols-1 lg:grid-cols-12 gap-6"
        >
          {/* Controls Panel */}
          <div className="lg:col-span-6 space-y-5">
            <div className="glass-panel rounded-2xl p-4 sm:p-6 border border-white/10 shadow-xl space-y-4">
              
              <div className="flex items-center justify-between pb-3 border-b border-white/5">
                <span className="text-xs sm:text-sm font-semibold text-white flex items-center gap-2">
                  <Sliders className="w-4 h-4 text-violet-400 shrink-0" />
                  Konfigurasi Koneksi Serial
                </span>
                {chipInfo && (
                  <span className="px-2 py-0.5 rounded text-[10px] sm:text-[11px] font-mono bg-emerald-500/10 text-emerald-400 border border-emerald-500/20 truncate max-w-[140px]">
                    Online: {chipInfo.chipName}
                  </span>
                )}
              </div>

              {/* Baud Rate Selection */}
              <div>
                <label className="text-xs font-mono text-slate-400 uppercase tracking-wider block mb-1.5">
                  Baud Rate Flashing:
                </label>
                <select
                  value={baudRate}
                  disabled={status === 'connected' || status === 'flashing'}
                  onChange={(e) => setBaudRate(Number(e.target.value))}
                  className="w-full bg-slate-900 border border-slate-700 rounded-xl px-3 py-2 text-xs font-mono text-slate-200 focus:outline-none focus:border-violet-400"
                >
                  <option value={115200}>115200 (Stabil / Safe Mode)</option>
                  <option value={460800}>460800 (Rekomendasi Cepat)</option>
                  <option value={921600}>921600 (Ultra Fast)</option>
                </select>
              </div>

              {/* Connect / Disconnect Buttons */}
              <div className="flex gap-2">
                {status !== 'connected' && status !== 'flashing' ? (
                  <button
                    onClick={handleConnect}
                    disabled={status === 'connecting'}
                    className="flex-1 flex items-center justify-center gap-2 py-2.5 px-3 sm:px-4 rounded-xl bg-violet-600 hover:bg-violet-500 text-white font-semibold text-xs font-mono transition-all shadow-sm active:scale-95 cursor-pointer disabled:opacity-50"
                  >
                    <Zap className="w-4 h-4 shrink-0" />
                    <span>{status === 'connecting' ? 'Menghubungkan...' : '1. Pilih Port & Hubungkan'}</span>
                  </button>
                ) : (
                  <button
                    onClick={handleDisconnect}
                    disabled={status === 'flashing'}
                    className="flex-1 flex items-center justify-center gap-2 py-2.5 px-3 sm:px-4 rounded-xl bg-slate-800 hover:bg-slate-700 text-slate-300 font-semibold text-xs font-mono transition-all border border-slate-700 active:scale-95 cursor-pointer"
                  >
                    <RotateCcw className="w-4 h-4 shrink-0" />
                    <span>Putuskan Port (Disconnect)</span>
                  </button>
                )}
              </div>

              {/* Target Firmware Option */}
              <div className="pt-3 border-t border-white/5 space-y-3">
                <span className="text-xs font-mono text-slate-400 uppercase tracking-wider block">
                  Pilihan Firmware:
                </span>
                
                {/* Default Bundled */}
                <div 
                  onClick={() => setCustomFile(null)}
                  className={`p-3 rounded-xl border text-xs cursor-pointer transition-all ${
                    !customFile 
                      ? 'bg-violet-600/10 border-violet-400/50 text-violet-200' 
                      : 'bg-slate-900/50 border-slate-800 text-slate-400 hover:border-slate-700'
                  }`}
                >
                  <div className="font-semibold flex items-center justify-between">
                    <span>📦 Firmware Asli (DeskBuddy v1.0.0)</span>
                    {!customFile && <CheckCircle2 className="w-4 h-4 text-violet-400" />}
                  </div>
                  <p className="text-[11px] text-slate-500 mt-1">
                    Termasuk bootloader.bin (0x0), partition-table.bin (0x8000), & carmood.bin (0x10000).
                  </p>
                </div>

                {/* Custom File Upload */}
                <div className="p-3 rounded-xl border border-slate-800 bg-slate-900/30 text-xs space-y-2">
                  <div className="flex items-center justify-between text-slate-400 font-medium">
                    <span>📁 Atau Flash File Binary Sendiri (.bin):</span>
                    {customFile && (
                      <button 
                        onClick={() => setCustomFile(null)}
                        className="text-[10px] text-rose-400 hover:underline"
                      >
                        Batal
                      </button>
                    )}
                  </div>
                  <input
                    type="file"
                    accept=".bin"
                    onChange={(e) => {
                      if (e.target.files && e.target.files[0]) {
                        setCustomFile(e.target.files[0]);
                      }
                    }}
                    className="block w-full text-xs text-slate-400 file:mr-2 file:py-1 file:px-2.5 file:rounded-lg file:border-0 file:text-[11px] file:font-mono file:bg-slate-800 file:text-violet-400 hover:file:bg-slate-700 cursor-pointer"
                  />
                  {customFile && (
                    <div className="flex items-center gap-2 pt-1">
                      <span className="text-[11px] text-slate-400 font-mono">Flash Offset:</span>
                      <input
                        type="text"
                        value={customOffset}
                        onChange={(e) => setCustomOffset(e.target.value)}
                        placeholder="0x10000"
                        className="bg-slate-950 border border-slate-800 rounded px-2 py-0.5 text-[11px] font-mono text-violet-300 w-24"
                      />
                    </div>
                  )}
                </div>
              </div>

              {/* Start Flashing Button */}
              <button
                onClick={handleStartFlash}
                disabled={status !== 'connected' && status !== 'error'}
                className={`w-full py-3 px-4 rounded-xl font-bold text-xs font-mono transition-all flex items-center justify-center gap-2 cursor-pointer ${
                  status === 'connected' || status === 'error'
                    ? 'bg-gradient-to-r from-violet-600 to-indigo-600 hover:from-violet-500 hover:to-indigo-500 text-white shadow-card active:scale-95'
                    : 'bg-slate-800 text-slate-500 cursor-not-allowed border border-slate-700/50'
                }`}
              >
                <Flame className="w-4 h-4" />
                <span>
                  {status === 'flashing'
                    ? 'SEDANG MEMPROSES FLASHING...'
                    : status === 'connected'
                    ? '2. MULAI FLASH FIRMWARE KE ESP32'
                    : 'HUBUNGKAN PORT TERLEBIH DAHULU'}
                </span>
              </button>

              {/* Progress Bar (Visible during flashing or success) */}
              {(status === 'flashing' || status === 'success') && (
                <div className="pt-2 space-y-2">
                  <div className="flex justify-between text-xs font-mono">
                    <span className="text-violet-400 font-medium truncate max-w-[280px]">
                      {currentFileStep || 'Memproses...'}
                    </span>
                    <span className="text-slate-300 font-bold">{progress}%</span>
                  </div>
                  <div className="w-full h-2.5 bg-slate-950 rounded-full overflow-hidden border border-slate-800">
                    <motion.div
                      className="h-full bg-gradient-to-r from-violet-500 via-fuchsia-500 to-indigo-500"
                      initial={{ width: 0 }}
                      animate={{ width: `${progress}%` }}
                      transition={{ ease: 'easeOut', duration: 0.2 }}
                    />
                  </div>
                </div>
              )}

            </div>
          </div>

          {/* Right: Terminal Console Output */}
          <div className="lg:col-span-6 flex flex-col">
            <Terminal logs={logs} onClear={clearLogs} />
            
            {/* Quick Tips */}
            <div className="mt-3 p-3.5 rounded-xl bg-slate-900/60 border border-slate-800/80 text-xs space-y-1.5 text-slate-400 font-mono">
              <div className="flex items-center gap-1.5 text-violet-400 font-semibold">
                <HelpCircle className="w-3.5 h-3.5" />
                <span>Tips ESP32-C3 / S3 Bootloader:</span>
              </div>
              <p className="text-[11px] leading-relaxed">
                Jika serial tidak merespons koneksi: <strong>Tahan tombol BOOT → Tekan & lepas RESET → Lepas tombol BOOT</strong>. Perangkat akan masuk ke mode Download Bootloader.
              </p>
            </div>
          </div>

        </motion.div>
      )}

    </section>
  );
}
