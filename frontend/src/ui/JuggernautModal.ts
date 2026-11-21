import { escapeHtml } from './escapeHtml';

/**
 * P309 (uživatel 20.11.2026): kouč-člověk volí, jestli jeho blitzující hráč s Juggernautem vezme Both Down jako
 * Pushed (`rules_bb2016.txt` ř. 8191-8195: „he may choose to treat a 'Both Down' result as if a 'Pushed' result has
 * been rolled instead“). Ne ⇒ Both Down platí (Block drží na nohou; Wrestle obránce Juggernaut v blitzu ruší).
 */
export class JuggernautModal {
    private container: HTMLElement;

    constructor(parent: HTMLElement) {
        this.container = document.createElement('div');
        this.container.className = 'reroll-modal';
        this.container.style.display = 'none';
        parent.appendChild(this.container);
    }

    /** onChoice: { use: true } = brát jako Pushed, { use: false } = Both Down platí. */
    show(playerName: string, defenderName: string, onChoice: (params: Record<string, unknown>) => void): void {
        this.container.innerHTML = `
            <div class="reroll-modal__backdrop"></div>
            <div class="reroll-modal__content">
                <div class="reroll-modal__title">${escapeHtml(playerName)} has Juggernaut — Both Down against ${escapeHtml(defenderName)}. Treat it as Pushed?</div>
                <div class="reroll-modal__text">Pushed: ${escapeHtml(defenderName)} is pushed back, nobody falls. Both Down: each player without Block is knocked down (${escapeHtml(defenderName)} may not use Wrestle against a Juggernaut on a Blitz).</div>
                <div class="reroll-modal__buttons">
                    <button class="reroll-modal__btn reroll-modal__btn--team" data-use="1">Pushed</button>
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
