import { describe, it, expect } from 'vitest';
import { getWeatherDisplay } from './ScoreBoard';

describe('getWeatherDisplay', () => {
    it('returns correct label and color for each weather type', () => {
        const heat = getWeatherDisplay('sweltering_heat');
        expect(heat.label).toBe('Sweltering Heat');
        expect(heat.color).toBe('#ff6b35');

        const sunny = getWeatherDisplay('very_sunny');
        expect(sunny.label).toBe('Very Sunny');
        expect(sunny.color).toBe('#ffd700');

        const nice = getWeatherDisplay('nice');
        expect(nice.label).toBe('Nice');
        expect(nice.color).toBe('#4caf50');

        const rain = getWeatherDisplay('pouring_rain');
        expect(rain.label).toBe('Pouring Rain');
        expect(rain.color).toBe('#42a5f5');

        const blizzard = getWeatherDisplay('blizzard');
        expect(blizzard.label).toBe('Blizzard');
        expect(blizzard.color).toBe('#e0e0e0');
    });

    it('returns fallback for unknown weather', () => {
        const unknown = getWeatherDisplay('tornado');
        expect(unknown.label).toBe('tornado');
        expect(unknown.color).toBe('#ccc');
    });
});
