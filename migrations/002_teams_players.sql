-- Blood Bowl: Teams and Players
-- Tables: teams, players, player_skills

DO $$ BEGIN
    IF NOT EXISTS (SELECT 1 FROM pg_type WHERE typname = 'team_status') THEN
        CREATE TYPE team_status AS ENUM ('active', 'retired');
    END IF;
    IF NOT EXISTS (SELECT 1 FROM pg_type WHERE typname = 'player_status') THEN
        CREATE TYPE player_status AS ENUM ('active', 'injured', 'dead', 'retired');
    END IF;
END $$;

CREATE TABLE IF NOT EXISTS teams (
    id SERIAL PRIMARY KEY,
    coach_id INTEGER NOT NULL REFERENCES coaches(id) ON DELETE CASCADE,
    race_id INTEGER NOT NULL REFERENCES races(id),
    name VARCHAR(100) NOT NULL,
    treasury INTEGER NOT NULL DEFAULT 1000000,
    rerolls INTEGER NOT NULL DEFAULT 0,
    fan_factor INTEGER NOT NULL DEFAULT 0,
    has_apothecary BOOLEAN NOT NULL DEFAULT FALSE,
    assistant_coaches INTEGER NOT NULL DEFAULT 0,
    cheerleaders INTEGER NOT NULL DEFAULT 0,
    status team_status NOT NULL DEFAULT 'active',
    created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE IF NOT EXISTS players (
    id SERIAL PRIMARY KEY,
    team_id INTEGER NOT NULL REFERENCES teams(id) ON DELETE CASCADE,
    positional_template_id INTEGER NOT NULL REFERENCES positional_templates(id),
    name VARCHAR(100) NOT NULL,
    number INTEGER NOT NULL,
    ma INTEGER NOT NULL,
    st INTEGER NOT NULL,
    ag INTEGER NOT NULL,
    av INTEGER NOT NULL,
    spp INTEGER NOT NULL DEFAULT 0,
    level INTEGER NOT NULL DEFAULT 1,
    status player_status NOT NULL DEFAULT 'active',
    created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
    UNIQUE(team_id, number)
);

CREATE TABLE IF NOT EXISTS player_skills (
    id SERIAL PRIMARY KEY,
    player_id INTEGER NOT NULL REFERENCES players(id) ON DELETE CASCADE,
    skill_id INTEGER NOT NULL REFERENCES skills(id),
    is_starting BOOLEAN NOT NULL DEFAULT FALSE,
    UNIQUE(player_id, skill_id)
);

CREATE INDEX IF NOT EXISTS idx_teams_coach ON teams(coach_id);
CREATE INDEX IF NOT EXISTS idx_teams_race ON teams(race_id);
CREATE INDEX IF NOT EXISTS idx_players_team ON players(team_id);
CREATE INDEX IF NOT EXISTS idx_player_skills_player ON player_skills(player_id);
