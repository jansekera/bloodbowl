import type { MatchPlayer, BallState, Position, MoveTarget, PathStep } from '../api/types';
import type { AnimationState } from '../animation/animations';
import { PitchGeometry } from './PitchGeometry';

export interface PitchColors {
    grass: string;
    grassAlt: string;
    endZoneHome: string;
    endZoneAway: string;
    wideZone: string;
    gridLine: string;
    losLine: string;
    highlight: string;
    highlightMove: string;
    highlightBlock: string;
}

const DEFAULT_COLORS: PitchColors = {
    grass: '#2d5a1e',
    grassAlt: '#2a5319',
    endZoneHome: '#1a3a5c',
    endZoneAway: '#5c1a1a',
    wideZone: 'rgba(255, 255, 255, 0.03)',
    gridLine: 'rgba(255, 255, 255, 0.12)',
    losLine: 'rgba(255, 255, 255, 0.5)',
    highlight: 'rgba(255, 255, 100, 0.3)',
    highlightMove: 'rgba(100, 200, 255, 0.25)',
    highlightBlock: 'rgba(255, 100, 100, 0.25)',
};

export interface TeamColors {
    primary: string;
    secondary: string;
    text: string;
}

const HOME_COLORS: TeamColors = { primary: '#2980b9', secondary: '#1a5276', text: '#fff' };
const AWAY_COLORS: TeamColors = { primary: '#c0392b', secondary: '#7b241c', text: '#fff' };

/** Position-based colors (override team primary) */
interface PositionStyle {
    fill: string;
    stroke: string;
}

const POSITION_COLORS: Record<string, PositionStyle> = {
    // Blitzers — red/orange
    'Blitzer':          { fill: '#e74c3c', stroke: '#a93226' },
    'Blitz-Ra':         { fill: '#e74c3c', stroke: '#a93226' },
    'Berserker':        { fill: '#e74c3c', stroke: '#a93226' },
    'Werewolf':         { fill: '#e74c3c', stroke: '#a93226' },
    'Blocker':          { fill: '#e74c3c', stroke: '#a93226' },
    'Bull Centaur':     { fill: '#e74c3c', stroke: '#a93226' },
    'Khorne Herald':    { fill: '#e74c3c', stroke: '#a93226' },
    'Pit Fighter':      { fill: '#e74c3c', stroke: '#a93226' },
    // Catchers — cyan/teal
    'Catcher':          { fill: '#1abc9c', stroke: '#148f77' },
    'Gutter Runner':    { fill: '#1abc9c', stroke: '#148f77' },
    'Runner':           { fill: '#1abc9c', stroke: '#148f77' },
    'Pogoer':           { fill: '#1abc9c', stroke: '#148f77' },
    // Throwers — gold/yellow
    'Thrower':          { fill: '#f39c12', stroke: '#b7770d' },
    'Passer':           { fill: '#f39c12', stroke: '#b7770d' },
    // Big guys — purple
    'Ogre':             { fill: '#8e44ad', stroke: '#6c3483' },
    'Troll':            { fill: '#8e44ad', stroke: '#6c3483' },
    'Treeman':          { fill: '#8e44ad', stroke: '#6c3483' },
    'Minotaur':         { fill: '#8e44ad', stroke: '#6c3483' },
    'Kroxigor':         { fill: '#8e44ad', stroke: '#6c3483' },
    'Rat Ogre':         { fill: '#8e44ad', stroke: '#6c3483' },
    'Deathroller':      { fill: '#8e44ad', stroke: '#6c3483' },
    'Beast of Nurgle':  { fill: '#8e44ad', stroke: '#6c3483' },
    'Tomb Guardian':    { fill: '#8e44ad', stroke: '#6c3483' },
    'Bloodthirster':    { fill: '#8e44ad', stroke: '#6c3483' },
    'Warpstone Troll':  { fill: '#8e44ad', stroke: '#6c3483' },
    // Special weapons — dark orange
    'Bombardier':       { fill: '#d35400', stroke: '#a04000' },
    'Looney':           { fill: '#d35400', stroke: '#a04000' },
    'Fanatic':          { fill: '#d35400', stroke: '#a04000' },
    'Bloodletter':      { fill: '#d35400', stroke: '#a04000' },
};

function getPositionStyle(player: MatchPlayer, teamColors: TeamColors): PositionStyle {
    return POSITION_COLORS[player.positionalName] ?? { fill: teamColors.primary, stroke: teamColors.secondary };
}

