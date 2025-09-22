import axios from 'axios';

export interface ChallengeResponse {
  id: string;
  challenge_id: number;
  ttl: number;
}

export interface VerifyResponse {
  ok: boolean;
  userId?: string;
  err?: string;
}

export interface SessionResponse {
  ok: boolean;
  userId?: string;
  expiresAt?: number;
}

export interface AdminUser {
  userId: string;
  displayName?: string;
  kid: number;
  scopeId: number;
  scopeType: number;
  seedB64: string;
  provisioningCode: string;
}

export interface UsersListResponse {
  users: AdminUser[];
}

const api = axios.create({
  baseURL: import.meta.env.VITE_API_BASE_URL ?? 'http://localhost:4000',
  withCredentials: true,
  headers: {
    'Content-Type': 'application/json',
  },
});

export const requestChallenge = async (): Promise<ChallengeResponse> => {
  const { data } = await api.post<ChallengeResponse>('/aat/challenge');
  return data;
};

export const verifyToken = async (
  token_b64: string,
  challenge_ref: string,
): Promise<VerifyResponse> => {
  const { data } = await api.post<VerifyResponse>('/aat/verify', {
    token_b64,
    challenge_ref,
  });
  return data;
};

export const getSession = async (): Promise<SessionResponse> => {
  const { data } = await api.get<SessionResponse>('/aat/session');
  return data;
};

export const logout = async (): Promise<void> => {
  await api.post('/aat/logout');
};

export const listUsers = async (): Promise<UsersListResponse> => {
  const { data } = await api.get<UsersListResponse>('/admin/users');
  return data;
};

export const createUser = async (payload: {
  userId: string;
  displayName?: string;
}): Promise<AdminUser> => {
  try {
    const { data } = await api.post<{ ok: boolean; user: AdminUser }>('/admin/users', payload);
    if (!data.ok) {
      throw new Error('Не удалось создать пользователя');
    }
    return data.user;
  } catch (err) {
    if (axios.isAxiosError(err)) {
      if (err.response?.status === 409) {
        throw new Error('Пользователь с таким userId уже существует.');
      }
      if (err.response?.status === 400) {
        throw new Error('Проверьте корректность введенных данных.');
      }
    }
    throw new Error('Не удалось создать пользователя. Попробуйте позже.');
  }
};
