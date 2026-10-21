import type { PendingGrab } from '../api/types';

/**
 * P224 (21.10.2026): kouč-člověk volí, jestli útočník použije Grab (`rules_bb2016.txt` ř. 8147-8154:
 * „may choose any empty square adjacent to his opponent“). Ano ⇒ vybere jedno z nabízených polí,
 * ne ⇒ běžné odtlačení. Follow-up se u bloku z akce Block volí až tady, po odtlačení
 * (ř. 608-611), ne spolu s kostkou.
 */
export class GrabModal {
    private container: HTMLElement;

    constructor(parent: HTMLElement) {
        this.container = document.createElement('div');
        this.container.className = 'reroll-modal';
        this.container.style.display = 'none';
        parent.appendChild(this.container);
    }

    /**
     * onChoice: { use: false } nebo { use: true, x, y }; u bloku z akce Block + followUp.
     * dieFace: zvolená kostka bloku (zobrazí se v nadpisu).
     */
    show(
        pending: PendingGrab,
        attackerName: string,
        defenderName: string,
        dieFace: string,
        onChoice: (params: Record<string, unknown>) => void,
    ): void {
        const followUpBox = pending.kind === 'block'
            ? '<label class="grab-modal__followup"><input type="checkbox" class="grab-modal__followup-input" checked> Follow up</label>'
            : '';
        let buttons = '';
        for (const s of pending.squares) {
            buttons += `<button class="reroll-modal__btn reroll-modal__btn--team" data-x="${s.x}" data-y="${s.y}">Grab: push to (${s.x}, ${s.y})</button>`;
        }
        buttons += '<button class="reroll-modal__btn reroll-modal__btn--decline" data-use="0">Standard push</button>';
        this.container.innerHTML = `
            <div class="reroll-modal__backdrop"></div>
            <div class="reroll-modal__content">
                <div class="reroll-modal__title">${this.escape(attackerName)} has Grab — block die: ${this.escape(dieFace)}. Choose where to push ${this.escape(defenderName)}</div>
                ${followUpBox}
                <div class="reroll-modal__buttons">${buttons}</div>
            </div>
        `;
        this.container.style.display = '';
        this.container.querySelectorAll('.reroll-modal__btn').forEach(btn => {
            btn.addEventListener('click', () => {
                const el = btn as HTMLElement;
                const box = this.container.querySelector('.grab-modal__followup-input') as HTMLInputElement | null;
                const extra = box ? { followUp: box.checked } : {};
                this.hide();
                if (el.dataset.x !== undefined) onChoice({ use: true, x: Number(el.dataset.x), y: Number(el.dataset.y), ...extra });
                else onChoice({ use: false, ...extra });
            });
        });
    }

    hide(): void {
        this.container.style.display = 'none';
        this.container.innerHTML = '';
    }

    private escape(text: string): string {
        const div = document.createElement('div');
        div.textContent = text;
        return div.innerHTML;
    }
}
