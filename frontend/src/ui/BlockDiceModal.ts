import type { PendingBlock } from '../api/types';

const DICE_DISPLAY: Record<string, { label: string; css: string; title: string }> = {
    'attacker_down': { label: 'AD', css: 'skull', title: 'Attacker Down' },
    'both_down': { label: 'BD', css: 'bothdown', title: 'Both Down' },
    'pushed': { label: '>', css: 'push', title: 'Push' },
    'defender_stumbles': { label: 'DS', css: 'stumble', title: 'Defender Stumbles' },
    'defender_down': { label: 'DD', css: 'pow', title: 'Defender Down' },
};

export class BlockDiceModal {
    private container: HTMLElement;
    // P81 krok 3 (30.09.2026): follow-up je volba utocnika (BB2016 r. 608-611);
    // posila se spolu s kostkou. Frenzy nasleduje povinne -- to hlida engine.
    // Audit parity 08.10.2026, nalez 5 (BB2016 r. 633-634): kostku vybira trener SILNEJSIHO
    // hrace. Vybira-li souper vedeny AI, kostky jsou jen k nahlednuti a klient hod pouze
    // potvrdi (faceIndex = null) -- kostku zvoli server.
    private onChoose: ((faceIndex: number | null, followUp: boolean) => void) | null = null;
    private onReroll: ((type: string) => void) | null = null;

    constructor(container: HTMLElement) {
        this.container = document.createElement('div');
        this.container.className = 'block-dice-modal';
        this.container.style.display = 'none';
        container.appendChild(this.container);
    }

    show(
        pending: PendingBlock,
        attackerName: string,
        defenderName: string,
        onChoose: (faceIndex: number | null, followUp: boolean) => void,
        onReroll: (type: string) => void,
        opponentPicks = false,
    ): void {
        this.onChoose = onChoose;
        this.onReroll = onReroll;

        const chooserLabel = pending.attackerChooses ? attackerName : defenderName;

        let rerollButtons = '';
        if (pending.proAvailable) {
            rerollButtons += '<button class="block-dice-modal__reroll block-dice-modal__reroll--pro" data-type="pro" title="Pro: 4+ to reroll worst die">Pro</button>';
        }
        if (pending.teamRerollAvailable) {
            rerollButtons += '<button class="block-dice-modal__reroll block-dice-modal__reroll--team" data-type="team" title="Reroll all block dice">Team Reroll</button>';
        }

        const diceHtml = pending.faces.map((face, idx) => {
            const d = DICE_DISPLAY[face] ?? { label: '?', css: 'default', title: face };
            return opponentPicks
                ? `<button class="block-dice-modal__die block-dice-modal__die--${d.css}" disabled title="${d.title}">${d.label}</button>`
                : `<button class="block-dice-modal__die block-dice-modal__die--${d.css}" data-index="${idx}" title="${d.title}">${d.label}</button>`;
        }).join('');

        const frenzyLabel = pending.isFrenzy ? ' <span class="block-dice-modal__frenzy">(Frenzy)</span>' : '';

        this.container.innerHTML = `
            <div class="block-dice-modal__backdrop"></div>
            <div class="block-dice-modal__content">
                <div class="block-dice-modal__title">Block Dice${frenzyLabel}</div>
                <div class="block-dice-modal__info">${this.escape(chooserLabel)} chooses</div>
                <div class="block-dice-modal__dice">${diceHtml}</div>
                ${opponentPicks ? '<button class="block-dice-modal__accept">Continue (opponent picks the die)</button>' : ''}
                <label class="block-dice-modal__followup"><input type="checkbox" class="block-dice-modal__followup-input" checked> Follow up if the defender is pushed</label>
                ${rerollButtons ? `<div class="block-dice-modal__rerolls">${rerollButtons}</div>` : ''}
            </div>
        `;

        this.container.style.display = '';

        const choose = (idx: number | null): void => {
            const followUpInput = this.container.querySelector<HTMLInputElement>('.block-dice-modal__followup-input');
            const followUp = followUpInput?.checked ?? true;
            const onChoose = this.onChoose;
            this.hide();
            onChoose?.(idx, followUp);
        };

        // Bind die click handlers (kostky bez data-index = vybira souper, neklikaji se)
        this.container.querySelectorAll('.block-dice-modal__die').forEach(btn => {
            const index = (btn as HTMLElement).dataset.index;
            if (index !== undefined) {
                btn.addEventListener('click', () => choose(parseInt(index, 10)));
            }
        });
        this.container.querySelector('.block-dice-modal__accept')?.addEventListener('click', () => choose(null));

        // Bind reroll handlers
        this.container.querySelectorAll('.block-dice-modal__reroll').forEach(btn => {
            btn.addEventListener('click', () => {
                const type = (btn as HTMLElement).dataset.type ?? '';
                this.hide();
                this.onReroll?.(type);
            });
        });
    }

    hide(): void {
        this.container.style.display = 'none';
        this.container.innerHTML = '';
        this.onChoose = null;
        this.onReroll = null;
    }

    isVisible(): boolean {
        return this.container.style.display !== 'none';
    }

    private escape(text: string): string {
        const div = document.createElement('div');
        div.textContent = text;
        return div.innerHTML;
    }

    destroy(): void {
        this.container.remove();
    }
}
