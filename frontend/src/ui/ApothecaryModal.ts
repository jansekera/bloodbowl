import type { PendingApothecary } from '../api/types';

/** P206 (b), 16.10.2026: volba kouče o lékárníkovi (ř. 1200-1210) — komu ho použít, u Casualty pak který výsledek. */
const RESULT_LABELS: Record<string, string> = {
    ko: 'KO',
    badly_hurt: 'Badly Hurt',
    miss_next_game: 'Seriously Injured (MNG)',
    niggling: 'Niggling Injury',
    ma_loss: '−1 MA',
    av_loss: '−1 AV',
    ag_loss: '−1 AG',
    st_loss: '−1 ST',
    dead: 'Dead',
};

export class ApothecaryModal {
    private container: HTMLElement;

    constructor(parent: HTMLElement) {
        this.container = document.createElement('div');
        this.container.className = 'reroll-modal';
        this.container.style.display = 'none';
        parent.appendChild(this.container);
    }

    /** onChoice: stage 'offer' → { playerId } (0 = nepoužít); stage 'choose' → { result } */
    show(
        pending: PendingApothecary,
        nameOf: (playerId: number) => string,
        onChoice: (params: Record<string, unknown>) => void,
    ): void {
        let title: string;
        let buttons = '';
        if (pending.stage === 'choose') {
            title = `Apothecary: ${this.escape(nameOf(pending.chosenPlayerId ?? 0))} — choose the result`;
            for (const r of pending.options) {
                buttons += `<button class="reroll-modal__btn reroll-modal__btn--team" data-result="${r}">${RESULT_LABELS[r] ?? r}</button>`;
            }
        } else {
            title = 'Use the apothecary?';
            for (const o of pending.offers) {
                const what = o.kind === 'ko' ? RESULT_LABELS.ko : (RESULT_LABELS[o.result ?? ''] ?? o.result);
                buttons += `<button class="reroll-modal__btn reroll-modal__btn--team" data-player="${o.playerId}">${this.escape(nameOf(o.playerId))} (${what})</button>`;
            }
            buttons += '<button class="reroll-modal__btn reroll-modal__btn--decline" data-player="0">Do not use</button>';
        }
        this.container.innerHTML = `
            <div class="reroll-modal__backdrop"></div>
            <div class="reroll-modal__content">
                <div class="reroll-modal__title">${title}</div>
                <div class="reroll-modal__buttons">${buttons}</div>
            </div>
        `;
        this.container.style.display = '';
        this.container.querySelectorAll('.reroll-modal__btn').forEach(btn => {
            btn.addEventListener('click', () => {
                const el = btn as HTMLElement;
                this.hide();
                if (el.dataset.result !== undefined) onChoice({ result: el.dataset.result });
                else onChoice({ playerId: Number(el.dataset.player ?? 0) });
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
