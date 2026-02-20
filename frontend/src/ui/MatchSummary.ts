import type { GameState } from '../api/types';

export interface MatchSummaryData {
    homeTeamName: string;
    awayTeamName: string;
    homeScore: number;
    awayScore: number;
    homeTouchdowns: string[];
    awayTouchdowns: string[];
    mvp: { name: string; team: string } | null;
}

/**
 * Match summary modal overlay shown at game end.
 */
export class MatchSummary {
    private overlay: HTMLElement;

    constructor(private container: HTMLElement) {
        this.overlay = document.createElement('div');
        this.overlay.className = 'match-summary-overlay';
        this.overlay.style.display = 'none';
        this.container.appendChild(this.overlay);

        // Close on click outside modal
        this.overlay.addEventListener('click', (e) => {
            if (e.target === this.overlay) {
                this.hide();
            }
        });
    }

    show(data: MatchSummaryData): void {
        const winner = data.homeScore > data.awayScore
            ? data.homeTeamName
            : data.awayScore > data.homeScore
                ? data.awayTeamName
                : null;

        const resultText = winner ? `${winner} wins!` : 'Draw!';

        this.overlay.innerHTML = `
            <div class="match-summary">
                <h2 class="match-summary__title">Match Over</h2>
                <div class="match-summary__result">${resultText}</div>
                <div class="match-summary__score">
                    <div class="match-summary__team">
                        <span class="match-summary__team-name">${data.homeTeamName}</span>
                        <span class="match-summary__team-score">${data.homeScore}</span>
                    </div>
                    <span class="match-summary__vs">-</span>
                    <div class="match-summary__team">
                        <span class="match-summary__team-score">${data.awayScore}</span>
                        <span class="match-summary__team-name">${data.awayTeamName}</span>
                    </div>
                </div>
                ${data.mvp ? `<div class="match-summary__mvp">MVP: ${data.mvp.name} (${data.mvp.team})</div>` : ''}
                <button class="match-summary__close">Close</button>
            </div>
        `;

        this.overlay.style.display = 'flex';

        const closeBtn = this.overlay.querySelector('.match-summary__close');
        closeBtn?.addEventListener('click', () => this.hide());
    }

    hide(): void {
        this.overlay.style.display = 'none';
    }

    destroy(): void {
        this.overlay.remove();
    }
}

/**
 * Extract summary data from game state.
 */
export function buildSummaryData(state: GameState): MatchSummaryData {
    return {
        homeTeamName: state.homeTeam.name,
        awayTeamName: state.awayTeam.name,
        homeScore: state.homeTeam.score,
        awayScore: state.awayTeam.score,
        homeTouchdowns: [],
        awayTouchdowns: [],
        mvp: null,
    };
}
