export type ToastLevel = 'info' | 'error' | 'success';

export interface ToastOptions {
    message: string;
    level?: ToastLevel;
    duration?: number;
}

/**
 * Toast notification system with auto-dismiss.
 */
export class Toast {
    private container: HTMLElement;

    constructor(parent: HTMLElement) {
        this.container = document.createElement('div');
        this.container.className = 'toast-container';
        parent.appendChild(this.container);
    }

    show(options: ToastOptions): void {
        const { message, level = 'info', duration = 3000 } = options;

        const toast = document.createElement('div');
        toast.className = `toast toast--${level}`;
        toast.textContent = message;
        this.container.appendChild(toast);

        // Auto-dismiss
        setTimeout(() => {
            toast.classList.add('toast--fading');
            setTimeout(() => toast.remove(), 300);
        }, duration);
    }

    info(message: string): void {
        this.show({ message, level: 'info' });
    }

    error(message: string): void {
        this.show({ message, level: 'error' });
    }

    success(message: string): void {
        this.show({ message, level: 'success' });
    }

    destroy(): void {
        this.container.remove();
    }
}

/** Map of level to CSS class for external use */
export const TOAST_LEVELS: Record<ToastLevel, string> = {
    info: 'toast--info',
    error: 'toast--error',
    success: 'toast--success',
};
