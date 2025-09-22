import { UserRecord } from '../types/aat';

export interface ProvisioningPayload {
  userId: string;
  displayName?: string;
  kid: number;
  scopeType: number;
  scopeId: number;
  seedB64: string;
}

export interface ProvisionedUser extends ProvisioningPayload {
  provisioningCode: string;
}

export const toProvisioningPayload = (user: UserRecord): ProvisioningPayload => ({
  userId: user.userId,
  displayName: user.displayName,
  kid: user.kid,
  scopeType: user.scopeType,
  scopeId: user.scopeId,
  seedB64: user.seedB64,
});

export const encodeProvisioningCode = (payload: ProvisioningPayload): string =>
  Buffer.from(JSON.stringify(payload)).toString('base64url');

export const toProvisionedUser = (user: UserRecord): ProvisionedUser => {
  const payload = toProvisioningPayload(user);
  return {
    ...payload,
    provisioningCode: encodeProvisioningCode(payload),
  };
};
