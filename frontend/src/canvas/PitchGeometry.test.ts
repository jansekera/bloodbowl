import { describe, it, expect } from 'vitest';
import { PitchGeometry } from './PitchGeometry';

describe('PitchGeometry', () => {
    const geo = new PitchGeometry(32, 40, 40);

    describe('constants', () => {
        it('has correct pitch dimensions', () => {
            expect(PitchGeometry.COLS).toBe(26);
            expect(PitchGeometry.ROWS).toBe(15);
        });

        it('calculates canvas dimensions correctly', () => {
            expect(geo.pitchWidth).toBe(26 * 32);
            expect(geo.pitchHeight).toBe(15 * 32);
            expect(geo.canvasWidth).toBe(26 * 32 + 2 * 40);
            expect(geo.canvasHeight).toBe(15 * 32 + 2 * 40);
        });
    });

    describe('gridToPixel', () => {
        it('converts (0,0) to top-left cell center', () => {
            const { px, py } = geo.gridToPixel({ x: 0, y: 0 });
            expect(px).toBe(40 + 16); // padding + half cell
            expect(py).toBe(40 + 16);
        });

        it('converts center cell correctly', () => {
            const { px, py } = geo.gridToPixel({ x: 13, y: 7 });
            expect(px).toBe(40 + 13 * 32 + 16);
            expect(py).toBe(40 + 7 * 32 + 16);
        });
    });

    describe('pixelToGrid', () => {
        it('converts pixel in first cell to (0,0)', () => {
            const pos = geo.pixelToGrid(41, 41);
            expect(pos).toEqual({ x: 0, y: 0 });
        });

        it('returns null for pixels outside the pitch', () => {
            expect(geo.pixelToGrid(0, 0)).toBeNull();
            expect(geo.pixelToGrid(39, 40)).toBeNull(); // just before pitch
            expect(geo.pixelToGrid(40 + 26 * 32 + 1, 40)).toBeNull(); // just after
        });

        it('round-trips through gridToPixel', () => {
            const original = { x: 10, y: 7 };
            const { px, py } = geo.gridToPixel(original);
            const result = geo.pixelToGrid(px, py);
            expect(result).toEqual(original);
        });
    });

    describe('zone detection', () => {
        it('detects home end zone at x=0', () => {
            expect(geo.isHomeEndZone({ x: 0, y: 7 })).toBe(true);
            expect(geo.isHomeEndZone({ x: 1, y: 7 })).toBe(false);
        });

        it('detects away end zone at x=25', () => {
            expect(geo.isAwayEndZone({ x: 25, y: 7 })).toBe(true);
            expect(geo.isAwayEndZone({ x: 24, y: 7 })).toBe(false);
        });

        it('detects wide zones (y < 4 or y >= 11)', () => {
            expect(geo.isWideZone({ x: 13, y: 0 })).toBe(true);
            expect(geo.isWideZone({ x: 13, y: 3 })).toBe(true);
            expect(geo.isWideZone({ x: 13, y: 4 })).toBe(false);
            expect(geo.isWideZone({ x: 13, y: 10 })).toBe(false);
            expect(geo.isWideZone({ x: 13, y: 11 })).toBe(true);
            expect(geo.isWideZone({ x: 13, y: 14 })).toBe(true);
        });

        it('detects line of scrimmage', () => {
            expect(geo.isLineOfScrimmage({ x: 12, y: 7 })).toBe(true);
            expect(geo.isLineOfScrimmage({ x: 13, y: 7 })).toBe(true);
            expect(geo.isLineOfScrimmage({ x: 11, y: 7 })).toBe(false);
            expect(geo.isLineOfScrimmage({ x: 14, y: 7 })).toBe(false);
        });
    });

    describe('isOnPitch', () => {
        it('accepts valid positions', () => {
            expect(geo.isOnPitch({ x: 0, y: 0 })).toBe(true);
            expect(geo.isOnPitch({ x: 25, y: 14 })).toBe(true);
            expect(geo.isOnPitch({ x: 13, y: 7 })).toBe(true);
        });

        it('rejects out-of-bounds positions', () => {
            expect(geo.isOnPitch({ x: -1, y: 0 })).toBe(false);
            expect(geo.isOnPitch({ x: 0, y: -1 })).toBe(false);
            expect(geo.isOnPitch({ x: 26, y: 0 })).toBe(false);
            expect(geo.isOnPitch({ x: 0, y: 15 })).toBe(false);
        });
    });

    describe('distance', () => {
        it('returns 0 for same position', () => {
            expect(geo.distance({ x: 5, y: 5 }, { x: 5, y: 5 })).toBe(0);
        });

        it('returns Chebyshev distance', () => {
            expect(geo.distance({ x: 5, y: 5 }, { x: 6, y: 5 })).toBe(1);
            expect(geo.distance({ x: 5, y: 5 }, { x: 6, y: 6 })).toBe(1);
            expect(geo.distance({ x: 5, y: 5 }, { x: 8, y: 7 })).toBe(3);
        });
    });

    describe('getAdjacentPositions', () => {
        it('returns 8 for center position', () => {
            const adj = geo.getAdjacentPositions({ x: 5, y: 5 });
            expect(adj).toHaveLength(8);
        });

        it('returns 3 for corner position', () => {
            const adj = geo.getAdjacentPositions({ x: 0, y: 0 });
            expect(adj).toHaveLength(3);
        });

        it('returns 5 for edge position', () => {
            const adj = geo.getAdjacentPositions({ x: 0, y: 5 });
            expect(adj).toHaveLength(5);
        });

        it('does not include the position itself', () => {
            const pos = { x: 5, y: 5 };
            const adj = geo.getAdjacentPositions(pos);
            const hasSelf = adj.some(p => p.x === pos.x && p.y === pos.y);
            expect(hasSelf).toBe(false);
        });
    });
});
