/** @type {import('tailwindcss').Config} */
export default {
  content: ["./index.html", "./src/**/*.{js,jsx}"],
  theme: {
    extend: {
      colors: {
        ink: "#202536", // texto principal / painel escuro (capa de caderno)
        paper: "#FAF7F0", // fundo padrão
        "paper-dim": "#F1ECE1", // fundo secundário, divisórias sutis
        marker: {
          DEFAULT: "#2F9E6E", // ação primária
          dark: "#237A56",
        },
        chalk: "#F2A73B", // destaque secundário
        "chalk-blue": "#4C7EA8",
        danger: "#E0563F", // erros e ações destrutivas
      },
      fontFamily: {
        display: ['"Fraunces"', "serif"],
        sans: ['"Plus Jakarta Sans"', "sans-serif"],
        mono: ['"IBM Plex Mono"', "monospace"],
      },
      borderRadius: {
        tile: "10px",
      },
    },
  },
  plugins: [],
};
