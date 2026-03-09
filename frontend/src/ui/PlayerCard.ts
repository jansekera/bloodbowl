import type { MatchPlayer } from '../api/types';
import { SKILL_DESCRIPTIONS } from './Tooltip';

/**
 * Persistent player detail card shown when a player is selected.
 */
export class PlayerCard {
    private element: HTMLElement;

    constructor(container: HTMLElement) {
        this.element = document.createElement('div');
        this.element.className = 'player-card';
        this.element.style.display = 'none';
        container.appendChild(this.element);
    }

    show(player: MatchPlayer): void {
        const stColor = player.stats.strength >= 4 ? 'player-card__stat--high' :
            player.stats.strength <= 2 ? 'player-card__stat--low' : '';
        const agColor = player.stats.agility >= 4 ? 'player-card__stat--high' :
            player.stats.agility <= 2 ? 'player-card__stat--low' : '';
        const avColor = player.stats.armour >= 9 ? 'player-card__stat--high' :
            player.stats.armour <= 7 ? 'player-card__stat--low' : '';

        const stateClass = this.stateClass(player.state);

        const skillsHtml = player.skills.length > 0
            ? player.skills.map(s => {
                const desc = SKILL_DESCRIPTIONS[s];
                const cat = this.skillCategory(s);
                return `<span class="player-card__skill player-card__skill--${cat}" title="${desc ?? s}">${s}</span>`;
            }).join('')
            : '<span class="player-card__no-skills">No skills</span>';

        const actions: string[] = [];
        if (player.hasMoved) actions.push('Moved');
        if (player.hasActed) actions.push('Acted');

        this.element.innerHTML = `
            <div class="player-card__header">
                <span class="player-card__number">#${player.number}</span>
                <span class="player-card__name">${this.escape(player.name)}</span>
                <span class="player-card__close" title="Close">&times;</span>
            </div>
            <div class="player-card__position">${this.escape(player.positionalName)}</div>
            <div class="player-card__stats">
                <div class="player-card__stat">
                    <span class="player-card__stat-label">MA</span>
                    <span class="player-card__stat-value">${player.stats.movement}</span>
                </div>
                <div class="player-card__stat ${stColor}">
                    <span class="player-card__stat-label">ST</span>
                    <span class="player-card__stat-value">${player.stats.strength}</span>
                </div>
                <div class="player-card__stat ${agColor}">
                    <span class="player-card__stat-label">AG</span>
                    <span class="player-card__stat-value">${player.stats.agility}</span>
                </div>
                <div class="player-card__stat ${avColor}">
                    <span class="player-card__stat-label">AV</span>
                    <span class="player-card__stat-value">${player.stats.armour}</span>
                </div>
            </div>
            <div class="player-card__state ${stateClass}">${player.state}${actions.length ? ' (' + actions.join(', ') + ')' : ''}</div>
            <div class="player-card__skills-label">Skills</div>
            <div class="player-card__skills">${skillsHtml}</div>
        `;

        this.element.style.display = 'block';

        // Close button
        this.element.querySelector('.player-card__close')?.addEventListener('click', () => this.hide());
    }

    hide(): void {
        this.element.style.display = 'none';
    }

    private stateClass(state: string): string {
        switch (state) {
            case 'standing': return 'player-card__state--standing';
            case 'prone': return 'player-card__state--prone';
            case 'stunned': return 'player-card__state--stunned';
            case 'ko': return 'player-card__state--ko';
            case 'injured':
            case 'dead':
            case 'ejected': return 'player-card__state--out';
            default: return '';
        }
    }

    private skillCategory(skill: string): string {
        // General skills
        const general = ['Block', 'Dodge', 'Fend', 'Wrestle', 'Kick', 'Kick-Off Return', 'Pass Block', 'Dirty Player', 'Sneaky Git', 'Sure Feet', 'Sure Hands', 'Catch', 'Strip Ball', 'Tackle', 'Brawler'];
        const strength = ['Guard', 'Mighty Blow', 'Stand Firm', 'Grab', 'Piling On', 'Break Tackle', 'Thick Skull', 'Multiple Block', 'Strong Arm'];
        const agility = ['Dodge', 'Sprint', 'Jump Up', 'Leap', 'Side Step', 'Diving Catch', 'Diving Tackle', 'Two Heads', 'Extra Arms', 'Nerves of Steel', 'Accurate', 'Safe Throw', 'Hail Mary Pass', 'Dump-Off'];
        const mutation = ['Horns', 'Claw', 'Prehensile Tail', 'Disturbing Presence', 'Foul Appearance', 'Tentacles', 'Big Hand', 'Very Long Legs', 'Titchy', "Nurgle's Rot"];

        if (mutation.includes(skill)) return 'mutation';
        if (strength.includes(skill)) return 'strength';
        if (agility.includes(skill)) return 'agility';
        if (general.includes(skill)) return 'general';
        return 'extraordinary';
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
