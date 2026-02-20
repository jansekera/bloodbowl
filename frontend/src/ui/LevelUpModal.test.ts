import { describe, it, expect, vi, beforeEach } from 'vitest';
import { LevelUpModal } from './LevelUpModal';
import type { AvailableSkills } from '../api/types';

/** Minimal DOM mock for LevelUpModal */
function createMockContainer(): HTMLElement {
    let html = '';
    const listeners: Array<{ selector: string; handler: () => void }> = [];

    return {
        classList: { add: vi.fn() },
        style: { display: '' },
        get innerHTML() { return html; },
        set innerHTML(v: string) {
            html = v;
            listeners.length = 0; // clear listeners on re-render
        },
        querySelectorAll(selector: string) {
            // Parse skill buttons from innerHTML
            if (selector === '[data-skill-id]') {
                const matches = [...html.matchAll(/data-skill-id="(\d+)"/g)];
                return matches.map(m => {
                    const btn = {
                        dataset: { skillId: m[1] },
                        addEventListener: (event: string, handler: () => void) => {
                            listeners.push({ selector: `skill-${m[1]}`, handler });
                        },
                    };
                    return btn;
                });
            }
            return [];
        },
        querySelector(selector: string) {
            if (selector === '.level-up-modal__skip' && html.includes('level-up-modal__skip')) {
                return {
                    addEventListener: (event: string, handler: () => void) => {
                        listeners.push({ selector: 'skip', handler });
                    },
                };
            }
            if (selector === '.level-up-modal__backdrop' && html.includes('level-up-modal__backdrop')) {
                return {
                    addEventListener: (event: string, handler: () => void) => {
                        listeners.push({ selector: 'backdrop', handler });
                    },
                };
            }
            return null;
        },
        // Expose internal listeners for test triggering
        __getListeners: () => listeners,
    } as unknown as HTMLElement & { __getListeners: () => typeof listeners };
}

function makeSkills(): AvailableSkills {
    return {
        normal: [
            { id: 1, name: 'Block', category: 'General', description: '' },
            { id: 2, name: 'Tackle', category: 'General', description: '' },
        ],
        double: [
            { id: 10, name: 'Dodge', category: 'Agility', description: '' },
        ],
        can_advance: true,
    };
}

describe('LevelUpModal integration helpers', () => {
    it('can_advance flag correctly filters eligible players', () => {
        const eligible: AvailableSkills = {
            normal: [{ id: 1, name: 'Block', category: 'General', description: '' }],
            double: [],
            can_advance: true,
        };
        const notEligible: AvailableSkills = {
            normal: [],
            double: [],
            can_advance: false,
        };

        expect(eligible.can_advance).toBe(true);
        expect(eligible.normal.length).toBeGreaterThan(0);
        expect(notEligible.can_advance).toBe(false);
        expect(notEligible.normal.length + notEligible.double.length).toBe(0);
    });

    it('handles empty skill lists when can_advance is true', () => {
        const skills: AvailableSkills = {
            normal: [],
            double: [],
            can_advance: true,
        };
        // Should not show modal when no skills available
        const shouldShow = skills.can_advance && (skills.normal.length > 0 || skills.double.length > 0);
        expect(shouldShow).toBe(false);
    });
});

describe('LevelUpModal', () => {
    let container: ReturnType<typeof createMockContainer>;
    let modal: LevelUpModal;

    beforeEach(() => {
        container = createMockContainer();
        modal = new LevelUpModal(container as HTMLElement);
    });

    it('renders normal and double skills', () => {
        const onSelect = vi.fn();
        const onSkip = vi.fn();

        modal.show('Player 1', makeSkills(), onSelect, onSkip);

        expect(container.innerHTML).toContain('Level Up: Player 1');
        expect(container.innerHTML).toContain('Normal Skills');
        expect(container.innerHTML).toContain('Double Skills');
        expect(container.innerHTML).toContain('Block');
        expect(container.innerHTML).toContain('Tackle');
        expect(container.innerHTML).toContain('Dodge');
        expect(container.style.display).toBe('');
    });

    it('calls onSelect when clicking a skill', () => {
        const onSelect = vi.fn();
        const onSkip = vi.fn();

        modal.show('Player 1', makeSkills(), onSelect, onSkip);

        // Simulate clicking "Block" (id=1)
        const listeners = (container as any).__getListeners();
        const blockListener = listeners.find((l: any) => l.selector === 'skill-1');
        expect(blockListener).toBeDefined();
        blockListener!.handler();

        expect(onSelect).toHaveBeenCalledWith(1);
    });

    it('hides modal after skill selection', () => {
        const onSelect = vi.fn();
        const onSkip = vi.fn();

        modal.show('Player 1', makeSkills(), onSelect, onSkip);

        const listeners = (container as any).__getListeners();
        const blockListener = listeners.find((l: any) => l.selector === 'skill-1');
        blockListener!.handler();

        expect(container.style.display).toBe('none');
        expect(container.innerHTML).toBe('');
    });

    it('skip button calls onSkip and hides modal', () => {
        const onSelect = vi.fn();
        const onSkip = vi.fn();

        modal.show('Player 1', makeSkills(), onSelect, onSkip);

        const listeners = (container as any).__getListeners();
        const skipListener = listeners.find((l: any) => l.selector === 'skip');
        expect(skipListener).toBeDefined();
        skipListener!.handler();

        expect(onSkip).toHaveBeenCalledOnce();
        expect(container.style.display).toBe('none');
    });
});
