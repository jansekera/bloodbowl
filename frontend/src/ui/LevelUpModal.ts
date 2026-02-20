import type { AvailableSkills, SkillInfo } from '../api/types';

export class LevelUpModal {
    private container: HTMLElement;
    private onSelect: ((skillId: number) => void) | null = null;
    private onSkip: (() => void) | null = null;

    constructor(container: HTMLElement) {
        this.container = container;
        this.container.classList.add('level-up-modal');
        this.container.style.display = 'none';
    }

    show(
        playerName: string,
        skills: AvailableSkills,
        onSelect: (skillId: number) => void,
        onSkip: () => void,
    ): void {
        this.onSelect = onSelect;
        this.onSkip = onSkip;
        this.container.style.display = '';
        this.render(playerName, skills);
    }

    hide(): void {
        this.container.style.display = 'none';
        this.container.innerHTML = '';
        this.onSelect = null;
        this.onSkip = null;
    }

    private render(playerName: string, skills: AvailableSkills): void {
        const normalHtml = skills.normal.length > 0
            ? '<h4>Normal Skills</h4>' +
              '<div class="level-up-modal__skills">' +
              skills.normal.map(s => this.renderSkill(s, 'normal')).join('') +
              '</div>'
            : '';

        const doubleHtml = skills.double.length > 0
            ? '<h4>Double Skills</h4>' +
              '<div class="level-up-modal__skills">' +
              skills.double.map(s => this.renderSkill(s, 'double')).join('') +
              '</div>'
            : '';

        this.container.innerHTML =
            '<div class="level-up-modal__backdrop"></div>' +
            '<div class="level-up-modal__content">' +
            `<h3>Level Up: ${this.escapeHtml(playerName)}</h3>` +
            normalHtml +
            doubleHtml +
            '<div class="level-up-modal__actions">' +
            '<button class="level-up-modal__skip">Skip</button>' +
            '</div>' +
            '</div>';

        // Bind events
        this.container.querySelectorAll('[data-skill-id]').forEach(btn => {
            btn.addEventListener('click', () => {
                const skillId = parseInt((btn as HTMLElement).dataset.skillId!, 10);
                this.onSelect?.(skillId);
                this.hide();
            });
        });

        const skipBtn = this.container.querySelector('.level-up-modal__skip');
        skipBtn?.addEventListener('click', () => {
            this.onSkip?.();
            this.hide();
        });

        const backdrop = this.container.querySelector('.level-up-modal__backdrop');
        backdrop?.addEventListener('click', () => {
            this.onSkip?.();
            this.hide();
        });
    }

    private renderSkill(skill: SkillInfo, type: 'normal' | 'double'): string {
        return `<button class="level-up-modal__skill level-up-modal__skill--${type}" data-skill-id="${skill.id}">${this.escapeHtml(skill.name)}</button>`;
    }

    private escapeHtml(text: string): string {
        return text.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;');
    }
}
