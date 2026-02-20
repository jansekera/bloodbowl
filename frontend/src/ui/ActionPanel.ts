import type { GameState } from '../api/types';

export type ActionCallback = (action: string) => void;

export class ActionPanel {
    private container: HTMLElement;
    private onAction: ActionCallback;
    private selectedMode: string | null = null;
    private _firstTargetSelected = false;

    constructor(container: HTMLElement, onAction: ActionCallback) {
        this.container = container;
        this.container.classList.add('action-panel');
        this.onAction = onAction;
    }

    getSelectedMode(): string | null {
        return this.selectedMode;
    }

    setSelectedMode(mode: string | null): void {
        this.selectedMode = mode;
        this._firstTargetSelected = false;
    }

    setFirstTargetSelected(selected: boolean): void {
        this._firstTargetSelected = selected;
    }

    update(state: GameState | null, isPlayerTurn: boolean): void {
        if (!state) {
            this.container.innerHTML = '';
            return;
        }

        const teamState = state.activeTeam === 'home' ? state.homeTeam : state.awayTeam;
        const phase = state.phase;

        let html = `<div class="action-panel__info">`;
        html += `<span class="action-panel__phase">${this.formatPhase(phase)}</span>`;

        if (phase === 'game_over') {
            const homeScore = state.homeTeam.score;
            const awayScore = state.awayTeam.score;
            const winner = homeScore > awayScore ? state.homeTeam.name
                : awayScore > homeScore ? state.awayTeam.name : null;
            html += `<span class="action-panel__score">${state.homeTeam.name} ${homeScore} - ${awayScore} ${state.awayTeam.name}</span>`;
            html += `<span class="action-panel__winner">${winner ? `${winner} wins!` : 'Draw!'}</span>`;
        } else {
            html += `<span class="action-panel__turn">Turn ${teamState.turnNumber}/8</span>`;
            html += `<span class="action-panel__rerolls">Rerolls: ${teamState.rerolls}</span>`;
            html += `<span class="action-panel__team">${teamState.name}'s turn</span>`;
        }
        html += `</div>`;

        if (phase === 'touchdown') {
            html += `<div class="action-panel__message action-panel__message--touchdown">TOUCHDOWN!</div>`;
        }

        if (isPlayerTurn) {
            html += `<div class="action-panel__buttons">`;
            if (phase === 'play') {
                const blockActive = this.selectedMode === 'block' ? ' btn--active' : '';
                const multiBlockActive = this.selectedMode === 'multiple_block' ? ' btn--active' : '';
                const blitzActive = this.selectedMode === 'blitz' ? ' btn--active' : '';
                const passActive = this.selectedMode === 'pass' ? ' btn--active' : '';
                const handoffActive = this.selectedMode === 'handoff' ? ' btn--active' : '';
                const foulActive = this.selectedMode === 'foul' ? ' btn--active' : '';
                const blitzDisabled = teamState.blitzUsedThisTurn ? ' disabled' : '';
                const passDisabled = teamState.passUsedThisTurn ? ' disabled' : '';
                const foulDisabled = teamState.foulUsedThisTurn ? ' disabled' : '';

                html += `<button class="btn btn--small btn--secondary${blockActive}" data-action="toggle_block">Block</button>`;
                html += `<button class="btn btn--small btn--secondary${multiBlockActive}" data-action="toggle_multiple_block">Multi Block</button>`;
                html += `<button class="btn btn--small btn--secondary${blitzActive}" data-action="toggle_blitz"${blitzDisabled}>Blitz</button>`;
                html += `<button class="btn btn--small btn--secondary${passActive}" data-action="toggle_pass"${passDisabled}>Pass</button>`;
                html += `<button class="btn btn--small btn--secondary${handoffActive}" data-action="toggle_handoff">Hand-off</button>`;
                html += `<button class="btn btn--small btn--secondary${foulActive}" data-action="toggle_foul"${foulDisabled}>Foul</button>`;
                html += `<button class="btn btn--small btn--danger" data-action="end_turn">End Turn</button>`;
            }
            if (phase === 'setup') {
                html += `<button class="btn btn--small btn--primary" data-action="end_setup">Done Setup</button>`;
            }
            html += `</div>`;

            if (this.selectedMode) {
                const hintText = this.getHintText(this.selectedMode);
                html += `<div class="action-panel__hint">${hintText}</div>`;
            }
        }

        this.container.innerHTML = html;

        // Bind button events
        this.container.querySelectorAll('[data-action]').forEach(btn => {
            btn.addEventListener('click', () => {
                const action = (btn as HTMLElement).dataset.action;
                if (action) this.onAction(action);
            });
        });
    }

    private getHintText(mode: string): string {
        switch (mode) {
            case 'block': return 'Click an enemy player to block';
            case 'multiple_block':
                return this._firstTargetSelected
                    ? 'Select second target to block'
                    : 'Select first target to block';
            case 'blitz': return 'Click an enemy player to blitz';
            case 'pass': return 'Select your ball carrier, then click a target square';
            case 'handoff': return 'Select your ball carrier, then click an adjacent teammate';
            case 'foul': return 'Click your player, then click an adjacent prone/stunned enemy';
            default: return `Click to ${mode}`;
        }
    }

    private formatPhase(phase: string): string {
        switch (phase) {
            case 'setup': return 'Setup';
            case 'play': return 'Playing';
            case 'kickoff': return 'Kickoff';
            case 'touchdown': return 'Touchdown!';
            case 'half_time': return 'Half Time';
            case 'game_over': return 'Game Over';
            default: return phase;
        }
    }
}
