-- Blood Bowl: migrace 004 -- validace výčtových sloupců        (30.08.2026)
--
-- Dva sloupce držely výčet jako VOLNÝ VARCHAR, tedy bez jakékoli kontroly:
--   skills.category         -- PHP enum `SkillCategory` existuje a používá se
--                              (`Skill::fromRow` volá `SkillCategory::from`),
--                              ale databáze přijala cokoli
--   match_events.event_type -- neměl ANI `CHECK`, ANI protějšek v PHP
--
-- ⚠️ Sousední sloupec `match_players.team_side` `CHECK` má, takže to nebyla
--    volba návrhu, ale mezera -- a mezera, která se nijak neprojeví: překlep
--    se uloží a nikdo se to nedozví.
--
-- ⭐ PROČ `CHECK` A NE NATIVNÍ `CREATE TYPE ... AS ENUM`:
--    seznam typů událostí ROSTE s každým novým pravidlem (`hand_off` 14.08.,
--    `stand_up` 21.08., `follow_up` 28.08.). U nativního typu je přidání
--    `ALTER TYPE ... ADD VALUE`, ale přejmenování nebo odebrání znamená
--    přestavět typ i všechny závislé sloupce. `CHECK` se zahodí a založí
--    znovu jednou migrací.
--
-- ⛔ SEZNAM SE JEN DOPLŇUJE, NIKDY NEPŘEJMENOVÁVÁ -- v už uložených řádcích
--    by se tím tiše změnil význam. Táž rodina jako `GameEvent::Type` v C++
--    enginu ("APPEND ONLY, or every event in already-collected corpora gets
--    renamed") a jako indexy v `MATCHUPS[]`.
--
-- ⭐ ZDROJ PRAVDY JE PHP ENUM `App\Enum\GameEventType`, ne tenhle soubor.
--    Že spolu souhlasí, hlídá `tests/Enum/GameEventTypeTest.php` -- bez toho
--    by se rozešly při prvním novém eventu, na který někdo zapomene migraci.

-- 1) skills.category -- 6 hodnot podle App\Enum\SkillCategory
ALTER TABLE skills DROP CONSTRAINT IF EXISTS skills_category_check;
ALTER TABLE skills ADD CONSTRAINT skills_category_check
    CHECK (category IN (
        'General', 'Agility', 'Strength', 'Passing', 'Mutation', 'Extraordinary'
    ));

-- 2) match_events.event_type -- 81 hodnot podle App\Enum\GameEventType
ALTER TABLE match_events DROP CONSTRAINT IF EXISTS match_events_event_type_check;
ALTER TABLE match_events ADD CONSTRAINT match_events_event_type_check
    CHECK (event_type IN (
        'always_hungry', 'animosity', 'apothecary', 'armour_roll',
        'ball_and_chain_block', 'ball_and_chain_move', 'ball_bounce', 'block',
        'bloodlust_bite', 'bloodlust_fail', 'bomb_explosion', 'bomb_landing',
        'bomb_throw', 'bone_head', 'catch', 'chain_push',
        'chainsaw', 'chainsaw_kickback', 'crowd_surf', 'diving_catch',
        'diving_tackle', 'dodge', 'dump_off', 'ejection',
        'end_turn', 'fend', 'follow_up', 'foul',
        'foul_appearance', 'frenzy', 'game_over', 'gfi',
        'hail_mary_pass', 'half_time', 'hand_off', 'hypnotic_gaze',
        'injury_roll', 'interception', 'juggernaut', 'kick_off_return',
        'kick_skill', 'kickoff', 'kickoff_table', 'ko_recovery',
        'leader', 'leap', 'loner', 'multiple_block',
        'no_hands', 'nurgles_rot', 'pass', 'pass_block',
        'pickup', 'piling_on', 'player_fell', 'player_move',
        'pro', 'push', 'really_stupid', 'regeneration',
        'reroll', 'safe_throw', 'secret_weapon', 'shadowing',
        'sneaky_git', 'stab', 'stakes_block_regen', 'stand_up',
        'strip_ball', 'sweltering_heat', 'take_root', 'tentacles',
        'throw_in', 'throw_team_mate', 'touchback', 'touchdown',
        'ttm_landing', 'turnover', 'weather_change', 'wild_animal',
        'wrestle'
    ));
