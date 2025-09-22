import crypto from 'node:crypto';
import { Challenge } from '../types/aat';

export class ChallengeService {
  private readonly challenges = new Map<string, Challenge>();

  constructor(private readonly ttlSeconds: number) {}

  create(): Challenge {
    const id = crypto.randomUUID();
    const challengeId = crypto.randomInt(0, 0xffffffff);
    const expiresAt = Date.now() + this.ttlSeconds * 1000;
    const challenge: Challenge = { id, challengeId, expiresAt };

    this.challenges.set(id, challenge);
    setTimeout(() => {
      this.challenges.delete(id);
    }, this.ttlSeconds * 1000).unref?.();

    return challenge;
  }

  get(id: string): Challenge | undefined {
    const entry = this.challenges.get(id);
    if (!entry) {
      return undefined;
    }
    if (entry.expiresAt < Date.now()) {
      this.challenges.delete(id);
      return undefined;
    }
    return entry;
  }
}
