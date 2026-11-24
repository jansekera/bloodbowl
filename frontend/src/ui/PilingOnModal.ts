import { escapeHtml } from './escapeHtml';

/**
 * P310 (uživatel 24.11.2026: „3 ano“): kouč-člověk volí, jestli jeho hráč použije Piling On (`rules_bb2016.txt`
 * ř. 8361-8371: „You can use a team re-roll to re-roll the Armour roll or Injury roll. Then the Piling On player is
 * Placed Prone in their own square“). `armour` = brnění vydrželo (přehodí se brnění), jinak padlo Stunned (zranění).
 */
export class PilingOnModal {
    private container: HTMLElement;

    constructor(parent: HTMLElement) {
        this.container = document.createElement('div');
        this.container.className = 'reroll-modal';
        this.container.style.display = 'none';
        parent.appendChild(this.container);
    }

    /** onChoice: { use: true } = použít (týmový přehoz, hráč si lehne), { use: false } = ne. */
    show(playerName: string, defenderName: string, armour: boolean, onChoice: (params: Record<string, unknown>) => void): void {
        const roll = armour ? 'Armour roll (armour held)' : 'Injury roll (only Stunned)';
        this.container.innerHTML = `
            <div class="reroll-modal__backdrop"></div>
            <div class="reroll-modal__content">
                <div class="reroll-modal__title">${escapeHtml(playerName)} has Piling On — re-roll the ${roll} against ${escapeHtml(defenderName)}?</div>
                <div class="reroll-modal__text">Costs a team re-roll (Loner: 1-3 the re-roll is lost and the player stays standing). ${escapeHtml(playerName)} is then placed prone — no armour roll, no turnover unless carrying the ball.</div>
                <div class="reroll-modal__buttons">
                    <button class="reroll-modal__btn reroll-modal__btn--team" data-use="1">Pile On</button>
                    <button class="reroll-modal__btn reroll-modal__btn--decline" data-use="0">No</button>
                </div>
            </div>
        `;
        this.container.style.display = '';
        this.container.querySelectorAll('.reroll-modal__btn').forEach(btn => {
            btn.addEventListener('click', () => {
                const use = (btn as HTMLElement).dataset.use === '1';
                this.hide();
                onChoice({ use });
            });
        });
    }

    hide(): void {
        this.container.style.display = 'none';
        this.container.innerHTML = '';
    }
}
