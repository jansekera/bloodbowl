/**
 * P296: lišta naplánované trasy pohybu — popis rizika a volby „Go“ (celá trasa), „Step“ (jedno pole, pak se
 * kouč rozhodne znovu, třeba podle výsledku úhybu) a „Cancel“. Nemodální: kouč smí dál klikat do hřiště.
 */
export type MovePlanChoice = 'go' | 'step' | 'cancel';

export class MovePlanBar {
    private container: HTMLElement;

    constructor(parent: HTMLElement) {
        this.container = document.createElement('div');
        this.container.className = 'move-plan';
        this.container.style.display = 'none';
        parent.appendChild(this.container);
    }

    show(playerName: string, description: string, onChoice: (choice: MovePlanChoice) => void): void {
        this.container.innerHTML = `
            <span class="move-plan__text">${this.escape(playerName)}: ${this.escape(description)}</span>
            <button class="move-plan__btn move-plan__btn--go" data-choice="go" title="Run the whole route (or click the target again)">Go</button>
            <button class="move-plan__btn" data-choice="step" title="Move one square, then decide again">Step</button>
            <button class="move-plan__btn move-plan__btn--cancel" data-choice="cancel">Cancel</button>
        `;
        this.container.style.display = '';
        this.container.querySelectorAll('.move-plan__btn').forEach(btn => {
            btn.addEventListener('click', () => onChoice((btn as HTMLElement).dataset.choice as MovePlanChoice));
        });
    }

    hide(): void {
        this.container.style.display = 'none';
        this.container.innerHTML = '';
    }

    private escape(s: string): string {
        return s.replace(/[&<>"']/g, c => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' }[c] as string));
    }
}
