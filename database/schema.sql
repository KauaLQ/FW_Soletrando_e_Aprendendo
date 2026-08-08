-- schema.sql
-- Script equivalente aos modelos SQLModel (models.py).
-- Use apenas se preferir criar as tabelas manualmente em vez de rodar
-- database.py (que faz isso automaticamente via SQLModel.metadata.create_all).

CREATE TABLE IF NOT EXISTS professores (
    id            SERIAL PRIMARY KEY,
    nome          VARCHAR NOT NULL,
    email         VARCHAR NOT NULL UNIQUE,
    senha_hash    VARCHAR NOT NULL,
    criado_em     TIMESTAMP NOT NULL DEFAULT NOW()
);

CREATE TABLE IF NOT EXISTS turmas (
    id            SERIAL PRIMARY KEY,
    nome          VARCHAR NOT NULL,
    ano_letivo    INTEGER NOT NULL,
    professor_id  INTEGER NOT NULL REFERENCES professores(id)
);

CREATE TABLE IF NOT EXISTS alunos (
    id            SERIAL PRIMARY KEY,
    nome          VARCHAR NOT NULL,
    turma_id      INTEGER NOT NULL REFERENCES turmas(id)
);

CREATE TABLE IF NOT EXISTS sessoes (
    id              SERIAL PRIMARY KEY,
    aluno_id        INTEGER NOT NULL REFERENCES alunos(id),
    data_inicio     TIMESTAMP NOT NULL DEFAULT NOW(),
    nivel_atingido  INTEGER NOT NULL DEFAULT 1,
    pin_pareamento  VARCHAR NOT NULL
);

CREATE TABLE IF NOT EXISTS tentativas (
    id                      SERIAL PRIMARY KEY,
    sessao_id               INTEGER NOT NULL REFERENCES sessoes(id),
    palavra_esperada        VARCHAR NOT NULL,
    transcricao_audio       VARCHAR NOT NULL,
    resultado               BOOLEAN NOT NULL,
    distancia_levenshtein   INTEGER NOT NULL,
    tempo_audio             DOUBLE PRECISION NOT NULL,
    criado_em               TIMESTAMP NOT NULL DEFAULT NOW(),
    audio                   VARCHAR
);

-- Índices auxiliares para as buscas mais comuns
CREATE INDEX IF NOT EXISTS idx_turmas_professor_id ON turmas(professor_id);
CREATE INDEX IF NOT EXISTS idx_alunos_turma_id ON alunos(turma_id);
CREATE INDEX IF NOT EXISTS idx_sessoes_aluno_id ON sessoes(aluno_id);
CREATE INDEX IF NOT EXISTS idx_sessoes_pin_pareamento ON sessoes(pin_pareamento);
CREATE INDEX IF NOT EXISTS idx_tentativas_sessao_id ON tentativas(sessao_id);
