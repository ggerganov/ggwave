import { Router } from 'express';
import { z } from 'zod';
import { ChallengeService } from '../services/challengeService';
import { TokenVerifier } from '../services/tokenVerifier';
import { SessionService } from '../services/sessionService';
import { config } from '../config';

const verifySchema = z.object({
  token_b64: z.string(),
  challenge_ref: z.string(),
});

export const COOKIE_NAME = 'aat_session';

export const createAatRouter = (
  challenges: ChallengeService,
  verifier: TokenVerifier,
  sessions: SessionService,
) => {
  const router = Router();

  router.post('/challenge', (_req, res) => {
    const challenge = challenges.create();
    res.json({
      id: challenge.id,
      challenge_id: challenge.challengeId,
      ttl: config.challengeTtlSeconds,
    });
  });

  router.post('/verify', async (req, res) => {
    const parseResult = verifySchema.safeParse(req.body);
    if (!parseResult.success) {
      return res.status(400).json({ ok: false, err: 'bad_length' });
    }

    const outcome = await verifier.verify(
      parseResult.data.token_b64,
      parseResult.data.challenge_ref,
    );

    if (!outcome.ok) {
      return res.json(outcome);
    }

    const session = sessions.create(outcome.userId);
    res.cookie(COOKIE_NAME, session.id, {
      httpOnly: true,
      secure: process.env.NODE_ENV === 'production',
      sameSite: 'lax',
      maxAge: config.sessionTtlSeconds * 1000,
    });

    return res.json(outcome);
  });

  router.get('/session', (req, res) => {
    const sessionId = req.cookies?.[COOKIE_NAME];
    if (!sessionId) {
      return res.json({ ok: false });
    }

    const session = sessions.get(sessionId);
    if (!session) {
      return res.json({ ok: false });
    }

    return res.json({ ok: true, userId: session.userId, expiresAt: session.expiresAt });
  });

  router.post('/logout', (req, res) => {
    const sessionId = req.cookies?.[COOKIE_NAME];
    if (sessionId) {
      sessions.destroy(sessionId);
      res.clearCookie(COOKIE_NAME);
    }
    res.json({ ok: true });
  });

  return router;
};
