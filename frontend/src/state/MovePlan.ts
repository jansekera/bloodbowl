import type { MoveTarget, Position } from '../api/types';

/**
 * P296 (uživatel 11.11.2026: „klik na cíl, kdy se ukáže od AI naplánovaná trasa, i možnost dělat krok za krokem
 * a rozhodovat se uprostřed, např. podle úspěchu / neúspěchu dodge“).
 *
 * První klik na cíl pohybu trasu jen naplánuje (ukáže ji, nic se nehází). Druhý klik na týž cíl nebo „Go“ ji
 * proběhne celou; „Step“ udělá jen první krok trasy (`partial` — pohyb zůstane otevřený). Po kroku server trasu
 * k témuž cíli spočítá znovu z nového pole. Plán platí, dokud je pohyb téhož hráče otevřený a cíl dosažitelný.
 */
export interface MovePlan {
    playerId: number;
    target: Position;
}

export type PlanClick = { kind: 'plan'; plan: MovePlan } | { kind: 'go' };

function samePos(a: Position, b: Position): boolean {
    return a.x === b.x && a.y === b.y;
}

/** Klik na pole pohybu: týž cíl podruhé = jít celou trasou, jinak (nový) plán. */
export function clickTarget(plan: MovePlan | null, playerId: number, cell: Position): PlanClick {
    if (plan && plan.playerId === playerId && samePos(plan.target, cell)) {
        return { kind: 'go' };
    }
    return { kind: 'plan', plan: { playerId, target: { x: cell.x, y: cell.y } } };
}

/** Cíl plánu mezi poli pohybu (s trasou a rizikem), nebo null, když už dosažitelný není. */
export function planTarget(plan: MovePlan | null, moveTargets: MoveTarget[]): MoveTarget | null {
    if (!plan) return null;
    return moveTargets.find(t => samePos(t, plan.target)) ?? null;
}

/** Pole prvního kroku trasy (krok Leapu je pole dopadu). */
export function nextStep(plan: MovePlan | null, moveTargets: MoveTarget[]): Position | null {
    const target = planTarget(plan, moveTargets);
    if (!target) return null;
    const first = target.path?.[0];
    return first ? { x: first.x, y: first.y } : { x: target.x, y: target.y };
}

/**
 * Po kroku: plán zůstává, když server nechal pohyb téhož hráče otevřený (`openMovePlayerId`) a cíl je
 * z nového pole pořád dosažitelný. Jinak (pád, turnover, cíl dosažen, jiný hráč) končí.
 */
export function planAfterStep(plan: MovePlan | null, continuationId: number | null | undefined, moveTargets: MoveTarget[]): MovePlan | null {
    if (!plan || continuationId !== plan.playerId) return null;
    return planTarget(plan, moveTargets) ? plan : null;
}

/** Krátký popis trasy pro lištu: počet polí, úhyby, GFI, šance. */
export function describePlan(target: MoveTarget): string {
    const steps = target.path?.length ?? 0;
    const parts = [`${steps} square${steps === 1 ? '' : 's'}`];
    if (target.dodges > 0) parts.push(`${target.dodges} dodge${target.dodges === 1 ? '' : 's'}`);
    if (target.gfis > 0) parts.push(`${target.gfis} GFI`);
    if (target.successChance !== undefined) parts.push(`${Math.round(target.successChance)} %`);
    return parts.join(' · ');
}
