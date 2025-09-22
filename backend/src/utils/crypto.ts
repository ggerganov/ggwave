import crypto from 'node:crypto';

export const deriveAuditorKey = (
  seed: Buffer,
  scopeId: number,
  epochDay: number,
  scopeType: number,
): Buffer => {
  const salt = Buffer.alloc(8);
  salt.writeUInt32LE(scopeId, 0);
  salt.writeUInt32LE(epochDay, 4);
  const info = Buffer.concat([Buffer.from('AAT-v1', 'utf-8'), Buffer.from([scopeType & 0xff])]);

  return Buffer.from(crypto.hkdfSync('sha256', seed, salt, info, 32));
};

export const hmacTruncated = (key: Buffer, payload: Buffer, tagLength: number): Buffer => {
  const full = crypto.createHmac('sha256', key).update(payload).digest();
  return full.subarray(0, tagLength);
};

export const constantTimeEqual = (a: Buffer, b: Buffer): boolean => {
  if (a.length !== b.length) {
    return false;
  }
  return crypto.timingSafeEqual(a, b);
};

export const bufferToBase64Url = (buffer: Buffer): string =>
  buffer
    .toString('base64')
    .replace(/=/g, '')
    .replace(/\+/g, '-')
    .replace(/\//g, '_');

export const hashToken = (token: Buffer): string =>
  crypto.createHash('sha256').update(token).digest('hex');
