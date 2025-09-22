interface ReplayKey {
  kid: number;
  scopeId: number;
  tslot: number;
  nonceHex: string;
}

const keyToString = (key: ReplayKey) =>
  `${key.kid}:${key.scopeId}:${key.tslot}:${key.nonceHex}`;

export class ReplayCache {
  private readonly store = new Map<string, number>();

  constructor(private readonly ttlSeconds: number) {}

  has(key: ReplayKey): boolean {
    this.prune();
    return this.store.has(keyToString(key));
  }

  add(key: ReplayKey): void {
    const expiresAt = Date.now() + this.ttlSeconds * 1000;
    this.store.set(keyToString(key), expiresAt);
  }

  private prune(): void {
    const now = Date.now();
    for (const [key, expiresAt] of this.store.entries()) {
      if (expiresAt < now) {
        this.store.delete(key);
      }
    }
  }
}
