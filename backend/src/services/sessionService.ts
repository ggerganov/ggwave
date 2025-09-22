import crypto from 'node:crypto';

export interface Session {
  id: string;
  userId: string;
  createdAt: number;
  expiresAt: number;
}

export class SessionService {
  private readonly sessions = new Map<string, Session>();

  constructor(private readonly ttlSeconds: number) {}

  create(userId: string): Session {
    const id = crypto.randomUUID();
    const now = Date.now();
    const session: Session = {
      id,
      userId,
      createdAt: now,
      expiresAt: now + this.ttlSeconds * 1000,
    };
    this.sessions.set(id, session);
    return session;
  }

  get(id: string): Session | undefined {
    const session = this.sessions.get(id);
    if (!session) {
      return undefined;
    }
    if (session.expiresAt < Date.now()) {
      this.sessions.delete(id);
      return undefined;
    }
    return session;
  }

  destroy(id: string): void {
    this.sessions.delete(id);
  }
}
