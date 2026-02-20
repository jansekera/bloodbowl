import type { ActionResult, AvailableSkills, BlockTarget, FoulTarget, GameEvent, GameState, HandOffTarget, MoveTarget, PassTarget, Race, Team, AvailableAction } from './types';

const API_BASE = '/api/v1';

interface ApiResponse<T> {
    data: T;
    state?: GameState;
    _links?: Record<string, { href: string; method?: string }>;
}

interface ApiError {
    error?: string;
    errors?: string[];
}

export class ApiClient {
    private baseUrl: string;

    constructor(baseUrl: string = API_BASE) {
        this.baseUrl = baseUrl;
    }

    async getRaces(): Promise<Race[]> {
        const res = await this.get<ApiResponse<Race[]>>('/races');
        return res.data;
    }

    async getRace(id: number): Promise<Race> {
        const res = await this.get<ApiResponse<Race>>(`/races/${id}`);
        return res.data;
    }

    async getTeams(): Promise<Team[]> {
        const res = await this.get<ApiResponse<Team[]>>('/teams');
        return res.data;
    }

    async getTeam(id: number): Promise<Team> {
        const res = await this.get<ApiResponse<Team>>(`/teams/${id}`);
        return res.data;
    }

    async getMatchState(matchId: number): Promise<GameState> {
        const res = await this.get<ApiResponse<GameState>>(`/matches/${matchId}/state`);
        return res.data;
    }

    async submitAction(matchId: number, action: string, params: Record<string, unknown> = {}): Promise<ActionResult> {
        const res = await this.post<ApiResponse<ActionResult>>(`/matches/${matchId}/actions`, {
            action,
            params,
        });
        return { ...res.data, state: res.state! };
    }

    async getValidMoves(matchId: number, playerId: number): Promise<MoveTarget[]> {
        const res = await this.get<ApiResponse<MoveTarget[]>>(`/matches/${matchId}/players/${playerId}/moves`);
        return res.data;
    }

    async getBlockTargets(matchId: number, playerId: number): Promise<BlockTarget[]> {
        const res = await this.get<ApiResponse<BlockTarget[]>>(`/matches/${matchId}/players/${playerId}/block-targets`);
        return res.data;
    }

    async getPassTargets(matchId: number, playerId: number): Promise<PassTarget[]> {
        const res = await this.get<ApiResponse<PassTarget[]>>(`/matches/${matchId}/players/${playerId}/pass-targets`);
        return res.data;
    }

    async getFoulTargets(matchId: number, playerId: number): Promise<FoulTarget[]> {
        const res = await this.get<ApiResponse<FoulTarget[]>>(`/matches/${matchId}/players/${playerId}/foul-targets`);
        return res.data;
    }

    async getHandOffTargets(matchId: number, playerId: number): Promise<HandOffTarget[]> {
        const res = await this.get<ApiResponse<HandOffTarget[]>>(`/matches/${matchId}/players/${playerId}/handoff-targets`);
        return res.data;
    }

    async getAllEvents(matchId: number): Promise<GameEvent[]> {
        const res = await this.get<ApiResponse<GameEvent[]>>(`/matches/${matchId}/events?all=1`);
        return res.data;
    }

    async getAvailableSkills(playerId: number): Promise<AvailableSkills> {
        const res = await this.get<ApiResponse<AvailableSkills>>(`/players/${playerId}/available-skills`);
        return res.data;
    }

    async advancePlayer(playerId: number, skillId: number): Promise<unknown> {
        return this.post<ApiResponse<unknown>>(`/players/${playerId}/advance`, { skill_id: skillId });
    }

    async getAvailableActions(matchId: number): Promise<AvailableAction[]> {
        const res = await this.get<ApiResponse<AvailableAction[]>>(`/matches/${matchId}/actions`);
        return res.data;
    }

    private async get<T>(path: string): Promise<T> {
        const response = await fetch(`${this.baseUrl}${path}`, {
            headers: { 'Accept': 'application/json' },
        });

        if (!response.ok) {
            const error: ApiError = await response.json().catch(() => ({}));
            throw new ApiClientError(
                error.error ?? error.errors?.[0] ?? `HTTP ${response.status}`,
                response.status,
            );
        }

        return response.json();
    }

    private async post<T>(path: string, body: Record<string, unknown>): Promise<T> {
        const response = await fetch(`${this.baseUrl}${path}`, {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
                'Accept': 'application/json',
            },
            body: JSON.stringify(body),
        });

        if (!response.ok) {
            const error: ApiError = await response.json().catch(() => ({}));
            throw new ApiClientError(
                error.error ?? error.errors?.[0] ?? `HTTP ${response.status}`,
                response.status,
            );
        }

        return response.json();
    }
}

export class ApiClientError extends Error {
    constructor(
        message: string,
        public readonly statusCode: number,
    ) {
        super(message);
        this.name = 'ApiClientError';
    }
}