function getRadiusScale(player: MatchPlayer): number {
    const st = player.stats.strength;
    if (st >= 5) return 1.15;
    if (st >= 4) return 1.07;
    if (st <= 2) return 0.85;
    return 1.0;
}

export class PitchRenderer {
    private ctx: CanvasRenderingContext2D;
    readonly geometry: PitchGeometry;
    private colors: PitchColors;
    private animState: AnimationState | null = null;

    constructor(
        private canvas: HTMLCanvasElement,
        cellSize: number = 32,
        colors?: Partial<PitchColors>,
    ) {
        const ctx = canvas.getContext('2d');
        if (!ctx) throw new Error('Cannot get 2d context');
        this.ctx = ctx;

        this.geometry = new PitchGeometry(cellSize);
        this.colors = { ...DEFAULT_COLORS, ...colors };

        canvas.width = this.geometry.canvasWidth;
        canvas.height = this.geometry.canvasHeight;
    }

    /** Set animation state for rendering overrides */
    setAnimationState(state: AnimationState | null): void {
        this.animState = state;
    }

    /** Full render of the pitch with all elements */
    render(
        players: MatchPlayer[],
        ball: BallState | null,
        highlightedCells: Position[] = [],
        selectedPlayerId: number | null = null,
        hoveredCell: Position | null = null,
    ): void {
        this.clear();
        this.drawPitch();
        this.drawHighlights(highlightedCells);

        if (hoveredCell) {
            this.drawCellHighlight(hoveredCell, 'rgba(255, 255, 255, 0.1)');
        }

        this.drawPlayers(players, selectedPlayerId);

        if (this.animState?.flashCell) {
            this.drawCellHighlight(this.animState.flashCell.pos, this.animState.flashCell.color);
        }

        // Ball: animation override takes priority
        if (this.animState?.ballOverride) {
            this.drawBallAtPixel(this.animState.ballOverride.px, this.animState.ballOverride.py);
        } else if (ball && !ball.isHeld && ball.position) {
            this.drawBall(ball.position);
        }
    }

    private clear(): void {
        this.ctx.fillStyle = '#111';
        this.ctx.fillRect(0, 0, this.canvas.width, this.canvas.height);
    }

    private drawPitch(): void {
        const { ctx, geometry: g, colors: c } = this;

        // Draw cells
        for (let x = 0; x < PitchGeometry.COLS; x++) {
            for (let y = 0; y < PitchGeometry.ROWS; y++) {
                const pos = { x, y };
                const { px, py } = g.gridToTopLeft(pos);

                // Base color (checkerboard)
                ctx.fillStyle = (x + y) % 2 === 0 ? c.grass : c.grassAlt;
                ctx.fillRect(px, py, g.cellSize, g.cellSize);

                // End zones
                if (g.isHomeEndZone(pos)) {
                    ctx.fillStyle = c.endZoneHome;
                    ctx.fillRect(px, py, g.cellSize, g.cellSize);
                } else if (g.isAwayEndZone(pos)) {
                    ctx.fillStyle = c.endZoneAway;
                    ctx.fillRect(px, py, g.cellSize, g.cellSize);
                }

                // Wide zone overlay
                if (g.isWideZone(pos)) {
                    ctx.fillStyle = c.wideZone;
                    ctx.fillRect(px, py, g.cellSize, g.cellSize);
                }
            }
        }

        // Grid lines
        ctx.strokeStyle = c.gridLine;
        ctx.lineWidth = 0.5;
        for (let x = 0; x <= PitchGeometry.COLS; x++) {
            const px = g.paddingX + x * g.cellSize;
            ctx.beginPath();
            ctx.moveTo(px, g.paddingY);
            ctx.lineTo(px, g.paddingY + g.pitchHeight);
            ctx.stroke();
        }
        for (let y = 0; y <= PitchGeometry.ROWS; y++) {
            const py = g.paddingY + y * g.cellSize;
            ctx.beginPath();
            ctx.moveTo(g.paddingX, py);
            ctx.lineTo(g.paddingX + g.pitchWidth, py);
            ctx.stroke();
        }

        // Line of scrimmage (thicker)
        ctx.strokeStyle = c.losLine;
        ctx.lineWidth = 2;
        const losPx = g.paddingX + PitchGeometry.LOS_COL * g.cellSize;
        ctx.beginPath();
        ctx.moveTo(losPx, g.paddingY);
        ctx.lineTo(losPx, g.paddingY + g.pitchHeight);
        ctx.stroke();

        // Wide zone separator lines
        ctx.strokeStyle = 'rgba(255, 255, 255, 0.2)';
        ctx.lineWidth = 1;
        ctx.setLineDash([4, 4]);
        for (const row of [PitchGeometry.WIDE_ZONE_TOP, PitchGeometry.WIDE_ZONE_BOTTOM]) {
            const py = g.paddingY + row * g.cellSize;
            ctx.beginPath();
            ctx.moveTo(g.paddingX, py);
            ctx.lineTo(g.paddingX + g.pitchWidth, py);
            ctx.stroke();
        }
        ctx.setLineDash([]);

        // Pitch border
        ctx.strokeStyle = '#fff';
        ctx.lineWidth = 2;
        ctx.strokeRect(g.paddingX, g.paddingY, g.pitchWidth, g.pitchHeight);

        // Labels
        ctx.fillStyle = '#aaa';
        ctx.font = '11px system-ui, sans-serif';
        ctx.textAlign = 'center';
        ctx.fillText('HOME', g.paddingX + g.cellSize / 2, g.paddingY - 10);
        ctx.fillText('AWAY', g.paddingX + g.pitchWidth - g.cellSize / 2, g.paddingY - 10);
    }

