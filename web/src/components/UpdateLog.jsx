import React, { useState } from 'react';
import { motion, AnimatePresence } from 'framer-motion';
import { 
  History, 
  Tag, 
  Sparkles, 
  CheckCircle2, 
  Cpu, 
  Globe, 
  Zap, 
  Calendar,
  ChevronDown,
  ArrowRight
} from 'lucide-react';

const CHANGELOG_DATA = [
  {
    version: 'v1.1.0',
    tag: 'Terbaru',
    status: 'Stable',
    date: '3 Oktober 2026',
    summary: 'Mode Sleep hemat daya (0 mA), perbaikan audio chiptune, dan penyegaran UI responsif.',
    categories: [
      {
        type: 'firmware',
        title: 'Firmware ESP32-C3',
        icon: Cpu,
        color: 'text-violet-400 bg-violet-500/10 border-violet-500/20',
        items: [
          'Ultra Low Power Light Sleep: Tahan sensor sentuh GPIO 7 selama 5 detik untuk mematikan layar OLED total (0 mA konsumsi daya) tanpa saklar fisik.',
          'Bangun instan: Sentuh sensor 1x untuk membangunkan perangkat kembali ke mode aktif normal.',
          'Audio Space Chiptune: Perbaikan frekuensi square wave GPIO 5 dengan envelope ADSR yang lebih jernih dan bebas noise.',
          'Dukungan driver display SH1106: Kompensasi offset 2 kolom otomatis di samping SSD1306 standar.',
        ]
      },
      {
        type: 'web',
        title: 'Web Companion & Flasher',
        icon: Globe,
        color: 'text-fuchsia-400 bg-fuchsia-500/10 border-fuchsia-500/20',
        items: [
          'Identitas Visual Senada: Skema warna antarmuka diselaraskan dengan logo grafiti DeskBuddy tanpa efek glow artifisial.',
          'Full Responsive: Optimasi layout untuk semua perangkat mulai dari smartphone 360px hingga layar desktop lebar.',
          'Menu Navigasi Mobile: Penambahan menu hamburger responsif dengan akses cepat ke Simulator, Panduan, Flasher, dan Log Update.',
          'Halaman Log Update: Dokumentasi riwayat pembaruan firmware dan web secara berkala.'
        ]
      }
    ]
  },
  {
    version: 'v1.0.1',
    tag: 'Rilis Patch',
    status: 'Maintenance',
    date: '20 September 2026',
    summary: '26 kutipan Babel typewriter intro, kalibrasi sensor sentuh TTP223, dan optimasi flasher.',
    categories: [
      {
        type: 'firmware',
        title: 'Firmware ESP32-C3',
        icon: Cpu,
        color: 'text-violet-400 bg-violet-500/10 border-violet-500/20',
        items: [
          'Boot Sequence Babel: Penambahan 26 kutipan inspiratif novel Babel dengan efek typewriter bertahap saat menyalakan robot.',
          'Skip Interaktif: Sentuh sensor 1x untuk mempercepat atau melewati tampilan kutipan.',
          'Filter Debounce: Peningkatan akurasi deteksi sentuhan single tap (<0.4s), double tap, dan hold 1.5s.'
        ]
      },
      {
        type: 'web',
        title: 'Web Flasher',
        icon: Globe,
        color: 'text-fuchsia-400 bg-fuchsia-500/10 border-fuchsia-500/20',
        items: [
          'Integrasi ESP Web Tools 1-Click Installer resmi langsung dari browser.',
          'Manual Web Serial Flasher (esptool-js) dengan terminal log live dan baudrate hingga 460800.',
          'Dukungan flash custom binary file (.bin) dengan pilihan offset custom.'
        ]
      }
    ]
  },
  {
    version: 'v1.0.0',
    tag: 'Inisialisasi',
    status: 'Initial',
    date: '28 Agustus 2026',
    summary: 'Peluncuran perdana firmware DeskBuddy Companion dan sistem simulator web.',
    categories: [
      {
        type: 'firmware',
        title: 'Firmware ESP32-C3',
        icon: Cpu,
        color: 'text-violet-400 bg-violet-500/10 border-violet-500/20',
        items: [
          '18+ animasi ekspresi wajah robotik responsif berbasis kompresi RLE Flash.',
          '3 Mini-Game Retro terintegrasi: Pixel Flappy, Space Shooter, dan Brick Breaker 30 FPS.',
          'Zen Mode & Pomodoro Timer 25/5 menit dengan alarm buzzer GPIO 5.',
          'Sinkronisasi jam NTP otomatis melalui koneksi Wi-Fi 2.4 GHz.'
        ]
      },
      {
        type: 'hardware',
        title: 'Hardware & Wiring',
        icon: Zap,
        color: 'text-emerald-400 bg-emerald-500/10 border-emerald-500/20',
        items: [
          'Skema I2C bersama (GPIO 8 SDA, GPIO 9 SCL) untuk OLED dan accelerometer LIS3DH.',
          'Sensor sentuh kapasitif TTP223 pada GPIO 7.',
          'Buzzer pasif pada GPIO 5 dengan timer LEDC PWM hardware.'
        ]
      }
    ]
  }
];

