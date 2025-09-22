import { config } from '../../config';
import { deriveAuditorKey, hmacTruncated } from '../../utils/crypto';

export interface TokenFactoryParams {
  seedB64: string;
  kid: number;
  scopeId: number;
  scopeType: number;
  bind32: number;
  tslot: number;
  epochDay: number;
  tagLength?: number;
}

export const buildWebLoginToken = ({
  seedB64,
  kid,
  scopeId,
  scopeType,
  bind32,
  tslot,
  epochDay,
  tagLength = config.tagLength,
}: TokenFactoryParams): Buffer => {
  const buffer = Buffer.alloc(36);
  const header = (1 << 5) | 1;
  buffer.writeUInt8(header, 0);
  buffer.writeUInt32LE(kid, 1);
  buffer.writeUInt32LE(tslot, 5);
  buffer.writeUInt8(scopeType, 9);
  buffer.writeUInt32LE(scopeId, 10);
  const nonce = Buffer.from('112233445566', 'hex');
  nonce.copy(buffer, 14);
  buffer.writeUInt32LE(bind32, 20);

  const payload = buffer.subarray(0, 24);
  const seed = Buffer.from(seedB64, 'base64');
  const key = deriveAuditorKey(seed, scopeId, epochDay, scopeType);
  const tag = hmacTruncated(key, payload, tagLength);
  tag.copy(buffer, 24);

  return buffer;
};
