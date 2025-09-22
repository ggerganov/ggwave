import fs from 'node:fs/promises';
import os from 'node:os';
import path from 'node:path';
import { afterAll, beforeAll, describe, expect, it, vi } from 'vitest';
import { ChallengeService } from '../services/challengeService';
import { ReplayCache } from '../services/replayCache';
import { ChallengeRateLimiter } from '../services/challengeLimiter';
import { UserRegistry } from '../services/userRegistry';
import { TokenVerifier } from '../services/tokenVerifier';
import { config } from '../config';
import { buildWebLoginToken } from './helpers/tokenFactory';

describe('TokenVerifier', () => {
  const seedB64 = 'pdtgqZMTREOrutZssKp6ugPBI9KEhLHDbVhDz2AdXWI=';
  let tmpFile: string;

  beforeAll(async () => {
    vi.useFakeTimers();
    vi.setSystemTime(new Date('2025-01-01T00:00:00Z'));
    tmpFile = path.join(os.tmpdir(), `users-${Date.now()}.json`);
    await fs.writeFile(
      tmpFile,
      JSON.stringify([
        {
          userId: 'demo-user',
          displayName: 'EchoPass Demo',
          kid: 13371337,
          scopeType: 1,
          scopeId: 1,
          seedB64,
        },
      ]),
      'utf-8',
    );
  });

  afterAll(async () => {
    vi.useRealTimers();
    await fs.rm(tmpFile, { force: true });
  });

  it('verifies a valid token', async () => {
    const challengeService = new ChallengeService(config.challengeTtlSeconds);
    const replayCache = new ReplayCache(180);
    const limiter = new ChallengeRateLimiter(10, config.challengeTtlSeconds);
    const userRegistry = new UserRegistry(tmpFile);
    await userRegistry.load();
    const verifier = new TokenVerifier(challengeService, replayCache, limiter, userRegistry);

    const challenge = challengeService.create();
    const epochDay = Math.floor(Date.now() / 1000 / 86400);
    const tslot = Math.floor(Date.now() / 1000 / config.slotSeconds);
    const token = buildWebLoginToken({
      seedB64,
      kid: 13371337,
      scopeId: 1,
      scopeType: 1,
      bind32: challenge.challengeId,
      tslot,
      epochDay,
    });
    const result = await verifier.verify(token.toString('base64'), challenge.id);
    expect(result.ok).toBe(true);
    if (result.ok) {
      expect(result.userId).toBe('demo-user');
    }
  });

  it('rejects token with incorrect length', async () => {
    const challengeService = new ChallengeService(config.challengeTtlSeconds);
    const replayCache = new ReplayCache(180);
    const limiter = new ChallengeRateLimiter(10, config.challengeTtlSeconds);
    const userRegistry = new UserRegistry(tmpFile);
    await userRegistry.load();
    const verifier = new TokenVerifier(challengeService, replayCache, limiter, userRegistry);

    const challenge = challengeService.create();
    const epochDay = Math.floor(Date.now() / 1000 / 86400);
    const tslot = Math.floor(Date.now() / 1000 / config.slotSeconds);
    const token = buildWebLoginToken({
      seedB64,
      kid: 13371337,
      scopeId: 1,
      scopeType: 1,
      bind32: challenge.challengeId,
      tslot,
      epochDay,
    });

    const truncated = token.subarray(0, token.length - 2);
    const outcome = await verifier.verify(truncated.toString('base64'), challenge.id);
    expect(outcome).toEqual({ ok: false, err: 'bad_length' });
  });

  it('rejects token with unsupported version', async () => {
    const challengeService = new ChallengeService(config.challengeTtlSeconds);
    const replayCache = new ReplayCache(180);
    const limiter = new ChallengeRateLimiter(10, config.challengeTtlSeconds);
    const userRegistry = new UserRegistry(tmpFile);
    await userRegistry.load();
    const verifier = new TokenVerifier(challengeService, replayCache, limiter, userRegistry);

    const challenge = challengeService.create();
    const epochDay = Math.floor(Date.now() / 1000 / 86400);
    const tslot = Math.floor(Date.now() / 1000 / config.slotSeconds);
    const token = buildWebLoginToken({
      seedB64,
      kid: 13371337,
      scopeId: 1,
      scopeType: 1,
      bind32: challenge.challengeId,
      tslot,
      epochDay,
    });

    const tampered = Buffer.from(token);
    tampered.writeUInt8(0xff, 0);

    const outcome = await verifier.verify(tampered.toString('base64'), challenge.id);
    expect(outcome).toEqual({ ok: false, err: 'bad_version' });
  });

  it('rejects reused nonce (replay)', async () => {
    const challengeService = new ChallengeService(config.challengeTtlSeconds);
    const replayCache = new ReplayCache(180);
    const limiter = new ChallengeRateLimiter(10, config.challengeTtlSeconds);
    const userRegistry = new UserRegistry(tmpFile);
    await userRegistry.load();
    const verifier = new TokenVerifier(challengeService, replayCache, limiter, userRegistry);

    const challenge = challengeService.create();
    const epochDay = Math.floor(Date.now() / 1000 / 86400);
    const tslot = Math.floor(Date.now() / 1000 / config.slotSeconds);
    const token = buildWebLoginToken({
      seedB64,
      kid: 13371337,
      scopeId: 1,
      scopeType: 1,
      bind32: challenge.challengeId,
      tslot,
      epochDay,
    });
    const tokenB64 = token.toString('base64');

    const first = await verifier.verify(tokenB64, challenge.id);
    expect(first.ok).toBe(true);
    const second = await verifier.verify(tokenB64, challenge.id);
    expect(second).toEqual({ ok: false, err: 'replay' });
  });
});