export default function UpdateLog() {
  const [selectedFilter, setSelectedFilter] = useState('all'); // 'all' | 'firmware' | 'web' | 'hardware'
  const [expandedVersions, setExpandedVersions] = useState({
    'v1.1.0': true,
    'v1.0.1': false,
    'v1.0.0': false,
  });

  const toggleExpand = (ver) => {
    setExpandedVersions((prev) => ({
      ...prev,
      [ver]: !prev[ver]
    }));
  };

  const filterOptions = [
    { id: 'all', label: 'Semua Kategori' },
    { id: 'firmware', label: 'Firmware ESP32' },
    { id: 'web', label: 'Web Companion' },
    { id: 'hardware', label: 'Hardware' },
  ];

  return (
    <section id="updates" className="py-8 border-t border-white/[0.08] scroll-mt-20">
      <div className="space-y-5">
        
        {/* Section Header */}
        <div className="flex flex-col sm:flex-row sm:items-center justify-between gap-3">
          <div>
            <div className="flex items-center gap-2">
              <span className="text-xs font-mono uppercase tracking-widest text-violet-400 flex items-center gap-1.5">
                <History className="w-3.5 h-3.5" />
                Changelog
              </span>
            </div>
            <h2 className="text-xl sm:text-2xl font-bold tracking-tight text-white mt-0.5">
              Log Update & Catatan Rilis
            </h2>
            <p className="text-xs font-mono text-slate-400 mt-1">
              Riwayat pembaruan firmware ESP32 DeskBuddy dan aplikasi web
            </p>
          </div>

          {/* Filter Pills */}
          <div className="w-full sm:w-auto overflow-x-auto scrollbar-none flex items-center bg-slate-900/90 border border-slate-800 p-1 rounded-xl text-xs font-mono">
            {filterOptions.map((opt) => (
              <button
                key={opt.id}
                onClick={() => setSelectedFilter(opt.id)}
                className={`flex-1 sm:flex-initial px-3 py-1.5 rounded-lg transition-all cursor-pointer whitespace-nowrap text-center ${
                  selectedFilter === opt.id
                    ? 'bg-violet-600/30 text-violet-200 border border-violet-500/40 font-semibold'
                    : 'text-slate-400 hover:text-slate-200'
                }`}
              >
                {opt.label}
              </button>
            ))}
          </div>
        </div>

        {/* Timeline List */}
        <div className="space-y-4">
          {CHANGELOG_DATA.map((release) => {
            const isExpanded = expandedVersions[release.version];
            
            // Filter categories
            const filteredCategories = release.categories.filter((cat) => {
              if (selectedFilter === 'all') return true;
              return cat.type === selectedFilter;
            });

            // If filtered out entirely, hide release
            if (filteredCategories.length === 0) return null;

            return (
              <div
                key={release.version}
                className="rounded-2xl border border-white/[0.08] bg-[#120e24] overflow-hidden transition-all shadow-card"
              >
                {/* Release Card Header */}
                <div 
                  onClick={() => toggleExpand(release.version)}
                  className="p-4 sm:p-5 flex flex-col sm:flex-row sm:items-center justify-between gap-3 cursor-pointer hover:bg-white/[0.02] transition-colors select-none"
                >
                  <div className="flex items-center gap-3">
                    <span className="text-base sm:text-lg font-bold font-mono text-white">
                      {release.version}
                    </span>
                    <span className="px-2 py-0.5 text-[10px] font-mono font-bold uppercase tracking-wider rounded bg-violet-500/20 text-violet-300 border border-violet-500/30">
                      {release.tag}
                    </span>
                    <span className="hidden xs:inline-flex items-center gap-1 text-[11px] font-mono text-slate-400">
                      <Calendar className="w-3 h-3 text-slate-500" />
                      {release.date}
                    </span>
                  </div>

                  <div className="flex items-center justify-between sm:justify-end gap-3 text-xs font-mono text-slate-400">
                    <span className="xs:hidden text-[11px] text-slate-500">
                      {release.date}
                    </span>
                    <div className="flex items-center gap-1.5 text-violet-300 text-xs">
                      <span>{isExpanded ? 'Sembunyikan' : 'Rincian'}</span>
                      <ChevronDown className={`w-4 h-4 transition-transform duration-200 ${isExpanded ? 'rotate-180' : ''}`} />
                    </div>
                  </div>
                </div>

                {/* Release Summary Text */}
                <div className="px-4 sm:px-5 pb-3 -mt-1 text-xs text-slate-300 font-sans border-b border-white/[0.04]">
                  {release.summary}
                </div>

                {/* Expandable Content */}
                <AnimatePresence>
                  {isExpanded && (
                    <motion.div
                      initial={{ height: 0, opacity: 0 }}
                      animate={{ height: 'auto', opacity: 1 }}
                      exit={{ height: 0, opacity: 0 }}
                      transition={{ duration: 0.2 }}
                      className="overflow-hidden"
                    >
                      <div className="p-4 sm:p-5 space-y-4 bg-slate-950/40">
                        {filteredCategories.map((cat, idx) => {
                          const Icon = cat.icon;
                          return (
                            <div key={idx} className="space-y-2">
                              <div className="flex items-center gap-2">
                                <span className={`p-1 rounded-md border text-xs ${cat.color}`}>
                                  <Icon className="w-3.5 h-3.5" />
                                </span>
                                <h4 className="text-xs sm:text-sm font-semibold font-mono text-slate-200">
                                  {cat.title}
                                </h4>
                              </div>
                              <ul className="space-y-1.5 pl-6 text-xs text-slate-400 font-sans leading-relaxed list-disc">
                                {cat.items.map((item, itemIdx) => (
                                  <li key={itemIdx} className="marker:text-violet-400">
                                    {item}
                                  </li>
                                ))}
                              </ul>
                            </div>
                          );
                        })}

                        {/* Direct Flash CTA for latest version */}
                        {release.version === 'v1.1.0' && (
                          <div className="pt-2 flex items-center justify-between border-t border-white/[0.04]">
                            <span className="text-[11px] font-mono text-slate-400">
                              Pasang versi ini langsung via Web Serial
                            </span>
                            <a
                              href="#flasher"
                              className="inline-flex items-center gap-1.5 px-3 py-1.5 rounded-lg bg-violet-600 hover:bg-violet-500 text-white text-xs font-mono font-medium transition-all active:scale-95"
                            >
                              <span>Buka Flasher</span>
                              <ArrowRight className="w-3 h-3" />
                            </a>
                          </div>
                        )}
                      </div>
                    </motion.div>
                  )}
                </AnimatePresence>

              </div>
            );
          })}
        </div>

      </div>
    </section>
  );
}
