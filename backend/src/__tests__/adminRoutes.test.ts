import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import express from 'express';
import request from 'supertest';
import { afterAll, beforeAll, describe, expect, it } from 'vitest';
import { createAdminRouter } from '../routes/admin';
import { UserRegistry } from '../services/userRegistry';
import { config } from '../config';
import { TOKEN_SCOPE_SITE } from '../types/aat';

describe('admin routes', () => {
  const previousScopeId = config.scopeId;
  const previousScopeType = config.scopeType;
  let tmpDir: string;
  let usersFile: string;
  let app: express.Express;

  beforeAll(async () => {
    tmpDir = fs.mkdtempSync(path.join(os.tmpdir(), 'aat-admin-'));
    usersFile = path.join(tmpDir, 'users.json');
    config.scopeId = 4242;
    config.scopeType = TOKEN_SCOPE_SITE;
    const registry = new UserRegistry(usersFile);
    app = express();
    app.use(express.json());
    app.use('/admin', createAdminRouter(registry, config.scopeId, config.scopeType));
  });

  afterAll(() => {
    config.scopeId = previousScopeId;
    config.scopeType = previousScopeType;
    if (tmpDir && fs.existsSync(tmpDir)) {
      fs.rmSync(tmpDir, { recursive: true, force: true });
    }
  });

  it('creates a user and returns a provisioning code', async () => {
    const createRes = await request(app)
      .post('/admin/users')
      .send({ userId: 'qa-tester', displayName: 'QA Tester' })
      .expect(201);

    expect(createRes.body.ok).toBe(true);
    const user = createRes.body.user;
    expect(user.userId).toBe('qa-tester');
    expect(user.displayName).toBe('QA Tester');
    expect(user.scopeId).toBe(4242);
    expect(typeof user.kid).toBe('number');
    expect(typeof user.seedB64).toBe('string');
    expect(typeof user.provisioningCode).toBe('string');

    const decoded = JSON.parse(Buffer.from(user.provisioningCode, 'base64url').toString('utf-8'));
    expect(decoded.userId).toBe('qa-tester');
    expect(decoded.seedB64).toBe(user.seedB64);
  });

  it('lists the created users', async () => {
    const listRes = await request(app).get('/admin/users').expect(200);
    expect(Array.isArray(listRes.body.users)).toBe(true);
    expect(listRes.body.users.length).toBeGreaterThanOrEqual(1);
    expect(listRes.body.users[0]).toHaveProperty('provisioningCode');
  });

  it('rejects duplicate userIds', async () => {
    await request(app)
      .post('/admin/users')
      .send({ userId: 'qa-tester', displayName: 'Another' })
      .expect(409);
  });
});
