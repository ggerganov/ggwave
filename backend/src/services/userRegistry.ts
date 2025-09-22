import fs from 'node:fs/promises';
import { dirname } from 'node:path';
import { randomBytes } from 'node:crypto';
import { z } from 'zod';
import { UserRecord } from '../types/aat';

const userSchema = z.object({
  userId: z.string(),
  displayName: z.string().optional(),
  kid: z.number().int().nonnegative(),
  scopeType: z.number().int().nonnegative(),
  scopeId: z.number().int().nonnegative(),
  seedB64: z.string(),
});

export class UserRegistry {
  private readonly usersByKid = new Map<number, UserRecord>();
  private readonly usersByUserId = new Map<string, UserRecord>();
  private loaded = false;

  constructor(private readonly filePath: string) {}

  async load(): Promise<void> {
    if (this.loaded) {
      return;
    }

    let parsed: z.infer<typeof userSchema>[] = [];
    try {
      const contents = await fs.readFile(this.filePath, 'utf-8');
      parsed = z.array(userSchema).parse(JSON.parse(contents));
    } catch (err) {
      if ((err as NodeJS.ErrnoException)?.code !== 'ENOENT') {
        throw err;
      }
      await this.persist();
    }

    this.replaceAll(parsed);

    this.loaded = true;
  }

  getByKid(kid: number): UserRecord | undefined {
    return this.usersByKid.get(kid);
  }

  getByUserId(userId: string): UserRecord | undefined {
    return this.usersByUserId.get(userId);
  }

  list(): UserRecord[] {
    return Array.from(this.usersByKid.values()).map((entry) => ({ ...entry }));
  }

  async createUser(input: {
    userId: string;
    displayName?: string;
    scopeId: number;
    scopeType: number;
  }): Promise<UserRecord> {
    await this.load();

    if (this.usersByUserId.has(input.userId)) {
      throw new Error('userId_exists');
    }

    const kid = this.generateUniqueKid();
    const seedB64 = randomBytes(32).toString('base64');
    const record: UserRecord = {
      userId: input.userId,
      displayName: input.displayName,
      kid,
      scopeId: input.scopeId,
      scopeType: input.scopeType,
      seedB64,
    };

    this.usersByKid.set(kid, record);
    this.usersByUserId.set(record.userId, record);
    await this.persist();

    return { ...record };
  }

  private replaceAll(entries: z.infer<typeof userSchema>[]): void {
    this.usersByKid.clear();
    this.usersByUserId.clear();
    for (const entry of entries) {
      const record: UserRecord = {
        userId: entry.userId,
        displayName: entry.displayName,
        kid: entry.kid,
        scopeType: entry.scopeType,
        scopeId: entry.scopeId,
        seedB64: entry.seedB64,
      };
      this.usersByKid.set(record.kid, record);
      this.usersByUserId.set(record.userId, record);
    }
  }

  private async persist(): Promise<void> {
    const entries = Array.from(this.usersByKid.values())
      .sort((a, b) => a.userId.localeCompare(b.userId))
      .map((record) => {
        const base = {
          userId: record.userId,
          kid: record.kid,
          scopeType: record.scopeType,
          scopeId: record.scopeId,
          seedB64: record.seedB64,
        };
        if (record.displayName) {
          return { ...base, displayName: record.displayName };
        }
        return base;
      });

    await fs.mkdir(dirname(this.filePath), { recursive: true });
    await fs.writeFile(this.filePath, `${JSON.stringify(entries, null, 2)}\n`);
  }

  private generateUniqueKid(): number {
    let attempt = 0;
    while (attempt < 10_000) {
      const candidate = randomBytes(4).readUInt32LE(0);
      if (!this.usersByKid.has(candidate)) {
        return candidate;
      }
      attempt += 1;
    }
    throw new Error('unable_to_allocate_kid');
  }
}
