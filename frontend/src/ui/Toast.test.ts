import { describe, it, expect } from 'vitest';
import { TOAST_LEVELS } from './Toast';
import { SKILL_DESCRIPTIONS } from './Tooltip';

describe('Toast', () => {
    it('has correct level class mappings', () => {
        expect(TOAST_LEVELS.info).toBe('toast--info');
        expect(TOAST_LEVELS.error).toBe('toast--error');
        expect(TOAST_LEVELS.success).toBe('toast--success');
    });
});

describe('SKILL_DESCRIPTIONS', () => {
    it('has descriptions for all key skills', () => {
        const requiredSkills = [
            'Block', 'Dodge', 'Mighty Blow', 'Guard', 'Frenzy',
            'Tackle', 'Sure Hands', 'Catch', 'Pass', 'Horns',
            'Claw', 'Accurate', 'Strong Arm', 'Safe Throw',
            'Two Heads', 'Extra Arms', 'No Hands', 'Fend',
            'Piling On', 'Sneaky Git', 'Kick', 'Leader',
            'Hail Mary Pass', 'Dump-Off', 'Diving Catch',
            'Secret Weapon', 'Take Root', 'Dirty Player',
        ];

        for (const skill of requiredSkills) {
            expect(SKILL_DESCRIPTIONS[skill], `Missing description for ${skill}`).toBeDefined();
            expect(SKILL_DESCRIPTIONS[skill].length).toBeGreaterThan(5);
        }
    });
});
