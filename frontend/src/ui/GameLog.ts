import type { GameEvent } from '../api/types';

export class GameLog {
    private container: HTMLElement;
    private events: GameEvent[] = [];

    constructor(container: HTMLElement) {
        this.container = container;
        this.container.classList.add('game-log');
        this.render();
    }

    addEvent(event: GameEvent): void {
        this.events.unshift(event);
        if (this.events.length > 50) {
            this.events = this.events.slice(0, 50);
        }
        this.render();
    }

    addEvents(events: GameEvent[]): void {
        for (const event of events) {
            this.events.unshift(event);
        }
        if (this.events.length > 50) {
            this.events = this.events.slice(0, 50);
        }
        this.render();
    }

    clear(): void {
        this.events = [];
        this.render();
    }

    private render(): void {
        if (this.events.length === 0) {
            this.container.innerHTML = '<div class="game-log__empty">No events yet</div>';
            return;
        }

        this.container.innerHTML = '<h3>Game Log</h3>' +
            this.events.map(e => this.renderEvent(e)).join('');
    }

    private renderEvent(event: GameEvent): string {
        const cssClass = this.getEventClass(event.type);
        return `<div class="game-log__event game-log__event--${cssClass}">${this.escapeHtml(event.description)}</div>`;
    }

    private getEventClass(type: string): string {
        switch (type) {
            case 'turnover': return 'turnover';
            case 'dodge': return 'roll';
            case 'gfi': return 'roll';
            case 'block': return 'block';
            case 'armour_roll': return 'roll';
            case 'injury_roll': return 'injury';
            case 'player_fell': return 'injury';
            case 'push': return 'block';
            case 'crowd_surf': return 'injury';
            case 'follow_up': return 'default';
            case 'end_turn': return 'phase';
            case 'ball_pickup': return 'ball';
            case 'ball_bounce': return 'ball';
            case 'pass': return 'ball';
            case 'catch': return 'ball';
            case 'hand_off': return 'ball';
            case 'interception': return 'ball';
            case 'throw_in': return 'ball';
            case 'touchdown': return 'touchdown';
            case 'kickoff': return 'phase';
            case 'touchback': return 'phase';
            case 'half_time': return 'phase';
            case 'game_over': return 'phase';
            case 'ko_recovery': return 'roll';
            case 'bone_head': return 'roll';
            case 'really_stupid': return 'roll';
            case 'wild_animal': return 'roll';
            case 'loner': return 'roll';
            case 'regeneration': return 'roll';
            case 'pro': return 'roll';
            case 'foul': return 'block';
            case 'ejection': return 'injury';
            case 'reroll': return 'roll';
            case 'apothecary': return 'roll';
            case 'weather_change': return 'phase';
            case 'sweltering_heat': return 'injury';
            case 'kickoff_table': return 'phase';
            case 'stand_up': return 'default';
            case 'strip_ball': return 'ball';
            case 'frenzy': return 'block';
            case 'pickup': return 'ball';
            case 'player_move': return 'default';
            case 'wrestle': return 'block';
            case 'tentacles': return 'roll';
            case 'juggernaut': return 'block';
            case 'diving_tackle': return 'roll';
            case 'leap': return 'roll';
            case 'throw_team_mate': return 'ball';
            case 'ttm_landing': return 'roll';
            case 'multiple_block': return 'block';
            default: return 'default';
        }
    }

    private escapeHtml(text: string): string {
        const div = document.createElement('div');
        div.textContent = text;
        return div.innerHTML;
    }
}
