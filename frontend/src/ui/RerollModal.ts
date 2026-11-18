import type { PendingReroll, Position } from '../api/types';
import { escapeHtml } from './escapeHtml';

/**
 * Co je v sázce (P294 krok 5b, uživatel 17.11.: „je dobře označit správně situaci, která chce reroll“). P306 A7:
 * popisek, kategorii a následek odmítnutí posílá server (`App\Enum\RollType`) — tady jen pole na hřišti.
 */
function situation(pending: PendingReroll, from: Position | null): string {
    const nasledek = pending.consequence !== null ? `<div class="reroll-modal__info">${escapeHtml(pending.consequence)}</div>` : '';
    if (pending.category === 'big_guy' || pending.category === 'throw') return nasledek;   // bez pole na hřišti
    // pohyb: odkud kam; míč a GFI, kterým se v blitzu platí rána (hráč se nehýbe), jen pole (P294 /simplify dávka 3)
    const naMiste = pending.category === 'ball' || (from !== null && from.x === pending.targetX && from.y === pending.targetY);
    const pole = naMiste ? `at (${pending.targetX}, ${pending.targetY})`
        : `${from ? `from (${from.x}, ${from.y}) ` : ''}to (${pending.targetX}, ${pending.targetY})`;
    return `<div class="reroll-modal__info">${pole}</div>${nasledek}`;
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
                <div class="reroll-modal__title">${escapeHtml(pending.label)} Failed!</div>
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
