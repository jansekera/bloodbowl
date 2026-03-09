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
    private onChoose: ((faceIndex: number) => void) | null = null;
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
        onChoose: (faceIndex: number) => void,
        onReroll: (type: string) => void,
    ): void {
        this.onChoose = onChoose;
        this.onReroll = onReroll;

        const chooserLabel = pending.attackerChooses ? attackerName : defenderName;
        const hasBothDown = pending.faces.some(f => f === 'both_down');

        let rerollButtons = '';
        if (pending.brawlerAvailable && hasBothDown) {
            rerollButtons += '<button class="block-dice-modal__reroll block-dice-modal__reroll--brawler" data-type="brawler" title="Reroll the Both Down die">Brawler</button>';
        }
        if (pending.proAvailable) {
            rerollButtons += '<button class="block-dice-modal__reroll block-dice-modal__reroll--pro" data-type="pro" title="Pro: 4+ to reroll worst die">Pro</button>';
        }
        if (pending.teamRerollAvailable) {
            rerollButtons += '<button class="block-dice-modal__reroll block-dice-modal__reroll--team" data-type="team" title="Reroll all block dice">Team Reroll</button>';
        }

        const diceHtml = pending.faces.map((face, idx) => {
            const d = DICE_DISPLAY[face] ?? { label: '?', css: 'default', title: face };
            return `<button class="block-dice-modal__die block-dice-modal__die--${d.css}" data-index="${idx}" title="${d.title}">${d.label}</button>`;
        }).join('');

        const frenzyLabel = pending.isFrenzy ? ' <span class="block-dice-modal__frenzy">(Frenzy)</span>' : '';

        this.container.innerHTML = `
            <div class="block-dice-modal__backdrop"></div>
            <div class="block-dice-modal__content">
                <div class="block-dice-modal__title">Block Dice${frenzyLabel}</div>
                <div class="block-dice-modal__info">${this.escape(chooserLabel)} chooses</div>
                <div class="block-dice-modal__dice">${diceHtml}</div>
                ${rerollButtons ? `<div class="block-dice-modal__rerolls">${rerollButtons}</div>` : ''}
            </div>
        `;

        this.container.style.display = '';

        // Bind die click handlers
        this.container.querySelectorAll('.block-dice-modal__die').forEach(btn => {
            btn.addEventListener('click', () => {
                const idx = parseInt((btn as HTMLElement).dataset.index ?? '0', 10);
                this.hide();
                this.onChoose?.(idx);
            });
        });

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
