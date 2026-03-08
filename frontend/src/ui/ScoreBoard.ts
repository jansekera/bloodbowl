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
        const weatherIcon = this.weatherIcon(state.weather ?? 'nice');
        const team = state.activeTeam === 'home' ? state.homeTeam : state.awayTeam;
        const turnPct = Math.min(100, ((team.turnNumber - 1) / 8) * 100);

        // Action usage indicators for active team
        const actions = this.actionIndicators(team);

        this.element.innerHTML = `
            <div class="scoreboard__team ${activeClass('home')}">
                <span class="scoreboard__name">${state.homeTeam.name}</span>
                <span class="scoreboard__race">${state.homeTeam.raceName}</span>
                <span class="scoreboard__score">${state.homeTeam.score}</span>
                <span class="scoreboard__rerolls">RR: ${state.homeTeam.rerolls}${state.homeTeam.rerollUsedThisTurn ? ' <span class="scoreboard__used">(used)</span>' : ''}</span>
            </div>
            <div class="scoreboard__center">
                <div class="scoreboard__weather">${weatherIcon} <span style="color: ${weather.color}">${weather.label}</span></div>
                <div class="scoreboard__turn-row">
                    <span class="scoreboard__turn-label">Turn ${team.turnNumber}/8</span>
                    <span class="scoreboard__half">Half ${state.half}</span>
                </div>
                <div class="scoreboard__progress">
                    <div class="scoreboard__progress-bar" style="width: ${turnPct}%"></div>
                </div>
                <div class="scoreboard__phase">${state.phase.toUpperCase()}</div>
                ${actions}
            </div>
            <div class="scoreboard__team ${activeClass('away')}">
                <span class="scoreboard__name">${state.awayTeam.name}</span>
                <span class="scoreboard__race">${state.awayTeam.raceName}</span>
                <span class="scoreboard__score">${state.awayTeam.score}</span>
                <span class="scoreboard__rerolls">RR: ${state.awayTeam.rerolls}${state.awayTeam.rerollUsedThisTurn ? ' <span class="scoreboard__used">(used)</span>' : ''}</span>
            </div>
        `;
    }

    private weatherIcon(weather: string): string {
        switch (weather) {
            case 'sweltering_heat': return '&#9728;';  // ☀
            case 'very_sunny': return '&#127774;';     // 🌞
            case 'nice': return '&#9925;';             // ⛅
            case 'pouring_rain': return '&#127783;';   // 🌧
            case 'blizzard': return '&#10052;';        // ❄
            default: return '';
        }
    }

    private actionIndicators(team: GameState['homeTeam']): string {
        const items: string[] = [];
        if (team.blitzUsedThisTurn) items.push('<span class="scoreboard__action scoreboard__action--used">Blitz</span>');
        if (team.passUsedThisTurn) items.push('<span class="scoreboard__action scoreboard__action--used">Pass</span>');
        if (team.foulUsedThisTurn) items.push('<span class="scoreboard__action scoreboard__action--used">Foul</span>');
        if (items.length === 0) return '';
        return `<div class="scoreboard__actions">${items.join(' ')}</div>`;
    }

    destroy(): void {
        this.element.remove();
    }
}
