interface Entry {
  remaining: number;
  expiresAt: number;
}

export class ChallengeRateLimiter {
  private readonly store = new Map<string, Entry>();

  constructor(private readonly maxPerWindow: number, private readonly ttlSeconds: number) {}

  tryConsume(ref: string): boolean {
    const now = Date.now();
    const entry = this.store.get(ref);

    if (!entry || entry.expiresAt < now) {
      this.store.set(ref, {
        remaining: this.maxPerWindow - 1,
        expiresAt: now + this.ttlSeconds * 1000,
      });
      return true;
    }

    if (entry.remaining <= 0) {
      return false;
    }

    entry.remaining -= 1;
    return true;
  }
}
