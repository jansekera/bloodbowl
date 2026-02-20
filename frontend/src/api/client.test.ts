import { describe, it, expect } from 'vitest';
import { ApiClient, ApiClientError } from './client';

describe('ApiClient', () => {
    it('constructs with default base URL', () => {
        const client = new ApiClient();
        // Just verify it constructs without error
        expect(client).toBeInstanceOf(ApiClient);
    });

    it('constructs with custom base URL', () => {
        const client = new ApiClient('http://localhost:8080/api/v1');
        expect(client).toBeInstanceOf(ApiClient);
    });
});

describe('ApiClientError', () => {
    it('stores message and status code', () => {
        const error = new ApiClientError('Not found', 404);
        expect(error.message).toBe('Not found');
        expect(error.statusCode).toBe(404);
        expect(error.name).toBe('ApiClientError');
    });

    it('is an instance of Error', () => {
        const error = new ApiClientError('Server error', 500);
        expect(error).toBeInstanceOf(Error);
    });
});