    private drawHighlights(cells: Position[]): void {
        for (const pos of cells) {
            this.drawCellHighlight(pos, this.colors.highlightMove);
        }
    }

    private drawCellHighlight(pos: Position, color: string): void {
        const { ctx, geometry: g } = this;
        const { px, py } = g.gridToTopLeft(pos);

        ctx.fillStyle = color;
        ctx.fillRect(px + 1, py + 1, g.cellSize - 2, g.cellSize - 2);
    }

    private drawPlayers(players: MatchPlayer[], selectedId: number | null): void {
        for (const player of players) {
            if (!player.position) continue;
            this.drawPlayerSprite(player, player.id === selectedId);
        }
    }

    private drawPlayerSprite(player: MatchPlayer, isSelected: boolean): void {
        if (!player.position) return;

        const { ctx, geometry: g } = this;
        let { px, py } = g.gridToPixel(player.position);

        // Animation overrides
        const override = this.animState?.playerOverrides.get(player.id);
        if (override) {
            px = override.px;
            py = override.py;
        }

        const shake = this.animState?.shakePlayer;
        if (shake && shake.playerId === player.id) {
            px += shake.offsetX;
            py += shake.offsetY;
        }

        const teamColors = player.teamSide === 'home' ? HOME_COLORS : AWAY_COLORS;
        const posStyle = getPositionStyle(player, teamColors);
        const radius = g.cellSize * 0.38 * getRadiusScale(player);

        // Selection ring
        if (isSelected) {
            ctx.beginPath();
            ctx.arc(px, py, radius + 3, 0, Math.PI * 2);
            ctx.strokeStyle = '#ffff00';
            ctx.lineWidth = 2;
            ctx.stroke();
        }

        // Skill ring: Block = solid white, Dodge = dashed green, both = solid gold
        const hasBlock = player.skills.includes('Block');
        const hasDodge = player.skills.includes('Dodge');
        if (hasBlock || hasDodge) {
            ctx.save();
            if (hasDodge && !hasBlock) {
                ctx.setLineDash([3, 3]);
                ctx.strokeStyle = 'rgba(100, 255, 100, 0.8)';
            } else if (hasBlock && hasDodge) {
                ctx.strokeStyle = 'rgba(255, 215, 0, 0.9)';
            } else {
                ctx.strokeStyle = 'rgba(255, 255, 255, 0.7)';
            }
            ctx.beginPath();
            ctx.arc(px, py, radius + 1.5, 0, Math.PI * 2);
            ctx.lineWidth = 2;
            ctx.stroke();
            ctx.setLineDash([]);
            ctx.restore();
        }

        // Player circle — position-based color
        ctx.beginPath();
        ctx.arc(px, py, radius, 0, Math.PI * 2);
        ctx.fillStyle = posStyle.fill;
        ctx.fill();
        ctx.strokeStyle = posStyle.stroke;
        ctx.lineWidth = 1.5;
        ctx.stroke();

        // Team indicator: thin inner ring in team color (so you can tell home/away)
        ctx.beginPath();
        ctx.arc(px, py, radius - 2, 0, Math.PI * 2);
        ctx.strokeStyle = teamColors.primary;
        ctx.lineWidth = 1.5;
        ctx.stroke();

        // Player number
        ctx.fillStyle = teamColors.text;
        ctx.font = `bold ${Math.round(g.cellSize * 0.38)}px system-ui, sans-serif`;
        ctx.textAlign = 'center';
        ctx.textBaseline = 'middle';
        ctx.fillText(String(player.number), px, py + 1);

        // Prone/stunned indicator
        if (player.state === 'prone' || player.state === 'stunned') {
            ctx.save();
            ctx.globalAlpha = 0.5;
            ctx.beginPath();
            ctx.arc(px, py, radius, 0, Math.PI * 2);
            ctx.fillStyle = player.state === 'stunned' ? '#ff0' : '#888';
            ctx.fill();
            ctx.restore();

            // Redraw number on top
            ctx.fillStyle = '#000';
            ctx.fillText(String(player.number), px, py + 1);
        }

        // Lost tacklezones indicator (dashed ring)
        if (player.lostTacklezones) {
            ctx.save();
            ctx.setLineDash([3, 3]);
            ctx.beginPath();
            ctx.arc(px, py, radius + 1, 0, Math.PI * 2);
            ctx.strokeStyle = 'rgba(128, 128, 128, 0.7)';
            ctx.lineWidth = 2;
            ctx.stroke();
            ctx.setLineDash([]);
            ctx.restore();
        }

        // Has-acted indicator (small dot)
        if (player.hasMoved || player.hasActed) {
            ctx.beginPath();
            ctx.arc(px + radius - 2, py - radius + 2, 3, 0, Math.PI * 2);
            ctx.fillStyle = '#aaa';
            ctx.fill();
        }

        // Ball carrier indicator
        // (drawn via drawBall when ball.isHeld, but also add a subtle ring)
    }

