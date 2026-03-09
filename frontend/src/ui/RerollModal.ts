import type { PendingReroll } from '../api/types';

const ROLL_TYPE_LABELS: Record<string, string> = {
    dodge: 'Dodge',
    gfi: 'Going For It',
    pickup: 'Pickup',
};

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
                <div class="reroll-modal__info">${this.escape(playerName)} rolled <strong>${pending.roll}</strong> (needed ${pending.target}+)</div>
                <div class="reroll-modal__buttons">${buttons}</div>
            </div>
        `;

        this.container.style.display = '';

        this.container.querySelectorAll('.reroll-modal__btn').forEach(btn => {
            btn.addEventListener('click', () => {
                const choice = (btn as HTMLElement).dataset.choice ?? 'decline';
                this.hide();
                this.onChoice?.(choice);
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

    private escape(text: string): string {
        const div = document.createElement('div');
        div.textContent = text;
        return div.innerHTML;
    }

    destroy(): void {
        this.container.remove();
    }
}
