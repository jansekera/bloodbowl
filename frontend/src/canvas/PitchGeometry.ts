import type { Position } from '../api/types';

/** Pitch dimensions and coordinate conversions */
export class PitchGeometry {
    static readonly COLS = 26;
    static readonly ROWS = 15;

    /** Line of scrimmage column (center) */
    static readonly LOS_COL = 13;

    /** Wide zone rows: top 0-3, bottom 11-14 */
    static readonly WIDE_ZONE_TOP = 4;
    static readonly WIDE_ZONE_BOTTOM = 11;

    readonly cellSize: number;
    readonly paddingX: number;
    readonly paddingY: number;
    readonly pitchWidth: number;
    readonly pitchHeight: number;
    readonly canvasWidth: number;
    readonly canvasHeight: number;

    constructor(cellSize: number = 32, paddingX: number = 40, paddingY: number = 40) {
        this.cellSize = cellSize;
        this.paddingX = paddingX;
        this.paddingY = paddingY;
        this.pitchWidth = PitchGeometry.COLS * cellSize;
        this.pitchHeight = PitchGeometry.ROWS * cellSize;
        this.canvasWidth = this.pitchWidth + 2 * paddingX;
        this.canvasHeight = this.pitchHeight + 2 * paddingY;
    }

    /** Convert grid position to pixel center of that cell */
    gridToPixel(pos: Position): { px: number; py: number } {
        return {
            px: this.paddingX + pos.x * this.cellSize + this.cellSize / 2,
            py: this.paddingY + pos.y * this.cellSize + this.cellSize / 2,
        };
    }

    /** Convert pixel position to grid position (or null if off-pitch) */
    pixelToGrid(px: number, py: number): Position | null {
        const gx = Math.floor((px - this.paddingX) / this.cellSize);
        const gy = Math.floor((py - this.paddingY) / this.cellSize);

        if (gx < 0 || gx >= PitchGeometry.COLS || gy < 0 || gy >= PitchGeometry.ROWS) {
            return null;
        }

        return { x: gx, y: gy };
    }

    /** Top-left pixel of a grid cell */
    gridToTopLeft(pos: Position): { px: number; py: number } {
        return {
            px: this.paddingX + pos.x * this.cellSize,
            py: this.paddingY + pos.y * this.cellSize,
        };
    }

    /** Check if a position is in the home end zone (x=0) */
    isHomeEndZone(pos: Position): boolean {
        return pos.x === 0;
    }

    /** Check if a position is in the away end zone (x=25) */
    isAwayEndZone(pos: Position): boolean {
        return pos.x === PitchGeometry.COLS - 1;
    }

    /** Check if a position is in a wide zone (top or bottom rows) */
    isWideZone(pos: Position): boolean {
        return pos.y < PitchGeometry.WIDE_ZONE_TOP
            || pos.y >= PitchGeometry.WIDE_ZONE_BOTTOM;
    }

    /** Check if a position is on the line of scrimmage */
    isLineOfScrimmage(pos: Position): boolean {
        return pos.x === PitchGeometry.LOS_COL - 1
            || pos.x === PitchGeometry.LOS_COL;
    }

    /** Check if a position is valid on the pitch */
    isOnPitch(pos: Position): boolean {
        return pos.x >= 0 && pos.x < PitchGeometry.COLS
            && pos.y >= 0 && pos.y < PitchGeometry.ROWS;
    }

    /** Get Chebyshev distance (max of dx, dy) between two positions */
    distance(a: Position, b: Position): number {
        return Math.max(Math.abs(a.x - b.x), Math.abs(a.y - b.y));
    }

    /** Get all adjacent positions (including diagonals) that are on the pitch */
    getAdjacentPositions(pos: Position): Position[] {
        const result: Position[] = [];
        for (let dx = -1; dx <= 1; dx++) {
            for (let dy = -1; dy <= 1; dy++) {
                if (dx === 0 && dy === 0) continue;
                const adj = { x: pos.x + dx, y: pos.y + dy };
                if (this.isOnPitch(adj)) {
                    result.push(adj);
                }
            }
        }
        return result;
    }
}