    private drawBall(position: Position): void {
        const { geometry: g } = this;
        const { px, py } = g.gridToPixel(position);
        this.drawBallAtPixel(px, py - g.cellSize * 0.25);
    }

    /** Draw ball at exact pixel coordinates (used by animations) */
    private drawBallAtPixel(px: number, py: number): void {
        const { ctx, geometry: g } = this;
        const ballRadius = g.cellSize * 0.15;

        ctx.beginPath();
        ctx.arc(px, py, ballRadius, 0, Math.PI * 2);
        ctx.fillStyle = '#8B4513';
        ctx.fill();
        ctx.strokeStyle = '#fff';
        ctx.lineWidth = 1;
        ctx.stroke();
    }

    /** Highlight valid setup zone for a team side */
    drawSetupZone(side: 'home' | 'away'): void {
        const { ctx, geometry: g } = this;
        const minX = side === 'home' ? 0 : PitchGeometry.LOS_COL;
        const maxX = side === 'home' ? PitchGeometry.LOS_COL - 1 : PitchGeometry.COLS - 1;

        ctx.fillStyle = 'rgba(100, 255, 100, 0.08)';
        for (let x = minX; x <= maxX; x++) {
            for (let y = 0; y < PitchGeometry.ROWS; y++) {
                const { px, py } = g.gridToTopLeft({ x, y });
                ctx.fillRect(px + 1, py + 1, g.cellSize - 2, g.cellSize - 2);
            }
        }
    }

    /** Draw move targets with risk-based color coding */
    drawRiskHighlights(targets: MoveTarget[]): void {
        const { ctx, geometry: g } = this;

        for (const t of targets) {
            const totalRolls = t.dodges + t.gfis;
            const { px, py } = g.gridToTopLeft({ x: t.x, y: t.y });

            let color: string;
            if (totalRolls === 0) {
                color = 'rgba(100, 255, 100, 0.25)';
            } else if (totalRolls === 1) {
                color = 'rgba(255, 255, 100, 0.3)';
            } else if (totalRolls === 2) {
                color = 'rgba(255, 165, 0, 0.3)';
            } else {
                color = 'rgba(255, 80, 80, 0.3)';
            }

            ctx.fillStyle = color;
            ctx.fillRect(px + 1, py + 1, g.cellSize - 2, g.cellSize - 2);

            // Risk label
            if (totalRolls > 0) {
                const parts: string[] = [];
                if (t.dodges > 0) parts.push(`D${t.dodges}`);
                if (t.gfis > 0) parts.push(`G${t.gfis}`);
                const label = parts.join('+');

                ctx.fillStyle = 'rgba(255, 255, 255, 0.8)';
                ctx.font = `${Math.round(g.cellSize * 0.28)}px system-ui, sans-serif`;
                ctx.textAlign = 'center';
                ctx.textBaseline = 'bottom';
                ctx.fillText(label, px + g.cellSize / 2, py + g.cellSize - 1);
            }
        }
    }

