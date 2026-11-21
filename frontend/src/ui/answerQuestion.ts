/**
 * Odpověď kouče na otázku serveru (kostka bloku, Stand Firm, Grab, Diving Catch, rozhodčí, bomba). Review P308 (7):
 * dialog se znovu ukáže (`onRejected`) jen když server odpověď nepřijal; chyba až při zpracování přijatého výsledku
 * se nechytá — otázka na serveru už nečeká.
 */
export async function answerQuestion<T>(
    submit: () => Promise<T>,
    onAccepted: (result: T) => Promise<void>,
    onRejected: (err: unknown) => void,
): Promise<void> {
    let result: T;
    try {
        result = await submit();
    } catch (err) {
        onRejected(err);
        return;
    }
    await onAccepted(result);
}
