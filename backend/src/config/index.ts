import fs from 'node:fs';
import path from 'node:path';
import dotenv from 'dotenv';

dotenv.config();

const toInt = (value: string | undefined, fallback: number): number => {
  const n = Number(value);
  return Number.isFinite(n) ? n : fallback;
};

const toArray = (value: string | undefined): string[] =>
  (value ?? '')
    .split(',')
    .map((item) => item.trim())
    .filter((item) => item.length > 0);

const resolveUsersFile = (): string => {
  if (process.env.USERS_FILE) {
    return path.resolve(process.env.USERS_FILE);
  }

  const cwdPath = path.resolve(process.cwd(), 'data/users.json');
  if (fs.existsSync(cwdPath)) {
    return cwdPath;
  }

  return path.resolve(__dirname, '../data/users.json');
};

export const config = {
  port: toInt(process.env.PORT, 4000),
  host: process.env.HOST ?? '0.0.0.0',
  logLevel: process.env.LOG_LEVEL ?? 'info',
  cookieSecret: process.env.COOKIE_SECRET ?? 'change-me',
  sessionTtlSeconds: toInt(process.env.SESSION_TTL, 900),
  slotSeconds: toInt(process.env.AAT_SLOT_SECONDS, 30),
  windowSlots: toInt(process.env.AAT_WINDOW_SLOTS, 2),
  challengeTtlSeconds: toInt(process.env.CHALLENGE_TTL, 60),
  tagLength: toInt(process.env.TAG_LEN, 12),
  allowedOrigins: toArray(process.env.ALLOWED_ORIGINS),
  rateLimitPerIp: toInt(process.env.RATE_LIMIT_IP, 60),
  rateLimitPerChallenge: toInt(process.env.RATE_LIMIT_CHALLENGE, 10),
  scopeId: toInt(process.env.SCOPE_ID, 1),
  scopeType: toInt(process.env.SCOPE_TYPE, 1),
  storage: {
    usersFile: resolveUsersFile(),
  },
};

export type AppConfig = typeof config;
