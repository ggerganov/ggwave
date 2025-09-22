import path from 'node:path';
import request from 'supertest';
import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest';
import { createApp } from '../index';
import { config } from '../config';
import { buildWebLoginToken } from './helpers/tokenFactory';

const usersFixture = path.resolve(__dirname, '../../data/users.json');
config.storage.usersFile = usersFixture;

describe('AAT routes', () => {
  const seedB64 = 'pdtgqZMTREOrutZssKp6ugPBI9KEhLHDbVhDz2AdXWI=';

  beforeEach(() => {
    vi.useFakeTimers();
    vi.setSystemTime(new Date('2025-01-01T00:00:00Z'));
  });

  afterEach(() => {
    vi.useRealTimers();
  });

  it('creates a challenge and verifies a token end-to-end', async () => {
    const app = await createApp();
    const agent = request(app);

    const challengeRes = await agent.post('/aat/challenge').expect(200);
    expect(challengeRes.body).toMatchObject({ ttl: config.challengeTtlSeconds });

    const { id, challenge_id: challengeId } = challengeRes.body;
    expect(typeof id).toBe('string');
    expect(typeof challengeId).toBe('number');

    const epochDay = Math.floor(Date.now() / 1000 / 86400);
    const tslot = Math.floor(Date.now() / 1000 / config.slotSeconds);
    const token = buildWebLoginToken({
      seedB64,
      kid: 13371337,
      scopeId: config.scopeId,
      scopeType: 1,
      bind32: challengeId,
      tslot,
      epochDay,
    }).toString('base64');

    const verifyRes = await agent
      .post('/aat/verify')
      .send({ token_b64: token, challenge_ref: id })
      .expect(200);

    expect(verifyRes.body).toEqual({ ok: true, userId: 'demo-user' });
    const cookies = verifyRes.headers['set-cookie'];
    expect(Array.isArray(cookies)).toBe(true);
    expect(cookies?.[0]).toContain('aat_session=');
  });
});
