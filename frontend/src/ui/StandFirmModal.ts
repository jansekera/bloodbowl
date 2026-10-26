import type { PendingStandFirm } from '../api/types';

/**
 * P226 (21.10.2026): kouč-člověk volí, jestli jeho hráč použije Stand Firm (`rules_bb2016.txt`
 * ř. 8510-8516: „may choose to not be pushed back as the result of a block“). Ptá se i v tahu
 * soupeře (tah AI na odpověď čeká), u hráče v řetězovém odtlačení, u Ball & Chain i Multiple Block.
 * Review P226: dialog ukáže stranu kostky (Pushed vs Knock-down, ř. 8511-8513) a pole, kam by
 * hráč bez Stand Firm šel; review P226 (LOW 1, 4): že pole může změnit Grab útočníka a že závisí na
 * Stand Firm hráčů v řetězu, kteří ještě neodpověděli.
 */
export class StandFirmModal {
    private container: HTMLElement;

    constructor(parent: HTMLElement) {
        this.container = document.createElement('div');
        this.container.className = 'reroll-modal';
        this.container.style.display = 'none';
        parent.appendChild(this.container);
    }

    /** onChoice: { use: true } = zůstane stát, { use: false } = nechá se odtlačit. */
    show(
        pending: PendingStandFirm,
        playerName: string,
        attackerName: string,
        playerPos: { x: number; y: number } | null,
        dependsOnNames: string[],
        onChoice: (params: Record<string, unknown>) => void,
    ): void {
        const chain = pending.playerId !== pending.defenderId ? ' (chain push)' : '';
        const target = pending.pushTo === null
            ? 'into the crowd'
            : (playerPos && playerPos.x === pending.pushTo.x && playerPos.y === pending.pushTo.y)
                ? 'nowhere (the push is blocked)'
                : `to (${pending.pushTo.x}, ${pending.pushTo.y})`;
        const die = pending.face === null ? 'Ball & Chain push of a prone player' : `die: ${pending.face}`;
        const notes = [
            pending.grabPossible ? 'The attacker may still choose another square with Grab.' : '',
            dependsOnNames.length > 0 ? `The square depends on Stand Firm of ${dependsOnNames.join(', ')} (not asked yet).` : '',
        ].filter(n => n !== '').map(n => ' ' + this.escape(n)).join('');
        this.container.innerHTML = `
            <div class="reroll-modal__backdrop"></div>
            <div class="reroll-modal__content">
                <div class="reroll-modal__title">${this.escape(playerName)} has Stand Firm — ${this.escape(attackerName)}'s block, ${this.escape(die)}${chain}. Use Stand Firm?</div>
                <div class="reroll-modal__text">Without Stand Firm he is pushed ${this.escape(target)}.${notes}</div>
                <div class="reroll-modal__buttons">
                    <button class="reroll-modal__btn reroll-modal__btn--team" data-use="1">Stand Firm (stay)</button>
                    <button class="reroll-modal__btn reroll-modal__btn--decline" data-use="0">Be pushed back</button>
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

    private escape(text: string): string {
        const div = document.createElement('div');
        div.textContent = text;
        return div.innerHTML;
    }
}
