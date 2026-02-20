-- Fáze 4: Match tables

DO $$ BEGIN
    IF NOT EXISTS (SELECT 1 FROM pg_type WHERE typname = 'match_status') THEN
        CREATE TYPE match_status AS ENUM ('setup', 'in_progress', 'finished', 'abandoned');
    END IF;
END $$;

CREATE TABLE IF NOT EXISTS matches (
    id SERIAL PRIMARY KEY,
    home_team_id INTEGER NOT NULL REFERENCES teams(id),
    away_team_id INTEGER NOT NULL REFERENCES teams(id),
    home_coach_id INTEGER NOT NULL REFERENCES coaches(id),
    away_coach_id INTEGER REFERENCES coaches(id), -- NULL for AI
    status match_status NOT NULL DEFAULT 'setup',
    home_score INTEGER NOT NULL DEFAULT 0,
    away_score INTEGER NOT NULL DEFAULT 0,
    current_half INTEGER NOT NULL DEFAULT 1,
    game_state JSONB, -- cached current GameState
    created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
    finished_at TIMESTAMP,
    CONSTRAINT different_teams CHECK (home_team_id != away_team_id)
);

CREATE TABLE IF NOT EXISTS match_events (
    id SERIAL PRIMARY KEY,
    match_id INTEGER NOT NULL REFERENCES matches(id) ON DELETE CASCADE,
    sequence_number INTEGER NOT NULL,
    event_type VARCHAR(50) NOT NULL,
    description TEXT NOT NULL,
    event_data JSONB NOT NULL DEFAULT '{}',
    created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE IF NOT EXISTS match_players (
    id SERIAL PRIMARY KEY,
    match_id INTEGER NOT NULL REFERENCES matches(id) ON DELETE CASCADE,
    player_id INTEGER NOT NULL REFERENCES players(id),
    team_side VARCHAR(4) NOT NULL CHECK (team_side IN ('home', 'away')),
    name VARCHAR(100) NOT NULL,
    number INTEGER NOT NULL,
    positional_name VARCHAR(50) NOT NULL,
    ma INTEGER NOT NULL,
    st INTEGER NOT NULL,
    ag INTEGER NOT NULL,
    av INTEGER NOT NULL,
    skills TEXT NOT NULL DEFAULT '[]', -- JSON array of skill names
    UNIQUE (match_id, player_id)
);

CREATE INDEX IF NOT EXISTS idx_matches_home_team ON matches(home_team_id);
CREATE INDEX IF NOT EXISTS idx_matches_away_team ON matches(away_team_id);
CREATE INDEX IF NOT EXISTS idx_matches_status ON matches(status);
CREATE INDEX IF NOT EXISTS idx_match_events_match ON match_events(match_id, sequence_number);
CREATE INDEX IF NOT EXISTS idx_match_players_match ON match_players(match_id);
