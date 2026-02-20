import { describe, it, expect } from 'vitest';
import type { MoveTarget } from '../api/types';

/** Risk level calculation matching PitchRenderer.drawRiskHighlights logic */
function getRiskLevel(target: MoveTarget): 'safe' | 'low' | 'medium' | 'high' {
    const totalRolls = target.dodges + target.gfis;
    if (totalRolls === 0) return 'safe';
    if (totalRolls === 1) return 'low';
    if (totalRolls === 2) return 'medium';
    return 'high';
}

function getRiskLabel(target: MoveTarget): string {
    const parts: string[] = [];
    if (target.dodges > 0) parts.push(`D${target.dodges}`);
    if (target.gfis > 0) parts.push(`G${target.gfis}`);
    return parts.join('+');
}

describe('Risk Highlight', () => {
    it('classifies safe move (no rolls)', () => {
        expect(getRiskLevel({ x: 5, y: 5, dodges: 0, gfis: 0 })).toBe('safe');
    });

    it('classifies low risk (1 roll)', () => {
        expect(getRiskLevel({ x: 5, y: 5, dodges: 1, gfis: 0 })).toBe('low');
        expect(getRiskLevel({ x: 5, y: 5, dodges: 0, gfis: 1 })).toBe('low');
    });

    it('classifies medium risk (2 rolls)', () => {
        expect(getRiskLevel({ x: 5, y: 5, dodges: 1, gfis: 1 })).toBe('medium');
        expect(getRiskLevel({ x: 5, y: 5, dodges: 2, gfis: 0 })).toBe('medium');
    });

    it('classifies high risk (3+ rolls)', () => {
        expect(getRiskLevel({ x: 5, y: 5, dodges: 2, gfis: 1 })).toBe('high');
        expect(getRiskLevel({ x: 5, y: 5, dodges: 3, gfis: 0 })).toBe('high');
    });

    it('generates correct risk labels', () => {
        expect(getRiskLabel({ x: 0, y: 0, dodges: 0, gfis: 0 })).toBe('');
        expect(getRiskLabel({ x: 0, y: 0, dodges: 1, gfis: 0 })).toBe('D1');
        expect(getRiskLabel({ x: 0, y: 0, dodges: 0, gfis: 1 })).toBe('G1');
        expect(getRiskLabel({ x: 0, y: 0, dodges: 2, gfis: 1 })).toBe('D2+G1');
    });
});
