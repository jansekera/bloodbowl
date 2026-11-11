import { describe, it, expect } from 'vitest';
import { escapeHtml } from './escapeHtml';

describe('escapeHtml', () => {
    it('escapes markup and both quote kinds (safe in text and in attributes)', () => {
        expect(escapeHtml(`<b class="x">Tom & 'Jerry'</b>`)).toBe('&lt;b class=&quot;x&quot;&gt;Tom &amp; &#39;Jerry&#39;&lt;/b&gt;');
    });

    it('leaves plain text alone', () => {
        expect(escapeHtml('Grand Oak Padraig · 3 squares')).toBe('Grand Oak Padraig · 3 squares');
    });
});
