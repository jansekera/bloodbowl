/**
 * Full-screen phase banner (half-time, touchdown, game over).
 */
export class PhaseBanner {
    private banner: HTMLElement;
    private timeout: ReturnType<typeof setTimeout> | null = null;

    constructor(private container: HTMLElement) {
        this.banner = document.createElement('div');
        this.banner.className = 'phase-banner';
        this.banner.style.display = 'none';
        this.container.appendChild(this.banner);
    }

    show(text: string, duration = 2000): Promise<void> {
        this.banner.textContent = text;
        this.banner.style.display = 'flex';

        return new Promise((resolve) => {
            this.timeout = setTimeout(() => {
                this.banner.style.display = 'none';
                this.timeout = null;
                resolve();
            }, duration);
        });
    }

    hide(): void {
        if (this.timeout) {
            clearTimeout(this.timeout);
            this.timeout = null;
        }
        this.banner.style.display = 'none';
    }

    destroy(): void {
        this.hide();
        this.banner.remove();
    }
}
