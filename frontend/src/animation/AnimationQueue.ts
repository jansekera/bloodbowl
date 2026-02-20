import type { GameEvent, Position } from '../api/types';
import type { AnimationState } from './animations';
import {
    createAnimationState,
    createBallArcAnimation,
    createBallBounceAnimation,
    createDelayAnimation,
    createFlashAnimation,
    createMoveAnimation,
    createShakeAnimation,
    type Animation,
} from './animations';

export class AnimationQueue {
    private queue: Animation[] = [];
    private current: Animation | null = null;
    private startTime = 0;
    private animFrameId = 0;
    private resolvePromise: (() => void) | null = null;
    private _isAnimating = false;
    readonly state: AnimationState;

    constructor(
        private renderCallback: () => void,
        private gridToPixel: (pos: Position) => { px: number; py: number },
    ) {
        this.state = createAnimationState();
    }

    get isAnimating(): boolean {
        return this._isAnimating;
    }

    /** Convert game events into animations and play them sequentially */
    async playEvents(events: GameEvent[]): Promise<void> {
        const animations = this.eventsToAnimations(events);
        if (animations.length === 0) return;

        this.queue = animations;
        this._isAnimating = true;

        return new Promise<void>((resolve) => {
            this.resolvePromise = resolve;
            this.playNext();
        });
    }

    /** Skip all remaining animations */
    skip(): void {
        if (this.animFrameId) {
            cancelAnimationFrame(this.animFrameId);
            this.animFrameId = 0;
        }
        if (this.current?.finish) {
            this.current.finish();
        }
        for (const anim of this.queue) {
            if (anim.finish) anim.finish();
        }
        this.queue = [];
        this.current = null;
        this._isAnimating = false;
        this.renderCallback();
        if (this.resolvePromise) {
            this.resolvePromise();
            this.resolvePromise = null;
        }
    }

    private playNext(): void {
        if (this.queue.length === 0) {
            this.current = null;
            this._isAnimating = false;
            this.renderCallback();
            if (this.resolvePromise) {
                this.resolvePromise();
                this.resolvePromise = null;
            }
            return;
        }

        this.current = this.queue.shift()!;
        if (this.current.start) {
            this.current.start();
        }
        this.startTime = performance.now();
        this.tick();
    }

    private tick = (): void => {
        if (!this.current) return;

        const elapsed = performance.now() - this.startTime;
        const progress = Math.min(1, elapsed / this.current.duration);

        this.current.update(progress);
        this.renderCallback();

        if (progress < 1) {
            this.animFrameId = requestAnimationFrame(this.tick);
        } else {
            if (this.current.finish) {
                this.current.finish();
            }
            this.animFrameId = 0;
            this.playNext();
        }
    };

