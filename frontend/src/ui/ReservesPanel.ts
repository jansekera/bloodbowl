import type { GameState, MatchPlayer } from '../api/types';

export type ReservesCallback = (playerId: number) => void;

export class ReservesPanel {
    private container: HTMLElement;
    private onSelect: ReservesCallback;
    private selectedPlayerId: number | null = null;

    constructor(container: HTMLElement, onSelect: ReservesCallback) {
        this.container = container;
        this.container.classList.add('reserves-panel');
        this.onSelect = onSelect;
    }

    setSelectedPlayer(playerId: number | null): void {
        this.selectedPlayerId = playerId;
        this.highlightSelected();
    }

    update(state: GameState | null, isPlayerTurn: boolean): void {
        if (!state || state.phase !== 'setup') {
            this.container.style.display = 'none';
            return;
        }

        this.container.style.display = '';

        const side = state.activeTeam;
        const offPitchPlayers = state.players.filter(
            p => p.teamSide === side && p.position === null,
        );
        const onPitchCount = state.players.filter(
            p => p.teamSide === side && p.position !== null,
        ).length;

        let html = `<h3 class="reserves-panel__title">Reserves (${offPitchPlayers.length})</h3>`;
        html += `<div class="reserves-panel__count">${onPitchCount}/11 placed</div>`;

        if (offPitchPlayers.length === 0) {
            html += `<div class="reserves-panel__empty">All players placed</div>`;
        } else {
            html += `<div class="reserves-panel__list">`;
            for (const player of offPitchPlayers) {
                const selected = player.id === this.selectedPlayerId ? ' reserves-player--selected' : '';
                html += this.renderPlayer(player, selected, isPlayerTurn);
            }
            html += `</div>`;
        }

        this.container.innerHTML = html;

        if (isPlayerTurn) {
            this.container.querySelectorAll('[data-player-id]').forEach(el => {
                el.addEventListener('click', () => {
                    const id = parseInt((el as HTMLElement).dataset.playerId ?? '0', 10);
                    if (id) this.onSelect(id);
                });
            });
        }
    }

    private renderPlayer(player: MatchPlayer, selectedClass: string, interactive: boolean): string {
        const cursor = interactive ? ' reserves-player--interactive' : '';
        const stats = player.stats;
        return `<div class="reserves-player${selectedClass}${cursor}" data-player-id="${player.id}">
            <span class="reserves-player__number">#${player.number}</span>
            <span class="reserves-player__name">${player.name}</span>
            <span class="reserves-player__pos">${player.positionalName}</span>
            <span class="reserves-player__stats">${stats.movement}/${stats.strength}/${stats.agility}/${stats.armour}</span>
        </div>`;
    }

    private highlightSelected(): void {
        this.container.querySelectorAll('.reserves-player').forEach(el => {
            const id = parseInt((el as HTMLElement).dataset.playerId ?? '0', 10);
            el.classList.toggle('reserves-player--selected', id === this.selectedPlayerId);
        });
    }
}
