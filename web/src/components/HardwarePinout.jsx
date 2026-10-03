import React from 'react';
import { Wrench, Usb, Zap, Cpu } from 'lucide-react';

export default function HardwarePinout() {
  const pinouts = [
    { module: 'OLED Display (SSD1306 / SH1106)', type: 'I2C', sda: 'GPIO 8', scl: 'GPIO 9', vcc: '3.3V', gnd: 'GND', notes: 'Alamat 0x3C (0.96" 128×64)' },
    { module: 'LIS3DH Accelerometer', type: 'I2C', sda: 'GPIO 8', scl: 'GPIO 9', vcc: '3.3V', gnd: 'GND', notes: 'Sensor gerak / tap meja' },
    { module: 'Touch Sensor (TTP223)', type: 'Digital / Cap', sda: 'GPIO 7', scl: '-', vcc: '3.3V', gnd: 'GND', notes: 'Touch & Wakeup Sleep' },
    { module: 'Passive Buzzer', type: 'LEDC PWM', sda: 'GPIO 5', scl: '-', vcc: '3.3V', gnd: 'GND', notes: 'Chiptune intro & chime tidur' },
  ];

  return (
    <section id="pinout" className="py-8 border-t border-white/[0.08]">
      <div className="space-y-4">
        
        <div>
          <span className="text-xs font-mono uppercase tracking-widest text-emerald-400">
            Wiring
          </span>
          <h2 className="text-xl sm:text-2xl font-bold tracking-tight text-white mt-0.5">
            Skema Pinout Hardware
          </h2>
        </div>

        {/* Minimalist Pinout Table */}
        <div className="rounded-xl overflow-hidden border border-white/[0.08] bg-slate-900/60 shadow-lg">
          <div className="overflow-x-auto">
            <table className="w-full text-left font-mono text-xs">
              <thead className="bg-slate-950/80 text-slate-400 border-b border-white/[0.06]">
                <tr>
                  <th className="py-2.5 px-4">Modul Perangkat</th>
                  <th className="py-2.5 px-4">Bus</th>
                  <th className="py-2.5 px-4 text-violet-400">Signal / SDA</th>
                  <th className="py-2.5 px-4 text-violet-400">SCL</th>
                  <th className="py-2.5 px-4 text-emerald-400">Power</th>
                  <th className="py-2.5 px-4 text-slate-400">Keterangan</th>
                </tr>
              </thead>
              <tbody className="divide-y divide-white/[0.04] text-slate-300">
                {pinouts.map((pin, idx) => (
                  <tr key={idx} className="hover:bg-white/[0.02] transition-colors">
                    <td className="py-2.5 px-4 font-semibold text-white">
                      {pin.module}
                    </td>
                    <td className="py-2.5 px-4 text-slate-400">{pin.type}</td>
                    <td className="py-2.5 px-4 font-bold text-violet-300">{pin.sda}</td>
                    <td className="py-2.5 px-4 font-bold text-violet-300">{pin.scl}</td>
                    <td className="py-2.5 px-4 text-emerald-300">{pin.vcc} & {pin.gnd}</td>
                    <td className="py-2.5 px-4 text-slate-400 text-[11px]">{pin.notes}</td>
                  </tr>
                ))}
              </tbody>
            </table>
          </div>
        </div>

        {/* 3 Compact Chips */}
        <div className="grid grid-cols-1 sm:grid-cols-3 gap-2.5 pt-1 font-mono text-xs">
          <div className="p-3 rounded-xl bg-slate-900/40 border border-white/[0.06] flex items-center gap-2.5 text-slate-300">
            <Usb className="w-4 h-4 text-violet-400 shrink-0" />
            <span>Kabel USB Data (bukan charger-only)</span>
          </div>

          <div className="p-3 rounded-xl bg-slate-900/40 border border-white/[0.06] flex items-center gap-2.5 text-slate-300">
            <Zap className="w-4 h-4 text-fuchsia-400 shrink-0" />
            <span>Tahan BOOT jika flashing gagal</span>
          </div>

          <div className="p-3 rounded-xl bg-slate-900/40 border border-white/[0.06] flex items-center gap-2.5 text-slate-300">
            <Cpu className="w-4 h-4 text-emerald-400 shrink-0" />
            <span>Driver CH340 / CP210x jika COM tak muncul</span>
          </div>
        </div>

      </div>
    </section>
  );
}
