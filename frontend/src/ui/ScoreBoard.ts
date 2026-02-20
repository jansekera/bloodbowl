import type { GameState } from '../api/types';

const WEATHER_DISPLAY: Record<string, { label: string; color: string }> = {
    sweltering_heat: { label: 'Sweltering Heat', color: '#ff6b35' },
    very_sunny: { label: 'Very Sunny', color: '#ffd700' },
    nice: { label: 'Nice', color: '#4caf50' },
    pouring_rain: { label: 'Pouring Rain', color: '#42a5f5' },
    blizzard: { label: 'Blizzard', color: '#e0e0e0' },
};

export function getWeatherDisplay(weather: string): { label: string; color: string } {
    return WEATHER_DISPLAY[weather] ?? { label: weather, color: '#ccc' };
}

/**
 * Updates the scoreboard DOM elements with current game state.
 */
export class ScoreBoard {
    private element: HTMLElement;

    constructor(container: HTMLElement) {
        this.element = document.createElement('div');
        this.element.className = 'scoreboard';
        container.appendChild(this.element);
    }

    update(state: GameState): void {
        const activeClass = (side: 'home' | 'away') =>
            state.activeTeam === side ? 'scoreboard__team--active' : '';

        const weather = getWeatherDisplay(state.weather ?? 'nice');

        this.element.innerHTML = `
            <div class="scoreboard__team ${activeClass('home')}">
                <span class="scoreboard__name">${state.homeTeam.name}</span>
                <span class="scoreboard__race">${state.homeTeam.raceName}</span>
                <span class="scoreboard__score">${state.homeTeam.score}</span>
                <span class="scoreboard__rerolls">RR: ${state.homeTeam.rerolls}</span>
            </div>
            <div class="scoreboard__center">
                <div class="scoreboard__turn">Turn ${state.homeTeam.turnNumber} / Half ${state.half}</div>
                <div class="scoreboard__phase">${state.phase.toUpperCase()}</div>
                <div class="scoreboard__weather" style="color: ${weather.color}; font-size: 0.85em;">${weather.label}</div>
            </div>
            <div class="scoreboard__team ${activeClass('away')}">
                <span class="scoreboard__name">${state.awayTeam.name}</span>
                <span class="scoreboard__race">${state.awayTeam.raceName}</span>
                <span class="scoreboard__score">${state.awayTeam.score}</span>
                <span class="scoreboard__rerolls">RR: ${state.awayTeam.rerolls}</span>
            </div>
        `;
    }

    destroy(): void {
        this.element.remove();
    }
}
