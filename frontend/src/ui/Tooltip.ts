import type { MatchPlayer } from '../api/types';

export const SKILL_DESCRIPTIONS: Record<string, string> = {
    'Block': 'Prevents knockdown on Both Down',
    'Dodge': 'Reroll failed dodge, forces Stumble on block',
    'Sure Hands': 'Reroll failed pickup',
    'Sure Feet': 'Reroll failed GFI',
    'Catch': 'Reroll failed catch',
    'Pass': '+1 to passing accuracy',
    'Frenzy': 'Must follow up, second block on pushback',
    'Tackle': 'Negates Dodge skill on blocks',
    'Mighty Blow': '+1 to armor/injury on blocks',
    'Guard': 'Provide assists even in tackle zones',
    'Stand Firm': 'Cannot be pushed back',
    'Horns': '+1 ST on blitz',
    'Claw': 'Armor breaks on 8+ regardless of AV',
    'Dauntless': 'Roll to match stronger opponent on block',
    'Strip Ball': 'Forces ball drop on pushback',
    'Pro': 'Reroll any one roll per turn on 4+',
    'Side Step': 'Choose pushback direction',
    'Nerves of Steel': 'Ignore tackle zones for pass/catch',
    'Loner': 'Team rerolls need 4+ to use',
    'Regeneration': 'Roll 4+ to avoid casualty',
    'Bone-head': 'On 1, lose action',
    'Really Stupid': 'Need ally or 4+ to act',
    'Wild Animal': 'Need 4+ for non-block actions',
    'Throw Team-Mate': 'Can throw Stunty teammates',
    'Right Stuff': 'Can be thrown by teammates',
    'Stunty': '+1 dodge, easier to injure',
    'Thick Skull': 'KO becomes Stunned on injury',
    'Prehensile Tail': '+1 to dodge for leaving players',
    'Break Tackle': 'Use ST instead of AG for dodge',
    'Sprint': 'Extra GFI attempt',
    'Jump Up': 'Stand up for free to block',
    'Leap': 'Jump over squares ignoring TZ',
    'Wrestle': 'Both Down becomes both prone (no armor)',
    'Tentacles': 'Opponents roll to leave your TZ',
    'Juggernaut': 'Both Down becomes push on blitz',
    'Grab': 'Choose push direction on block',
    'Disturbing Presence': '-1 to nearby enemy pass/catch',
    'Diving Tackle': '+1 TZ penalty for dodging',
    'Accurate': '+1 to passing accuracy',
    'Strong Arm': 'Reduce pass range by one category',
    'Safe Throw': 'Reroll interception attempts',
    'Two Heads': '+1 to dodge rolls',
    'Extra Arms': '+1 to catch and pickup',
    'No Hands': 'Cannot handle the ball',
    'Sneaky Git': 'Avoid ejection on foul doubles',
    'Fend': 'Opponent cannot follow up blocks',
    'Piling On': 'Reroll armor/injury (go prone)',
    'Kick': 'More accurate kickoff placement',
    'Kick-Off Return': 'Free move after kickoff',
    'Leader': '+1 team reroll',
    'Hail Mary Pass': 'Throw anywhere (always inaccurate)',
    'Dump-Off': 'Quick pass when blocked',
    'Diving Catch': '+1 catch in TZ, catch inaccurate passes',
    'Secret Weapon': 'Ejected at end of drive',
    'Take Root': 'On 1, cannot move',
    'Big Hand': 'Ignore TZ for pickup',
    'Dirty Player': '+1 to foul armor roll',
    'Stab': 'Armor roll instead of block dice (no pushback)',
    'Shadowing': 'Follow dodging opponent on D6+MA roll',
    'Bombardier': 'Throw a bomb instead of normal pass',
    'Bloodlust': 'Must roll 2+ before action or bite a Thrall',
    'Hypnotic Gaze': 'Target adjacent opponent loses tackle zones on 2+',
    'Ball & Chain': 'Move randomly, auto-block anyone contacted',
    'Decay': 'Roll injury twice, take worse result',
    'Chainsaw': 'Auto armor roll instead of block dice, kickback on double 1',
    'Foul Appearance': 'Opponent must roll 2+ before blocking',
    'Always Hungry': 'On 1, eat thrown teammate instead of throwing',
    'Very Long Legs': '+1 to Leap and interception rolls',
    'Animosity': 'Roll D6 when passing/handing off to different race; on 1, action fails',
    'Pass Block': 'Move up to 3 squares toward receiver when opponent declares pass',
    "Nurgle's Rot": 'Infects victim on casualty (flavor)',
    'Titchy': '+1 dodge, opponents easier to dodge away from',
    'Stakes': 'Blocks Regeneration when causing a casualty',
    'Multiple Block': 'Block 2 adjacent opponents (each at +2 ST, no follow-up)',
};

/**
 * Hover tooltip that shows player info when hovering over a player on the pitch.
 */
export class Tooltip {
    private element: HTMLDivElement;

    constructor(container: HTMLElement) {
        this.element = document.createElement('div');
        this.element.className = 'pitch-tooltip';
        this.element.style.display = 'none';
        container.appendChild(this.element);
    }

    show(player: MatchPlayer, px: number, py: number): void {
        const skillsHtml = player.skills.length > 0
            ? player.skills.map(s => {
                const desc = SKILL_DESCRIPTIONS[s];
                return desc
                    ? `<span class="pitch-tooltip__skill" title="${desc}">${s}</span>`
                    : `<span class="pitch-tooltip__skill">${s}</span>`;
            }).join(', ')
            : 'None';

        this.element.innerHTML = `
            <div class="pitch-tooltip__header">
                <strong>#${player.number} ${player.name}</strong>
                <span>${player.positionalName}</span>
            </div>
            <div class="pitch-tooltip__stats">
                MA ${player.stats.movement} |
                ST ${player.stats.strength} |
                AG ${player.stats.agility} |
                AV ${player.stats.armour}
            </div>
            <div class="pitch-tooltip__skills">${skillsHtml}</div>
            <div class="pitch-tooltip__status">${player.state}${player.hasMoved ? ' (moved)' : ''}</div>
        `;

        this.element.style.display = 'block';
        this.element.style.left = `${px + 16}px`;
        this.element.style.top = `${py - 10}px`;

        // Keep tooltip on screen
        const rect = this.element.getBoundingClientRect();
        if (rect.right > window.innerWidth) {
            this.element.style.left = `${px - rect.width - 10}px`;
        }
        if (rect.bottom > window.innerHeight) {
            this.element.style.top = `${py - rect.height - 10}px`;
        }
    }

    hide(): void {
        this.element.style.display = 'none';
    }

    destroy(): void {
        this.element.remove();
    }
}
