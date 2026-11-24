import { escapeHtml } from './escapeHtml';

/**
 * P312 (uživatel 20.11.2026: „ano“): kouč-člověk volí, jestli jeho hráč použije Wrestle (`rules_bb2016.txt`
 * ř. 8670-8676: „may use Wrestle when he blocks or is blocked and a 'Both Down' result … is chosen“). Ano ⇒ oba
 * hráči na zem bez hodu na brnění (turnover jen u nosiče na tahu); ne ⇒ platí Both Down.
 */
export class WrestleModal {
    private container: HTMLElement;

    constructor(parent: HTMLElement) {
        this.container = document.createElement('div');
        this.container.className = 'reroll-modal';
        this.container.style.display = 'none';
        parent.appendChild(this.container);
    }

    /** onChoice: { use: true } = Wrestle, { use: false } = Both Down platí. */
    show(playerName: string, opponentName: string, onChoice: (params: Record<string, unknown>) => void): void {
        this.container.innerHTML = `
            <div class="reroll-modal__backdrop"></div>
            <div class="reroll-modal__content">
                <div class="reroll-modal__title">${escapeHtml(playerName)} has Wrestle — Both Down against ${escapeHtml(opponentName)}. Use Wrestle?</div>
                <div class="reroll-modal__text">Wrestle: both players are placed prone, no armour rolls (turnover only if the active player holds the ball). No: Both Down applies — each player without Block is knocked down.</div>
                <div class="reroll-modal__buttons">
                    <button class="reroll-modal__btn reroll-modal__btn--team" data-use="1">Wrestle</button>
                    <button class="reroll-modal__btn reroll-modal__btn--decline" data-use="0">Both Down</button>
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
