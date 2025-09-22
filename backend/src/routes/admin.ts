import { Router } from 'express';
import { z } from 'zod';
import { UserRegistry } from '../services/userRegistry';
import { ProvisionedUser, toProvisionedUser } from '../utils/provisioning';

const createUserSchema = z.object({
  userId: z.string().min(3).max(128),
  displayName: z.string().min(1).max(120).optional(),
});

const serializeUser = (user: ProvisionedUser) => ({
  userId: user.userId,
  displayName: user.displayName,
  kid: user.kid,
  scopeId: user.scopeId,
  scopeType: user.scopeType,
  seedB64: user.seedB64,
  provisioningCode: user.provisioningCode,
});

export const createAdminRouter = (users: UserRegistry, scopeId: number, scopeType: number) => {
  const router = Router();

  router.get('/users', async (_req, res) => {
    await users.load();
    const items = users
      .list()
      .sort((a, b) => a.userId.localeCompare(b.userId))
      .map((user) => serializeUser(toProvisionedUser(user)));
    res.json({ users: items });
  });

  router.post('/users', async (req, res) => {
    const parse = createUserSchema.safeParse(req.body);
    if (!parse.success) {
      return res.status(400).json({ ok: false, err: 'invalid_payload' });
    }

    try {
      const created = await users.createUser({
        userId: parse.data.userId,
        displayName: parse.data.displayName,
        scopeId,
        scopeType,
      });
      return res.status(201).json({
        ok: true,
        user: serializeUser(toProvisionedUser(created)),
      });
    } catch (err) {
      if (err instanceof Error && err.message === 'userId_exists') {
        return res.status(409).json({ ok: false, err: 'user_exists' });
      }
      console.error('Failed to create user', err);
      return res.status(500).json({ ok: false, err: 'internal' });
    }
  });

  return router;
};
