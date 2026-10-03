import React, { useRef, useEffect } from 'react';
import { Terminal as TerminalIcon, Trash2, Download, Check } from 'lucide-react';

export default function Terminal({ logs, onClear }) {
  const terminalEndRef = useRef(null);

  useEffect(() => {
    terminalEndRef.current?.scrollIntoView({ behavior: 'smooth' });
  }, [logs]);

  const handleDownload = () => {
    const text = logs.join('\n');
    const blob = new Blob([text], { type: 'text/plain' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = `mood-esp-flash-log-${new Date().toISOString().slice(0, 19)}.txt`;
    a.click();
    URL.revokeObjectURL(url);
  };

  return (
    <div className="rounded-xl border border-slate-800 bg-[#06080d] overflow-hidden shadow-2xl flex flex-col font-mono text-xs">
      {/* Terminal Title Bar */}
      <div className="flex items-center justify-between px-3 sm:px-4 py-2 sm:py-2.5 bg-slate-900/90 border-b border-slate-800/80 gap-2">
        <div className="flex items-center gap-1.5 sm:gap-2 min-w-0">
          <TerminalIcon className="w-3.5 h-3.5 text-cyan-400 shrink-0" />
          <span className="font-semibold text-slate-300 text-[10px] sm:text-[11px] tracking-wide truncate">
            SERIAL LOGS
          </span>
          <span className="px-1.5 py-0.5 rounded text-[9px] sm:text-[10px] bg-slate-800 text-slate-400 shrink-0">
            {logs.length}
          </span>
        </div>
        <div className="flex items-center gap-1 sm:gap-2 shrink-0">
          <button
            onClick={onClear}
            title="Bersihkan log"
            className="p-1 rounded text-slate-400 hover:text-slate-200 hover:bg-slate-800 transition-colors"
          >
            <Trash2 className="w-3.5 h-3.5" />
          </button>
          <button
            onClick={handleDownload}
            title="Download log file"
            className="p-1 rounded text-slate-400 hover:text-slate-200 hover:bg-slate-800 transition-colors"
          >
            <Download className="w-3.5 h-3.5" />
          </button>
        </div>
      </div>

      {/* Terminal Content */}
      <div className="p-4 h-64 overflow-y-auto space-y-1 select-text scrollbar-thin scrollbar-thumb-slate-800">
        {logs.length === 0 ? (
          <div className="text-slate-600 italic select-none">
            Belum ada aktivitas serial. Hubungkan perangkat Anda dan mulai proses flash...
          </div>
        ) : (
          logs.map((log, index) => {
            const isError = log.toLowerCase().includes('error') || log.toLowerCase().includes('fail');
            const isSuccess = log.toLowerCase().includes('success') || log.toLowerCase().includes('hash verified') || log.toLowerCase().includes('done') || log.toLowerCase().includes('berhasil');
            const isHighlight = log.startsWith('==') || log.startsWith('>>') || log.startsWith('[');

            return (
              <div
                key={index}
                className={`leading-relaxed break-all ${
                  isError
                    ? 'text-rose-400 font-semibold'
                    : isSuccess
                    ? 'text-emerald-400 font-medium'
                    : isHighlight
                    ? 'text-cyan-300'
                    : 'text-slate-400'
                }`}
              >
                {log}
              </div>
            );
          })
        )}
        <div ref={terminalEndRef} />
      </div>
    </div>
  );
}
