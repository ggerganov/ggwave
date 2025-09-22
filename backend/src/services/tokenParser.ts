import { ParsedToken, TOKEN_HEADER_VERSION, TOKEN_TOTAL_LENGTH, TOKEN_TYPE_WEB_LOGIN } from '../types/aat';

export const parseToken = (token: Buffer): ParsedToken | null => {
  if (token.length !== TOKEN_TOTAL_LENGTH) {
    return null;
  }

  const header = token.readUInt8(0);
  const version = header >> 5;
  const type = header & 0x1f;

  if (version !== TOKEN_HEADER_VERSION || type !== TOKEN_TYPE_WEB_LOGIN) {
    return null;
  }

  const kid = token.readUInt32LE(1);
  const tslot = token.readUInt32LE(5);
  const scopeType = token.readUInt8(9);
  const scopeId = token.readUInt32LE(10);
  const nonce = token.subarray(14, 20);
  const bind32 = token.readUInt32LE(20);
  const tag = token.subarray(24, 36);
  const payload = token.subarray(0, 24);

  return {
    header,
    version,
    type,
    kid,
    tslot,
    scopeType,
    scopeId,
    nonce,
    bind32,
    tag,
    payload,
  };
};
