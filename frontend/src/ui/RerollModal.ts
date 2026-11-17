import type { PendingReroll, Position } from '../api/types';
import { escapeHtml } from './escapeHtml';

const ROLL_TYPE_LABELS: Record<string, string> = {
    dodge: 'Dodge',
    gfi: 'Going For It',
    pickup: 'Pickup',
    catch: 'Catch',
    leap: 'Leap',
    leap_gfi: 'Going For It (Leap)',
    bone_head: 'Bone Head',
    really_stupid: 'Really Stupid',
    wild_animal: 'Wild Animal',
    pass: 'Pass',
    bomb: 'Bomb Throw',
    ttm: 'Throw Team-Mate',
    ttm_landing: 'Landing (Right Stuff)',
    always_hungry: 'Always Hungry',
    jump_up: 'Jump Up (blok z lehu)',
};

/** Hody pohybu: neúspěch po odmítnutí = pád a turnover (ř. 497-499, 1702-1703, 8264-8277). */
const MOVE_ROLLS = new Set(['dodge', 'gfi', 'leap', 'leap_gfi']);

/** P294 krok 5b (uživatel 17.11.: „je dobře označit správně situaci, která chce reroll“): odkud kam a co je v sázce. */
/** P294 krok 5d: zvednutí a chytání — kde a co odmítnutí znamená (nepovedené zvednutí = turnover ř. 371-375, chytání ne ř. 376-378). */
const BALL_ROLLS: Record<string, string> = {
    pickup: 'Accept Failure = the ball bounces and the turn ends.',
    catch: 'Accept Failure = the ball bounces.',
};

function situation(pending: PendingReroll, from: Position | null): string {
    const mic = BALL_ROLLS[pending.rollType];
    if (mic !== undefined) {
        return `<div class="reroll-modal__info">at (${pending.targetX}, ${pending.targetY})</div><div class="reroll-modal__info">${mic}</div>`;
    }
    if (!MOVE_ROLLS.has(pending.rollType)) return '';
    const odkud = from ? `from (${from.x}, ${from.y}) ` : '';
    return `<div class="reroll-modal__info">${odkud}to (${pending.targetX}, ${pending.targetY})</div>`
        + '<div class="reroll-modal__info">Accept Failure = the player falls (armour roll) and the turn ends.</div>';
}

export class RerollModal {
    private container: HTMLElement;
    private onChoice: ((choice: string) => void) | null = null;

    constructor(parent: HTMLElement) {
        this.container = document.createElement('div');
        this.container.className = 'reroll-modal';
        this.container.style.display = 'none';
        parent.appendChild(this.container);
    }

    show(
        pending: PendingReroll,
        playerName: string,
        from: Position | null,
        onChoice: (choice: string) => void,
    ): void {
        this.onChoice = onChoice;

        const label = ROLL_TYPE_LABELS[pending.rollType] ?? pending.rollType;

        let buttons = '';
        if (pending.proAvailable) {
            buttons += '<button class="reroll-modal__btn reroll-modal__btn--pro" data-choice="pro">Pro (4+)</button>';
        }
        if (pending.teamRerollAvailable) {
            buttons += '<button class="reroll-modal__btn reroll-modal__btn--team" data-choice="team_reroll">Team Reroll</button>';
        }
        buttons += '<button class="reroll-modal__btn reroll-modal__btn--decline" data-choice="decline">Accept Failure</button>';

        this.container.innerHTML = `
            <div class="reroll-modal__backdrop"></div>
            <div class="reroll-modal__content">
                <div class="reroll-modal__title">${label} Failed!</div>
                <div class="reroll-modal__info">${escapeHtml(playerName)} rolled <strong>${pending.roll}</strong> (needed ${pending.target}+)</div>
                ${situation(pending, from)}
                ${pending.proFailed ? '<div class="reroll-modal__info">Pro roll failed — a team re-roll may re-roll the Pro roll.</div>' : ''}
                <div class="reroll-modal__buttons">${buttons}</div>
            </div>
        `;

        this.container.style.display = '';

        this.container.querySelectorAll('.reroll-modal__btn').forEach(btn => {
            btn.addEventListener('click', () => {
                const choice = (btn as HTMLElement).dataset.choice ?? 'decline';
                // OPRAVENO 17.11.2026 — `hide()` maže `onChoice`, a volalo se až po něm ⇒ klik na tlačítko nic
                //   neodeslal (od vzniku dialogu 9.3.2026; našel Playwright při P294 krok 5b)
                const onChoice = this.onChoice;
                this.hide();
                onChoice?.(choice);
            });
        });
    }

    hide(): void {
        this.container.style.display = 'none';
        this.container.innerHTML = '';
        this.onChoice = null;
    }

    isVisible(): boolean {
        return this.container.style.display !== 'none';
    }


    destroy(): void {
        this.container.remove();
    }
}
