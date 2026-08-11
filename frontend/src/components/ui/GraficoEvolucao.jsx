import { LineChart, Line, XAxis, YAxis, Tooltip, ResponsiveContainer, CartesianGrid } from "recharts";

export default function GraficoEvolucao({ sessoes, alunoId }) {
  const dados = sessoes.map((s, i) => {
    const total = s.tentativas.length;
    const acertos = s.tentativas.filter((t) => t.resultado).length;
    return {
      sessao: `#${i + 1}`,
      precisao: total ? Math.round((acertos / total) * 100) : 0,
    };
  });

  return (
    <div className="w-full h-[180px] min-w-0">
      <ResponsiveContainer key={alunoId} width="100%" height="100%">
        <LineChart data={dados} margin={{ top: 8, right: 8, left: -20, bottom: 0 }}>
          <CartesianGrid stroke="#20253612" vertical={false} />
          <XAxis dataKey="sessao" tick={{ fontSize: 12, fill: "#20253680" }} axisLine={false} tickLine={false} />
          <YAxis domain={[0, 100]} tick={{ fontSize: 12, fill: "#20253680" }} axisLine={false} tickLine={false} width={36} />
          <Tooltip
            formatter={(valor) => [`${valor}%`, "Precisão"]}
            contentStyle={{ borderRadius: 12, border: "1px solid #2025361a", fontSize: 13 }}
          />
          <Line type="monotone" dataKey="precisao" stroke="#2f9e6e" strokeWidth={2} dot={{ r: 3, fill: "#2f9e6e" }} />
        </LineChart>
      </ResponsiveContainer>
    </div>
  );
}