import fs from 'node:fs';
import path from 'node:path';
import express from 'express';
import cors from 'cors';
import helmet from 'helmet';
import cookieParser from 'cookie-parser';
import morgan from 'morgan';
import rateLimit from 'express-rate-limit';
import { config } from './config';
import { ChallengeService } from './services/challengeService';
import { ReplayCache } from './services/replayCache';
import { ChallengeRateLimiter } from './services/challengeLimiter';
import { UserRegistry } from './services/userRegistry';
import { TokenVerifier } from './services/tokenVerifier';
import { SessionService } from './services/sessionService';
import { createAatRouter } from './routes/aat';
import { createAdminRouter } from './routes/admin';

const corsOptions: cors.CorsOptions = {
  origin: (origin, callback) => {
    if (!origin) {
      return callback(null, true);
    }
    if (config.allowedOrigins.length === 0 || config.allowedOrigins.includes(origin)) {
      return callback(null, true);
    }
    return callback(new Error('Origin not allowed by CORS'));
  },
  credentials: true,
};

const createApp = async () => {
  const app = express();

  const ipLimiter = rateLimit({
    windowMs: 60 * 1000,
    max: config.rateLimitPerIp,
    standardHeaders: true,
    legacyHeaders: false,
  });

  app.use(ipLimiter);
  app.use(cors(corsOptions));
  app.use(helmet());
  app.use(express.json({ limit: '1mb' }));
  app.use(cookieParser(config.cookieSecret));
  app.use(morgan('combined'));

  const challengeService = new ChallengeService(config.challengeTtlSeconds);
  const antiReplayTtl = Math.max(120, config.slotSeconds * (config.windowSlots * 2 + 2));
  const replayCache = new ReplayCache(antiReplayTtl);
  const challengeLimiter = new ChallengeRateLimiter(
    config.rateLimitPerChallenge,
    config.challengeTtlSeconds,
  );
  const userRegistry = new UserRegistry(config.storage.usersFile);
  await userRegistry.load();
  const tokenVerifier = new TokenVerifier(
    challengeService,
    replayCache,
    challengeLimiter,
    userRegistry,
  );
  const sessionService = new SessionService(config.sessionTtlSeconds);

  app.get('/health', (_req, res) => {
    res.json({ status: 'ok', time: Date.now() });
  });

  app.use('/aat', createAatRouter(challengeService, tokenVerifier, sessionService));
  app.use('/admin', createAdminRouter(userRegistry, config.scopeId, config.scopeType));

  const staticDir = path.resolve(__dirname, '../public');
  if (fs.existsSync(staticDir)) {
    app.use(express.static(staticDir));
    app.get('*', (_req, res) => {
      res.sendFile(path.join(staticDir, 'index.html'));
    });
  }

  return app;
};

const start = async () => {
  const app = await createApp();
  app.listen(config.port, config.host, () => {
    console.log(`EchoPass backend listening on http://${config.host}:${config.port}`);
  });
};

if (require.main === module) {
  start().catch((err) => {
    console.error('Failed to start server', err);
    process.exit(1);
  });
}

export { createApp };
