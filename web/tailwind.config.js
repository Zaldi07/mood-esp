/** @type {import('tailwindcss').Config} */
export default {
  content: [
    "./index.html",
    "./src/**/*.{js,ts,jsx,tsx}",
  ],
  darkMode: 'class',
  theme: {
    extend: {
      colors: {
        oled: {
          bg: '#090714',
          card: '#110d24',
          surface: '#171233',
          border: '#2a1f4d',
          accent: '#8b5cf6',
          purple: '#7c3aed',
          violet: '#6d28d9',
          lilac: '#c4b5fd',
          green: '#10b981',
          amber: '#f59e0b',
          rose: '#f43f5e'
        }
      },
      fontFamily: {
        sans: ['"Plus Jakarta Sans"', 'system-ui', 'sans-serif'],
        mono: ['"JetBrains Mono"', 'monospace'],
      },
      boxShadow: {
        'glow-purple': '0 0 25px -5px rgba(139, 92, 246, 0.45)',
        'glow-violet': '0 0 30px -5px rgba(124, 58, 237, 0.5)',
        'glow-lilac': '0 0 20px -3px rgba(196, 181, 253, 0.4)',
        'glow-green': '0 0 25px -5px rgba(16, 185, 129, 0.35)',
        'glow-oled': '0 0 30px -5px rgba(139, 92, 246, 0.3)',
      },
      animation: {
        'pulse-subtle': 'pulse 3s cubic-bezier(0.4, 0, 0.6, 1) infinite',
        'scanline': 'scanline 8s linear infinite',
      },
      keyframes: {
        scanline: {
          '0%': { transform: 'translateY(-100%)' },
          '100%': { transform: 'translateY(1000%)' },
        }
      }
    },
  },
  plugins: [],
}
