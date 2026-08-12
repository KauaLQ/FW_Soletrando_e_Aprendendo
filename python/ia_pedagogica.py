import os
from dotenv import load_dotenv
from google import genai
from google.genai import types
from config import BASE_DIR
from schemas import RelatorioIA

load_dotenv(dotenv_path=BASE_DIR / ".env")

API_KEY = os.getenv("GEMINI_API_KEY")
if not API_KEY:
    raise ValueError("GEMINI_API_KEY não encontrada. Verifique o .env")

cliente = genai.Client(api_key=API_KEY)

# Configurável via .env para não depender de alterar código quando o
# Google aposentar o modelo de novo. Modelos disponíveis e prazos de desativação em:
# https://ai.google.dev/gemini-api/docs/deprecations
MODELO = os.getenv("GEMINI_MODEL")

def montar_prompt(tentativas: list[dict], nome_referencia: str, tipo_analise: str) -> str:
    """Monta o prompt com o papel do agente e os dados coletados do banco."""
    dados_formatados = "\n".join(
        f"- Esperado: '{t['palavra_esperada']}' | Transcrito: '{t['transcricao_audio']}' | "
        f"Resultado: {'Acertou' if t['resultado'] else 'Errou'}"
        for t in tentativas
    )
    return f"""
Você é um psicopedagogo especialista em alfabetização infantil e fonoaudiologia,
analisando dados de um jogo de soletração por voz.

Análise referente a: {nome_referencia} ({tipo_analise}).

Lista de tentativas registradas (palavra esperada vs. o que a criança falou,
transcrito por reconhecimento de voz):
{dados_formatados}

Tarefas:
1. Identifique padrões de erro fonético ou ortográfico recorrentes (troca de
   letras com som parecido, omissão de vogais, confusão CH/X, inversão de
   letras, etc). Ignore erros isolados sem repetição de padrão.
   IMPORTANTE: o campo "tipo" de cada padrão deve ser SEMPRE exatamente
   "fonetico" ou "ortografico" (sem acento, minúsculo) — o nome específico
   do padrão (ex: "Substituição de consoantes por proximidade fonética")
   vai no campo "descricao", nunca no "tipo".
2. Calcule o resumo de desempenho (total de tentativas, acertos, erros, taxa
   de acerto em %).
3. Sugira de 5 a 10 palavras novas para fixação, coerentes com o nível/faixa
   etária infantil, que reforcem exatamente os padrões de erro encontrados.
4. Sugira de 2 a 3 metodologias pedagógicas práticas e aplicáveis em sala de
   aula (ex: consciência fonológica, pares mínimos, jogos de trocas de letras)
   para mitigar as dificuldades encontradas.
5. Seja construtivo e encorajador no tom, como um profissional falando com
   o professor sobre o aluno.
""".strip()

def gerar_relatorio(tentativas: list[dict], nome_referencia: str, tipo_analise: str) -> RelatorioIA:
    """Chama o Gemini e já devolve uma instância validada de RelatorioIA
    (o SDK novo faz o parse pra Pydantic sozinho quando passamos a classe
    como response_schema)."""
    prompt = montar_prompt(tentativas, nome_referencia, tipo_analise)

    resposta = cliente.models.generate_content(
        model=MODELO,
        contents=prompt,
        config=types.GenerateContentConfig(
            response_mime_type="application/json",
            response_schema=RelatorioIA,
        ),
    )
    return resposta.parsed