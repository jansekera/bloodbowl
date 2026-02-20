-- Blood Bowl: Initial Schema
-- Tables: skills, races, positional_templates, positional_template_skills, coaches

CREATE TABLE IF NOT EXISTS skills (
    id SERIAL PRIMARY KEY,
    name VARCHAR(100) NOT NULL UNIQUE,
    category VARCHAR(50) NOT NULL,
    description TEXT NOT NULL DEFAULT ''
);

CREATE TABLE IF NOT EXISTS races (
    id SERIAL PRIMARY KEY,
    name VARCHAR(100) NOT NULL UNIQUE,
    reroll_cost INTEGER NOT NULL,
    has_apothecary BOOLEAN NOT NULL DEFAULT TRUE
);

CREATE TABLE IF NOT EXISTS positional_templates (
    id SERIAL PRIMARY KEY,
    race_id INTEGER NOT NULL REFERENCES races(id) ON DELETE CASCADE,
    name VARCHAR(100) NOT NULL,
    max_count INTEGER NOT NULL,
    cost INTEGER NOT NULL,
    ma INTEGER NOT NULL,
    st INTEGER NOT NULL,
    ag INTEGER NOT NULL,
    av INTEGER NOT NULL,
    normal_access VARCHAR(100) NOT NULL DEFAULT '',
    double_access VARCHAR(100) NOT NULL DEFAULT '',
    UNIQUE(race_id, name)
);

CREATE TABLE IF NOT EXISTS positional_template_skills (
    id SERIAL PRIMARY KEY,
    positional_template_id INTEGER NOT NULL REFERENCES positional_templates(id) ON DELETE CASCADE,
    skill_id INTEGER NOT NULL REFERENCES skills(id) ON DELETE CASCADE,
    UNIQUE(positional_template_id, skill_id)
);

CREATE TABLE IF NOT EXISTS coaches (
    id SERIAL PRIMARY KEY,
    name VARCHAR(100) NOT NULL,
    email VARCHAR(255) NOT NULL UNIQUE,
    password_hash VARCHAR(255) NOT NULL,
    created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP
);

CREATE INDEX IF NOT EXISTS idx_positional_templates_race ON positional_templates(race_id);
CREATE INDEX IF NOT EXISTS idx_positional_template_skills_template ON positional_template_skills(positional_template_id);
CREATE INDEX IF NOT EXISTS idx_coaches_email ON coaches(email);
