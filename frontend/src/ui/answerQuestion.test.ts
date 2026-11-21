import { describe, it, expect, vi } from 'vitest';
import { answerQuestion } from './answerQuestion';

// Review P308 (7): dialog otázky kouči se znovu ukáže, jen když server odpověď nepřijal. Chyba až při zpracování
// výsledku (animace, další dialog) znamená, že otázka na serveru už nečeká — znovu ji ukázat by bylo špatně.
describe('answerQuestion', () => {
    it('re-shows the question when the server rejects the answer', async () => {
        const accepted = vi.fn();
        const rejected = vi.fn();
        await answerQuestion(() => Promise.reject(new Error('422')), accepted, rejected);
        expect(rejected).toHaveBeenCalledOnce();
        expect(accepted).not.toHaveBeenCalled();
    });

    it('does not re-show the question when handling an accepted answer fails', async () => {
        const rejected = vi.fn();
        const failing = vi.fn(() => Promise.reject(new Error('render')));
        await expect(answerQuestion(() => Promise.resolve('ok'), failing, rejected)).rejects.toThrow('render');
        expect(failing).toHaveBeenCalledWith('ok');
        expect(rejected).not.toHaveBeenCalled();
    });
});
