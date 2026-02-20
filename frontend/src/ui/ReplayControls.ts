import type { GameEvent } from '../api/types';

export type ReplayCallback = (action: 'prev' | 'next' | 'play' | 'pause' | 'speed') => void;

export class ReplayControls {
    private container: HTMLElement;
    private events: GameEvent[] = [];
    private currentIndex: number = -1;
    private isPlaying: boolean = false;
    private speed: number = 1;
    private playTimer: ReturnType<typeof setInterval> | null = null;
    private onStep: ((event: GameEvent) => void) | null = null;

    constructor(container: HTMLElement) {
        this.container = container;
        this.container.classList.add('replay-controls');
    }

    setEvents(events: GameEvent[]): void {
        this.events = events;
        this.currentIndex = -1;
        this.isPlaying = false;
        this.render();
    }

    setOnStep(callback: (event: GameEvent) => void): void {
        this.onStep = callback;
    }

    getEventCount(): number {
        return this.events.length;
    }

    getCurrentIndex(): number {
        return this.currentIndex;
    }

    isActive(): boolean {
        return this.isPlaying;
    }

    next(): GameEvent | null {
        if (this.currentIndex >= this.events.length - 1) {
            this.pause();
            return null;
        }
        this.currentIndex++;
        const event = this.events[this.currentIndex];
        this.onStep?.(event);
        this.render();
        return event;
    }

    prev(): GameEvent | null {
        if (this.currentIndex <= 0) return null;
        this.currentIndex--;
        const event = this.events[this.currentIndex];
        this.render();
        return event;
    }

    play(): void {
        if (this.isPlaying) return;
        this.isPlaying = true;
        this.playTimer = setInterval(() => {
            if (!this.next()) {
                this.pause();
            }
        }, 1000 / this.speed);
        this.render();
    }

    pause(): void {
        this.isPlaying = false;
        if (this.playTimer) {
            clearInterval(this.playTimer);
            this.playTimer = null;
        }
        this.render();
    }

    cycleSpeed(): void {
        if (this.speed === 1) this.speed = 2;
        else if (this.speed === 2) this.speed = 4;
        else this.speed = 1;

        // Restart timer if playing
        if (this.isPlaying) {
            this.pause();
            this.play();
        } else {
            this.render();
        }
    }

    render(): void {
        const total = this.events.length;
        const current = this.currentIndex + 1;
        const desc = this.currentIndex >= 0 && this.currentIndex < this.events.length
            ? this.events[this.currentIndex].description
            : '';

        this.container.innerHTML = `
            <div class="replay-controls__bar">
                <button class="btn btn--small" data-replay="prev" title="Previous event">&#9198;</button>
                <button class="btn btn--small" data-replay="${this.isPlaying ? 'pause' : 'play'}" title="${this.isPlaying ? 'Pause' : 'Play'}">
                    ${this.isPlaying ? '&#9208;' : '&#9654;'}
                </button>
                <button class="btn btn--small" data-replay="next" title="Next event">&#9197;</button>
                <button class="btn btn--small" data-replay="speed" title="Change speed">${this.speed}×</button>
                <span class="replay-controls__counter">Event ${current} / ${total}</span>
            </div>
            ${desc ? `<div class="replay-controls__desc">${desc}</div>` : ''}
        `;

        this.container.querySelectorAll('[data-replay]').forEach(btn => {
            btn.addEventListener('click', () => {
                const action = (btn as HTMLElement).dataset.replay;
                switch (action) {
                    case 'prev': this.prev(); break;
                    case 'next': this.next(); break;
                    case 'play': this.play(); break;
                    case 'pause': this.pause(); break;
                    case 'speed': this.cycleSpeed(); break;
                }
            });
        });
    }

    destroy(): void {
        this.pause();
        this.container.innerHTML = '';
    }
}