    /** Draw the movement path line from player to hovered destination */
    drawMovePath(playerPos: Position, path: PathStep[]): void {
        if (path.length === 0) return;

        const { ctx, geometry: g } = this;
        const cs = g.cellSize;

        // Build full list of positions: start from player, then each step
        const points: Position[] = [playerPos, ...path];

        ctx.save();
        ctx.lineWidth = 3;
        ctx.lineCap = 'round';
        ctx.lineJoin = 'round';

        // Draw path segments with color based on step risk
        for (let i = 0; i < points.length - 1; i++) {
            const from = g.gridToPixel(points[i]);
            const to = g.gridToPixel(points[i + 1]);
            const step = path[i]; // path[i] = the step entering points[i+1]

            // Color by risk: green=safe, yellow=dodge, orange=GFI, red=dodge+GFI
            if (step.dodge && step.gfi) {
                ctx.strokeStyle = 'rgba(255, 80, 80, 0.9)';
            } else if (step.dodge) {
                ctx.strokeStyle = 'rgba(255, 200, 50, 0.9)';
            } else if (step.gfi) {
                ctx.strokeStyle = 'rgba(255, 165, 0, 0.9)';
            } else {
                ctx.strokeStyle = 'rgba(100, 255, 100, 0.8)';
            }

            ctx.beginPath();
            ctx.moveTo(from.px, from.py);
            ctx.lineTo(to.px, to.py);
            ctx.stroke();
        }

        // Draw dots at each step
        for (let i = 0; i < path.length; i++) {
            const step = path[i];
            const { px, py } = g.gridToPixel(step);
            const dotRadius = cs * 0.08;

            ctx.beginPath();
            ctx.arc(px, py, dotRadius, 0, Math.PI * 2);

            if (step.dodge) {
                ctx.fillStyle = '#ffcc00';
            } else if (step.gfi) {
                ctx.fillStyle = '#ff8800';
            } else {
                ctx.fillStyle = '#66ff66';
            }
            ctx.fill();
            ctx.strokeStyle = 'rgba(0, 0, 0, 0.5)';
            ctx.lineWidth = 1;
            ctx.stroke();
            ctx.lineWidth = 3;
        }

        // Arrow at destination
        const last = g.gridToPixel(path[path.length - 1]);
        const prev = points.length >= 2
            ? g.gridToPixel(points[points.length - 2])
            : last;

        const angle = Math.atan2(last.py - prev.py, last.px - prev.px);
        const arrowSize = cs * 0.2;

        ctx.fillStyle = 'rgba(255, 255, 255, 0.9)';
        ctx.beginPath();
        ctx.moveTo(
            last.px + Math.cos(angle) * arrowSize,
            last.py + Math.sin(angle) * arrowSize,
        );
        ctx.lineTo(
            last.px + Math.cos(angle + 2.5) * arrowSize * 0.7,
            last.py + Math.sin(angle + 2.5) * arrowSize * 0.7,
        );
        ctx.lineTo(
            last.px + Math.cos(angle - 2.5) * arrowSize * 0.7,
            last.py + Math.sin(angle - 2.5) * arrowSize * 0.7,
        );
        ctx.closePath();
        ctx.fill();

        ctx.restore();
    }

    /** Draw a "selected target" ring around a cell (used for multi-block first target) */
    drawSelectedTarget(position: Position): void {
        const { ctx, geometry: g } = this;
        const { px, py } = g.gridToPixel(position);
        const radius = g.cellSize * 0.45;

        ctx.save();
        ctx.beginPath();
        ctx.arc(px, py, radius, 0, Math.PI * 2);
        ctx.strokeStyle = 'rgba(255, 80, 80, 0.9)';
        ctx.lineWidth = 3;
        ctx.stroke();

        // Checkmark
        const s = g.cellSize * 0.15;
        ctx.strokeStyle = 'rgba(255, 255, 255, 0.9)';
        ctx.lineWidth = 2;
        ctx.beginPath();
        ctx.moveTo(px - s, py);
        ctx.lineTo(px - s * 0.3, py + s * 0.7);
        ctx.lineTo(px + s, py - s * 0.5);
        ctx.stroke();
        ctx.restore();
    }

    /** Draw ball on a player (carrier) */
    drawBallOnPlayer(position: Position): void {
        const { ctx, geometry: g } = this;
        const { px, py } = g.gridToPixel(position);
        const ballRadius = g.cellSize * 0.12;

        ctx.beginPath();
        ctx.arc(px + g.cellSize * 0.28, py - g.cellSize * 0.28, ballRadius, 0, Math.PI * 2);
        ctx.fillStyle = '#DAA520';
        ctx.fill();
        ctx.strokeStyle = '#fff';
        ctx.lineWidth = 0.5;
        ctx.stroke();
    }
}
