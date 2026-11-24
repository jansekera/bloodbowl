import type { PendingSideStep } from '../api/types';
import { escapeHtml } from './escapeHtml';

/**
 * P312 (uživatel 20.11.2026: „ano“): kouč-člověk odtlačeného hráče se Side Step volí pole (`rules_bb2016.txt`
 * ř. 8474-8478: „his coach may choose to move the player to any adjacent square, not just the three squares shown
 * on the Push Back diagram“), nebo skill nepoužije (běžné odtlačení). Doporučené pole = volba AI.
 */
export class SideStepModal {
    private container: HTMLElement;

    constructor(parent: HTMLElement) {
        this.container = document.createElement('div');
        this.container.className = 'reroll-modal';
        this.container.style.display = 'none';
        parent.appendChild(this.container);
    }

    /** onChoice: { use: true, x, y } nebo { use: false }. */
    show(pending: PendingSideStep, playerName: string, onChoice: (params: Record<string, unknown>) => void): void {
        let buttons = '';
        for (const s of pending.squares) {
            const doporuceno = s.x === pending.suggested.x && s.y === pending.suggested.y ? ' (suggested)' : '';
            buttons += `<button class="reroll-modal__btn reroll-modal__btn--team" data-x="${s.x}" data-y="${s.y}">Side Step to (${s.x}, ${s.y})${doporuceno}</button>`;
        }
        buttons += '<button class="reroll-modal__btn reroll-modal__btn--decline" data-use="0">Standard push</button>';
        this.container.innerHTML = `
            <div class="reroll-modal__backdrop"></div>
            <div class="reroll-modal__content">
                <div class="reroll-modal__title">${escapeHtml(playerName)} is pushed back and has Side Step — choose the square</div>
                <div class="reroll-modal__buttons">${buttons}</div>
            </div>
        `;
        this.container.style.display = '';
        this.container.querySelectorAll('.reroll-modal__btn').forEach(btn => {
            btn.addEventListener('click', () => {
                const el = btn as HTMLElement;
                this.hide();
                if (el.dataset.x !== undefined) onChoice({ use: true, x: Number(el.dataset.x), y: Number(el.dataset.y) });
                else onChoice({ use: false });
            });
        });
    }

    hide(): void {
        this.container.style.display = 'none';
        this.container.innerHTML = '';
    }
}
