/**
 * Review P296 (11.11.2026): jedno escapování textu do HTML pro všechny dialogy a panely (dřív 12 soukromých kopií —
 * většina přes DOM, ta neescapovala uvozovky, takže v atributu nebyla bezpečná).
 */
const ENTITIES: Record<string, string> = { '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' };

export function escapeHtml(text: string): string {
    return text.replace(/[&<>"']/g, c => ENTITIES[c]);
}