    /** Parse events into animation primitives */
    private eventsToAnimations(events: GameEvent[]): Animation[] {
        const anims: Animation[] = [];

        for (const event of events) {
            const data = event.data;

            switch (event.type) {
                case 'player_move': {
                    const from = this.parsePosition(data['from'] as string);
                    const to = this.parsePosition(data['to'] as string);
                    if (from && to) {
                        anims.push(createMoveAnimation(
                            this.state,
                            data['playerId'] as number,
                            from,
                            to,
                            this.gridToPixel,
                            150,
                        ));
                    }
                    break;
                }

                case 'block': {
                    const targetId = data['targetId'] as number | undefined;
                    if (targetId) {
                        anims.push(createShakeAnimation(this.state, targetId, 4, 250));
                    }
                    break;
                }

                case 'push': {
                    const from = this.parsePosition(data['from'] as string);
                    const to = this.parsePosition(data['to'] as string);
                    const pushed = data['playerId'] as number | undefined;
                    if (from && to && pushed) {
                        anims.push(createMoveAnimation(
                            this.state,
                            pushed,
                            from,
                            to,
                            this.gridToPixel,
                            200,
                        ));
                    }
                    break;
                }

                case 'pass': {
                    const from = this.parsePosition(data['from'] as string);
                    const to = this.parsePosition(data['to'] as string);
                    if (from && to) {
                        anims.push(createBallArcAnimation(
                            this.state,
                            from,
                            to,
                            this.gridToPixel,
                            40,
                            400,
                        ));
                    }
                    break;
                }

                case 'ball_bounce': {
                    const from = this.parsePosition(data['from'] as string);
                    const to = this.parsePosition(data['to'] as string);
                    if (from && to) {
                        anims.push(createBallBounceAnimation(
                            this.state,
                            from,
                            to,
                            this.gridToPixel,
                            120,
                        ));
                    }
                    break;
                }

                case 'kickoff': {
                    const kickTo = this.parsePosition(data['kickTo'] as string);
                    const landedAt = this.parsePosition(data['landedAt'] as string);
                    if (kickTo && landedAt) {
                        anims.push(createBallArcAnimation(
                            this.state,
                            { x: 13, y: 7 }, // center of pitch
                            landedAt,
                            this.gridToPixel,
                            60,
                            600,
                        ));
                    }
                    break;
                }

                case 'player_fell':
                case 'crowd_surf': {
                    const playerId = data['playerId'] as number | undefined;
                    if (playerId) {
                        const pos = this.parsePosition(data['position'] as string);
                        if (pos) {
                            anims.push(createFlashAnimation(
                                this.state,
                                pos,
                                'rgba(255, 0, 0, 1)',
                                350,
                            ));
                        }
                    }
                    break;
                }

                case 'touchdown': {
                    anims.push(createDelayAnimation(500));
                    break;
                }

                case 'turnover': {
                    anims.push(createDelayAnimation(300));
                    break;
                }

                case 'wrestle': {
                    const attackerId = data['attackerId'] as number | undefined;
                    const defenderId = data['defenderId'] as number | undefined;
                    if (attackerId) {
                        anims.push(createShakeAnimation(this.state, attackerId, 3, 200));
                    }
                    if (defenderId) {
                        anims.push(createShakeAnimation(this.state, defenderId, 3, 200));
                    }
                    break;
                }

                case 'leap': {
                    const playerId = data['playerId'] as number | undefined;
                    const success = data['success'] as boolean;
                    if (playerId) {
                        const color = success ? 'rgba(0, 200, 255, 1)' : 'rgba(255, 0, 0, 1)';
                        anims.push(createFlashAnimation(
                            this.state,
                            { x: 0, y: 0 }, // flash on player (will be looked up during render)
                            color,
                            300,
                        ));
                    }
                    break;
                }

                case 'throw_team_mate': {
                    const from = this.parsePosition(data['from'] as string);
                    const to = this.parsePosition(data['to'] as string);
                    if (from && to) {
                        anims.push(createBallArcAnimation(
                            this.state,
                            from,
                            to,
                            this.gridToPixel,
                            60,
                            600,
                        ));
                    }
                    break;
                }

                case 'tentacles': {
                    const tentPos = data['tentaclesPlayerId'] as number | undefined;
                    if (tentPos) {
                        anims.push(createShakeAnimation(this.state, tentPos, 3, 250));
                    }
                    break;
                }

                case 'bone_head':
                case 'really_stupid':
                case 'wild_animal': {
                    const playerId = data['playerId'] as number | undefined;
                    const success = data['success'] as boolean;
                    if (playerId && !success) {
                        anims.push(createShakeAnimation(this.state, playerId, 2, 300));
                    }
                    break;
                }

                case 'fend': {
                    const playerId = data['playerId'] as number | undefined;
                    if (playerId) {
                        anims.push(createShakeAnimation(this.state, playerId, 2, 200));
                    }
                    break;
                }

                case 'piling_on': {
                    const attackerId = data['attackerId'] as number | undefined;
                    if (attackerId) {
                        anims.push(createShakeAnimation(this.state, attackerId, 3, 250));
                    }
                    break;
                }

                case 'dump_off':
                case 'hail_mary_pass': {
                    anims.push(createDelayAnimation(200));
                    break;
                }

                case 'secret_weapon':
                case 'take_root': {
                    anims.push(createDelayAnimation(150));
                    break;
                }

                case 'multiple_block': {
                    // Marker event — individual block events follow
                    break;
                }
            }
        }

        return anims;
    }

    /** Parse "x,y" string into Position */
    private parsePosition(str: string | undefined): Position | null {
        if (!str || typeof str !== 'string') return null;
        const parts = str.split(',');
        if (parts.length !== 2) return null;
        const x = parseInt(parts[0], 10);
        const y = parseInt(parts[1], 10);
        if (isNaN(x) || isNaN(y)) return null;
        return { x, y };
    }
}
