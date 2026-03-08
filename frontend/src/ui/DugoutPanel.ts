import type { GameState, MatchPlayer } from '../api/types';

/**
 * Displays KO'd, Injured, and Dead players for both teams.
 */
export class DugoutPanel {
    private element: HTMLElement;

    constructor(container: HTMLElement) {
        this.element = document.createElement('div');
        this.element.className = 'dugout-panel';
        container.appendChild(this.element);
    }

    update(state: GameState): void {
        const homePlayers = state.players.filter(p => p.teamSide === 'home');
        const awayPlayers = state.players.filter(p => p.teamSide === 'away');

        this.element.innerHTML =
            this.renderTeam(state.homeTeam.name, homePlayers) +
            this.renderTeam(state.awayTeam.name, awayPlayers);
    }

    private renderTeam(teamName: string, players: MatchPlayer[]): string {
        const ko = players.filter(p => p.state === 'ko');
        const injured = players.filter(p => p.state === 'injured' || p.state === 'ejected');
        const dead = players.filter(p => p.state === 'dead');

        if (ko.length === 0 && injured.length === 0 && dead.length === 0) {
            return '';
        }

        let html = `<div class="dugout-panel__title">${this.escape(teamName)}</div>`;

        if (ko.length > 0) {
            html += this.renderSection('KO', 'ko', ko);
        }
        if (injured.length > 0) {
            html += this.renderSection('Injured', 'injured', injured);
        }
        if (dead.length > 0) {
            html += this.renderSection('Dead', 'dead', dead);
        }

        return html;
    }

    private renderSection(label: string, cssClass: string, players: MatchPlayer[]): string {
        let html = `<div class="dugout-section">`;
        html += `<div class="dugout-section__label dugout-section__label--${cssClass}">${label} (${players.length})</div>`;
        for (const p of players) {
            html += `<div class="dugout-player">#${p.number} ${this.escape(p.positionalName)}</div>`;
        }
        html += `</div>`;
        return html;
    }

    private escape(text: string): string {
        const div = document.createElement('div');
        div.textContent = text;
        return div.innerHTML;
    }

    destroy(): void {
        this.element.remove();
    }
}
