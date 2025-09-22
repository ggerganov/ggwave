import { ChallengeService } from './challengeService';
import { ReplayCache } from './replayCache';
import { ChallengeRateLimiter } from './challengeLimiter';
import { UserRegistry } from './userRegistry';
import { parseToken } from './tokenParser';
import {
  TOKEN_SCOPE_SITE,
  TOKEN_TOTAL_LENGTH,
  VerificationOutcome,
  VerificationFailure,
  VerificationSuccess,
} from '../types/aat';
import { constantTimeEqual, deriveAuditorKey, hashToken, hmacTruncated } from '../utils/crypto';
import { config } from '../config';

const SECONDS_PER_DAY = 86400;

export class TokenVerifier {
  constructor(
    private readonly challenges: ChallengeService,
    private readonly replayCache: ReplayCache,
    private readonly rateLimiter: ChallengeRateLimiter,
    private readonly users: UserRegistry,
  ) {}

  async verify(tokenB64: string, challengeRef: string): Promise<VerificationOutcome> {
    const challenge = this.challenges.get(challengeRef);
    if (!challenge) {
      return this.fail('challenge_expired');
    }

    if (!this.rateLimiter.tryConsume(challengeRef)) {
      return this.fail('challenge_expired');
    }

    let token: Buffer;
    try {
      token = Buffer.from(tokenB64, 'base64');
    } catch (err) {
      return this.fail('bad_length');
    }

    if (token.length !== TOKEN_TOTAL_LENGTH) {
      return this.fail('bad_length');
    }

    const parsed = parseToken(token);
    if (!parsed) {
      return this.fail('bad_version');
    }

    if (parsed.scopeType !== TOKEN_SCOPE_SITE) {
      return this.fail('scope_mismatch');
    }

    const user = this.users.getByKid(parsed.kid);
    if (!user) {
      return this.fail('unknown_kid');
    }

    if (user.scopeId !== parsed.scopeId || user.scopeType !== parsed.scopeType) {
      return this.fail('scope_mismatch');
    }

    if (parsed.bind32 !== challenge.challengeId) {
      return this.fail('bind32_mismatch');
    }

    const nowSeconds = Math.floor(Date.now() / 1000);
    const currentTslot = Math.floor(nowSeconds / config.slotSeconds);

    if (Math.abs(parsed.tslot - currentTslot) > config.windowSlots) {
      return this.fail('tslot_window');
    }

    const epochDay = Math.floor(nowSeconds / SECONDS_PER_DAY);
    const seed = Buffer.from(user.seedB64, 'base64');
    const payload = parsed.payload;
    const tag = parsed.tag;

    let authenticated = false;

    for (const day of [epochDay - 1, epochDay, epochDay + 1]) {
      const key = deriveAuditorKey(seed, parsed.scopeId, day, parsed.scopeType);
      const expectedTag = hmacTruncated(key, payload, config.tagLength);
      if (constantTimeEqual(expectedTag, tag)) {
        authenticated = true;
        break;
      }
    }

    if (!authenticated) {
      return this.fail('tag');
    }

    const nonceHex = parsed.nonce.toString('hex');
    if (this.replayCache.has({
      kid: parsed.kid,
      scopeId: parsed.scopeId,
      tslot: parsed.tslot,
      nonceHex,
    })) {
      return this.fail('replay');
    }

    this.replayCache.add({
      kid: parsed.kid,
      scopeId: parsed.scopeId,
      tslot: parsed.tslot,
      nonceHex,
    });

    return this.success(user.userId, token);
  }

  private fail(err: VerificationFailure['err']): VerificationFailure {
    return { ok: false, err };
  }

  private success(userId: string, token: Buffer): VerificationSuccess {
    const tokenHash = hashToken(token);
    console.info('[aat] verified token for', userId, tokenHash);
    return { ok: true, userId };
  }
}
