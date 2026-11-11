import type { PendingReferee } from '../api/types';
import { escapeHtml } from './escapeHtml';

/**
 * P291 (uživatel 10.11.2026: „udělej ať se zeptá — člověk může některého hráče preferovat, ať zůstane“).
 * Rozhodčí vylučuje hráče kouče: použít úplatek (`rules_bb2016.txt` ř. 2871-2879, 2+ = výrok neplatí) a / nebo
 * protestovat (Argue the call, ř. 1091-1102: 6 = jen do rezerv, 1 = trenér vyhozen)? Po faulu se napřed uplácí
 * (úspěch ruší i turnover), u tajné zbraně na konci drive napřed protestuje. Nabízí jen to, co tým má.
 */
export class RefereeModal {
    private container: HTMLElement;

    constructor(parent: HTMLElement) {
        this.container = document.createElement('div');
        this.container.className = 'reroll-modal';
        this.container.style.display = 'none';
        parent.appendChild(this.container);
    }

    show(pending: PendingReferee, playerName: string, onChoice: (params: { bribe: boolean; argue: boolean }) => void): void {
        const name = escapeHtml(playerName);
        const why = pending.foul ? `The referee spotted the foul and sends ${name} off (turnover).` : `${name} has a Secret Weapon and is sent off at the end of the drive.`;
        const bribeText = pending.foul ? 'Bribe (2+): he stays and there is no turnover.' : 'Bribe (2+): he stays.';
        const argueText = 'Argue the call (6): he goes to the reserves instead' + (pending.foul ? ' (turnover stays)' : '') + '; on a 1 your head coach is sent off.';

        const options: Array<{ label: string; bribe: boolean; argue: boolean }> = [];
        if (pending.canBribe && pending.canArgue) {
            options.push(pending.foul
                ? { label: 'Bribe, then argue if it fails', bribe: true, argue: true }
                : { label: 'Argue, then bribe if it fails', bribe: true, argue: true });
        }
        if (pending.canBribe) options.push({ label: 'Bribe', bribe: true, argue: false });
        if (pending.canArgue) options.push({ label: 'Argue the call', bribe: false, argue: true });
        options.push({ label: 'Accept', bribe: false, argue: false });

        const buttons = options.map((o, i) => {
            const cls = i === options.length - 1 ? 'reroll-modal__btn--decline' : 'reroll-modal__btn--team';
            return `<button class="reroll-modal__btn ${cls}" data-bribe="${o.bribe ? 1 : 0}" data-argue="${o.argue ? 1 : 0}">${o.label}</button>`;
        }).join('');

        this.container.innerHTML = `
            <div class="reroll-modal__backdrop"></div>
            <div class="reroll-modal__content">
                <div class="reroll-modal__title">Referee: ${name} is sent off</div>
                <div class="reroll-modal__text">${why}</div>
                ${pending.canBribe ? `<div class="reroll-modal__text">${bribeText}</div>` : ''}
                ${pending.canArgue ? `<div class="reroll-modal__text">${argueText}</div>` : ''}
                <div class="reroll-modal__buttons">${buttons}</div>
            </div>
        `;
        this.container.style.display = '';
        this.container.querySelectorAll('.reroll-modal__btn').forEach(btn => {
            btn.addEventListener('click', () => {
                const d = (btn as HTMLElement).dataset;
                this.hide();
                onChoice({ bribe: d.bribe === '1', argue: d.argue === '1' });
            });
        });
    }

    hide(): void {
        this.container.style.display = 'none';
        this.container.innerHTML = '';
    }
}
