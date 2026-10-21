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
    learnedSkills?: string[];
}

/** Ball state */
export interface BallState {
    position: Position | null;
    isHeld: boolean;
    carrierId: number | null;
}

/** Pending block dice waiting for player choice */
export interface PendingBlock {
    attackerId: number;
    defenderId: number;
    faces: string[];
    attackerChooses: boolean;
    isBlitz: boolean;
    isFrenzy: boolean;
    proAvailable: boolean;
    teamRerollAvailable: boolean;
    rerollUsed: boolean;
}

/** Pending reroll waiting for player choice (dodge/GFI/pickup) */
export interface PendingReroll {
    rollType: 'dodge' | 'gfi' | 'pickup';
    playerId: number;
    target: number;
    roll: number;
    proAvailable: boolean;
    teamRerollAvailable: boolean;
    targetX: number;
    targetY: number;
}

/** P206 (b): čekající volba o lékárníkovi */
export interface PendingApothecary {
    offers: { playerId: number; kind: 'ko' | 'casualty'; result: string | null; x: number | null; y: number | null; regeneration: boolean }[];
    turnover: boolean;
    stage: 'offer' | 'choose';
    chosenPlayerId: number | null;
    options: string[];
}

/** P224: čekající otázka, jestli útočník použije Grab (volná pole vedle obránce) */
export interface PendingGrab {
    kind: 'block' | 'bnc';
    attackerId: number;
    defenderId: number;
    squares: { x: number; y: number }[];
    /** index zvolené kostky v pendingBlock.faces (kind 'block') */
    faceIndex: number;
    /** zvolená strana kostky u Ball & Chain (kind 'bnc') */
    face: string | null;
}

/** P226: čekající otázka, jestli hráč kouče-člověka použije Stand Firm (playerId ≠ defenderId = řetěz) */
export interface PendingStandFirm {
    playerId: number;
    attackerId: number;
    defenderId: number;
    /** 'block' (Block/Blitz), 'bnc' (Ball & Chain), 'mb' (Multiple Block) */
    kind: 'block' | 'bnc' | 'mb';
    /** zvolená strana kostky (Pushed vs Knock-down, ř. 8511-8513) */
    face: string;
    /** index zvolené kostky v pendingBlock.faces (kind 'block') */
    faceIndex: number;
    /** kam by hráč bez Stand Firm šel; null = do davu */
    pushTo: { x: number; y: number } | null;
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
    pendingBlock?: PendingBlock | null;
    pendingReroll?: PendingReroll | null;
    /** P210: hráč, který ohlásil Blitz a ještě nedal ránu (chodí kamkoli, pak jedna rána). */
    declaredBlitzerId?: number | null;
    pendingApothecary?: PendingApothecary | null;
    pendingGrab?: PendingGrab | null;
    pendingStandFirm?: PendingStandFirm | null;
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

/** Single step in a movement path */
export interface PathStep {
    x: number;
    y: number;
    dodge: boolean;
    gfi: boolean;
    dodgeTarget?: number;
}

/** Move target with risk info */
export interface MoveTarget {
    x: number;
    y: number;
    dodges: number;
    gfis: number;
    path?: PathStep[];
    successChance?: number;
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
    diceCount: number;
    attackerChooses: boolean;
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
