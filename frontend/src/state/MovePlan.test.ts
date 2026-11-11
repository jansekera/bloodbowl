import { describe, it, expect } from 'vitest';
import { clickTarget, describePlan, nextStep, planAfterStep, planTarget } from './MovePlan';
import type { MoveTarget } from '../api/types';

/** P296: plán pohybu po krocích — první klik plánuje, druhý jde; krok = první pole trasy; plán přežije krok. */
const target: MoveTarget = {
    x: 8, y: 5, dodges: 1, gfis: 1, successChance: 55.6,
    path: [{ x: 6, y: 5, dodge: true, gfi: false }, { x: 7, y: 5, dodge: false, gfi: false }, { x: 8, y: 5, dodge: false, gfi: true }],
};
const other: MoveTarget = { x: 6, y: 6, dodges: 0, gfis: 0, path: [{ x: 6, y: 6, dodge: false, gfi: false }] };

describe('MovePlan', () => {
    it('first click on a target only plans the route', () => {
        expect(clickTarget(null, 1, { x: 8, y: 5 })).toEqual({ kind: 'plan', plan: { playerId: 1, target: { x: 8, y: 5 } } });
    });

    it('second click on the same target goes the whole route', () => {
        const plan = { playerId: 1, target: { x: 8, y: 5 } };
        expect(clickTarget(plan, 1, { x: 8, y: 5 })).toEqual({ kind: 'go' });
    });

    it('click on another target (or with another player) replans', () => {
        const plan = { playerId: 1, target: { x: 8, y: 5 } };
        expect(clickTarget(plan, 1, { x: 6, y: 6 })).toEqual({ kind: 'plan', plan: { playerId: 1, target: { x: 6, y: 6 } } });
        expect(clickTarget(plan, 2, { x: 8, y: 5 }).kind).toBe('plan');
    });

    it('a step is the first square of the route', () => {
        expect(nextStep({ playerId: 1, target: { x: 8, y: 5 } }, [other, target])).toEqual({ x: 6, y: 5 });
        expect(nextStep({ playerId: 1, target: { x: 9, y: 9 } }, [other, target])).toBeNull();
    });

    it('the plan survives a step while the same move stays open and the target is reachable', () => {
        const plan = { playerId: 1, target: { x: 8, y: 5 } };
        expect(planAfterStep(plan, 1, [target])).toBe(plan);
    });

    it('the plan ends when the move was closed, the target is gone or reached', () => {
        const plan = { playerId: 1, target: { x: 8, y: 5 } };
        expect(planAfterStep(plan, null, [target])).toBeNull();   // pád, turnover, konec pohybu
        expect(planAfterStep(plan, 2, [target])).toBeNull();
        expect(planAfterStep(plan, 1, [other])).toBeNull();      // cíl dosažen / nedosažitelný
    });

    it('describes the route risk', () => {
        expect(planTarget({ playerId: 1, target: { x: 8, y: 5 } }, [target])).toBe(target);
        expect(describePlan(target)).toBe('3 squares · 1 dodge · 1 GFI · 56 %');
        expect(describePlan(other)).toBe('1 square');
    });
});
