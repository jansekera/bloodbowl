/** Grid position on the Blood Bowl pitch (26 wide x 15 tall) */
export interface Position {
    x: number;
    y: number;
}

export interface PlayerStats {
    movement: number;
    strength: number;
    agility: number;
    armour: number;
}

export interface SkillInfo {
    id: number;
    name: string;
    category: string;
}

/** A player on the pitch during a match */
export interface MatchPlayer {
    id: number;
    playerId: number;
    name: string;
    number: number;
    positionalName: string;
    stats: PlayerStats;
    skills: string[];
    state: 'standing' | 'prone' | 'stunned' | 'ko' | 'injured' | 'dead' | 'ejected' | 'off_pitch';
    position: Position | null;
    hasMoved: boolean;
    hasActed: boolean;
    movementRemaining: number;
    teamSide: 'home' | 'away';
    lostTacklezones?: boolean;
    proUsedThisTurn?: boolean;
}

/** Ball state */
export interface BallState {
    position: Position | null;
    isHeld: boolean;
    carrierId: number | null;
}

/** Full game state sent from the server */
export interface GameState {
    matchId: number;
    half: number;
    activeTeam: 'home' | 'away';
    phase: 'coin_toss' | 'setup' | 'kickoff' | 'play' | 'touchdown' | 'half_time' | 'game_over';
    homeTeam: TeamState;
    awayTeam: TeamState;
    players: MatchPlayer[];
    ball: BallState;
    turnoverPending: boolean;
    kickingTeam: 'home' | 'away' | null;
    aiTeam: 'home' | 'away' | null;
    weather: string;
}

export interface TeamState {
    teamId: number;
    name: string;
    raceName: string;
    side: 'home' | 'away';
    score: number;
    rerolls: number;
    rerollUsedThisTurn: boolean;
    turnNumber: number;
    blitzUsedThisTurn: boolean;
    passUsedThisTurn: boolean;
    foulUsedThisTurn: boolean;
}

/** Move target with risk info */
export interface MoveTarget {
    x: number;
    y: number;
    dodges: number;
    gfis: number;
}

/** Action result from submitting an action */
export interface ActionResult {
    success: boolean;
    turnover: boolean;
    events: GameEvent[];
    state: GameState;
}

/** Game event from the event log */
export interface GameEvent {
    type: string;
    description: string;
    data: Record<string, unknown>;
}

/** Block target info */
export interface BlockTarget {
    playerId: number;
    name: string;
    x: number;
    y: number;
}

/** Available action for the UI */
export interface AvailableAction {
    type: string;
    playerId?: number;
}

/** Pass target with range info */
export interface PassTarget {
    x: number;
    y: number;
    range: string;
}

/** Foul target info */
export interface FoulTarget {
    playerId: number;
    name: string;
    x: number;
    y: number;
}

/** Hand-off target info */
export interface HandOffTarget {
    playerId: number;
    name: string;
    x: number;
    y: number;
}

/** Available skills for player advancement */
export interface AvailableSkills {
    normal: SkillInfo[];
    double: SkillInfo[];
    can_advance: boolean;
}

/** Team info from the API */
export interface Team {
    id: number;
    name: string;
    race_name: string;
    treasury: number;
    rerolls: number;
    player_count: number;
}

/** Race info from the API */
export interface Race {
    id: number;
    name: string;
    reroll_cost: number;
    has_apothecary: boolean;
    positionals: Positional[];
}

export interface Positional {
    id: number;
    name: string;
    max_count: number;
    cost: number;
    stats: PlayerStats;
    starting_skills: SkillInfo[];
}
