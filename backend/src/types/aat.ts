export const TOKEN_TOTAL_LENGTH = 36;
export const TOKEN_HEADER_VERSION = 1;
export const TOKEN_TYPE_WEB_LOGIN = 1;
export const TOKEN_SCOPE_SITE = 1;

export type VerifyError =
  | 'bad_length'
  | 'bad_version'
  | 'unknown_kid'
  | 'scope_mismatch'
  | 'tag'
  | 'tslot_window'
  | 'bind32_mismatch'
  | 'replay'
  | 'challenge_expired';

export interface ParsedToken {
  header: number;
  version: number;
  type: number;
  kid: number;
  tslot: number;
  scopeType: number;
  scopeId: number;
  nonce: Buffer;
  bind32: number;
  tag: Buffer;
  payload: Buffer;
}

export interface Challenge {
  id: string;
  challengeId: number;
  expiresAt: number;
}

export interface UserRecord {
  userId: string;
  displayName?: string;
  kid: number;
  scopeType: number;
  scopeId: number;
  seedB64: string;
}

export interface VerificationSuccess {
  ok: true;
  userId: string;
}

export interface VerificationFailure {
  ok: false;
  err: VerifyError;
}

export type VerificationOutcome = VerificationSuccess | VerificationFailure;
